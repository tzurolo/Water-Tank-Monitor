//var querystring = require('querystring'); // only need this for url-encoded CSV format
var http = require('http');
var net = require('net');
var readline = require('readline');

var HOST = 'localhost';
var SENSOR_PORT = 3000;
var ThingSpeakSensorWritekey = "DD7TVSCZEHKZLAQP";
var DISPLAY_PORT = 3010;
var ThingSpeakPumpWritekey = "P55J3PTLW77TJFLB";

var mainsock;
var pendingCommand = '';

var tankEmptyDistance = 300;
var tankFullDistance = 30;

var latestValidWaterDistance = { "distance" : -1, "timestamp" : 0};  // -1 means unknown
var latestWaterLevel = { "level" : -1, "timestamp" : 0};  // -1 means unknown

var LoJASensorUnitId = 2;
var LoJADisplayUnitId = 3;

var gpsStart = Date.UTC(1980, 0, 6);
function gpsTime (date)
{
    return Math.round((date.getTime() - gpsStart) / 1000);
}

function waterLevelFromDistance (distance)
{
    return Math.round(
        ((tankEmptyDistance - distance) * 100) /
        (tankEmptyDistance - tankFullDistance));
}

var maxFillRate = 0.0175;    // cm per second
var maxDrainRate = 0.0175;   // cm per second
function computeDistanceBounds (sampleTimestamp, latestValidDistance)
{
	// default: full range
	var bounds = { "low" : tankFullDistance,
				  "high" : tankEmptyDistance};
	if (latestValidDistance.distance >= 0) {
		var timeInterval = sampleTimestamp - latestValidDistance.timestamp;  // seconds
		var latestDistance = latestValidDistance.distance;

		bounds.low = latestDistance - maxFillRate * timeInterval;
		if (bounds.low < tankFullDistance) bounds.low = tankFullDistance;

	    bounds.high = latestDistance + maxDrainRate * timeInterval;
		if (bounds.high > tankEmptyDistance) bounds.high = tankEmptyDistance;
	}

	return bounds;
}

function isWithinBounds (value, bounds)
{
	return (value >= bounds.low) && (value <= bounds.high);
}

//
//  HTTP post to ThingSpeak
//
function postBulkDataToThingSpeak(bulkData) {
    // Build the post string from the given data object
    var postData = {
       "write_api_key" : ThingSpeakSensorWritekey,
       "time_format" : "relative",
       "updates" : bulkData
    };
    var postDataStr = JSON.stringify(postData);
    
    // An object of options to indicate where to post to
    var post_options = {
        host: 'api.thingspeak.com',
        port: '80',
        path: '/channels/8203/bulk_update.json',
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Content-Length': Buffer.byteLength(postDataStr)
        }
    };

  // Set up the request
  var post_req = http.request(post_options, function(res) {
      res.setEncoding('utf8');
      res.on('data', function (chunk) {
          console.log('Response: ' + chunk);
      });
  });
  
  // post the data
  console.log("Posting: " + postDataStr);
  post_req.write(postDataStr);
  post_req.end();
}

function postDataToThingSpeak(data) {
    // Build the post string from the given data object
	var fieldDataStr = '';
	for (var field in data){
		if (fieldDataStr.length > 0) {
			fieldDataStr += ',';
		}
        fieldDataStr += '&' + field + '=' + data[field];
    }
    var postDataStr = 'api_key=' + ThingSpeakPumpWritekey + fieldDataStr;
	
    // An object of options to indicate where to post to
    var post_options = {
        host: 'api.thingspeak.com',
        port: '80',
        path: '/update',
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
            'Cache-Control': 'no-cache'
        }
    };

  // Set up the request
  var post_req = http.request(post_options, function(res) {
      res.setEncoding('utf8');
      res.on('data', function (chunk) {
          console.log('Response: ' + chunk);
      });
  });
  
  // post the data
  console.log("Posting: " + postDataStr);
  post_req.write(postDataStr);
  post_req.end();
}

//POST /update.json HTTP/1.1
//Host: api.thingspeak.com
//Content-Type: application/x-www-form-urlencoded
//Cache-Control: no-cache
//Postman-Token: ae8df110-e894-4f64-a0a6-17a374a14a2b
//
//api_key=P55J3PTLW77TJFLB&field1=1

//
//  End of HTTP post to ThingSpeak
//

//
// parse sensor data and send to ThingSpeak
//

var sensorFieldDescriptors = {
   "D" : {fieldName : "delta_t",  divisor : 1   },
   "W" : {fieldName : "field1",   divisor : 10  },
   "T" : {fieldName : "field2",   divisor : 1   },
   "B" : {fieldName : "field3",   divisor : 100 },
   "Q" : {fieldName : "field4",   divisor : 1   },
   "C" : {fieldName : "field5",   divisor : 1   },
   "I" : {fieldName : "id",       divisor : 1   }
   };

// sets a field of sample from the given fieldStr. fieldStr
// is expected to be an uppercase letter followed by a number
function parseField(fieldStr, fieldDescriptors, sample)  {
    var key = fieldStr.substring(0,1);
    var value = parseFloat(fieldStr.substring(1));
    if (key in fieldDescriptors) {
        var fieldDesc = fieldDescriptors[key];
        sample[fieldDesc.fieldName] = value / fieldDesc.divisor;
    }
}

function parseSensorDataFeed(sensorDataStr) {
    var packetStrs = sensorDataStr.match(/[0-9A-Z\-]+;/g);
    var connDataStr = packetStrs[0];
    // parse sample data
    var fieldRE = new RegExp("\\w-?\\d+", "g");
    var samples = [];
    for (i = 1; i < packetStrs.length; ++i) {
        var sampleStr = packetStrs[i];
        var sample = {"delta_t" : 0};
        var sampleFields = sampleStr.match(fieldRE);
        for (f in sampleFields) {
           parseField(sampleFields[f], sensorFieldDescriptors, sample);
        }
        samples.push(sample);
    }

	// set timestamps on samples
	var sampleTime = gpsTime(new Date());
	for (i = samples.length - 1; i >= 0; --i) {
		samples[i].abs_t = sampleTime;
		sampleTime -= samples[i].delta_t;
	}

	// filter samples with simple Kalman-like filter.
	// for good samples compute water level from distance to water.
	for (s in samples) {
		// filter sample
		var distance = samples[s].field1;
		var sampleTime = samples[s].abs_t;
		var bounds = computeDistanceBounds(sampleTime, latestValidWaterDistance);
		//console.log("Distance: " + distance + ", bounds: " + bounds.low + ".." + bounds.high);
		if (isWithinBounds(distance, bounds)) {
			// valid sample
			// update filter state
			latestValidWaterDistance.distance = distance;
			latestValidWaterDistance.timestamp = sampleTime;
			
			// compute and capture water level for display
			waterLevel = waterLevelFromDistance(distance);
			latestWaterLevel.level = waterLevel;
			latestWaterLevel.timestamp = samples[s].abs_t;
			
			samples[s].field6 = waterLevel;
			
			// clear out abs_t - it's only needed for this filter
			delete samples[s].abs_t;
		} else {
			console.log('>>> rejecting sample distance ' + distance + ", bounds: " + bounds.low + ".." + bounds.high);
		}
	}
	
    // set connection data on last sample
    var lastSample = samples[samples.length-1];
    var connFields = connDataStr.match(fieldRE);
    for (x in connFields) {
        parseField(connFields[x], sensorFieldDescriptors, lastSample);
    }
    
    if (lastSample.id == LoJASensorUnitId) {
        // send to ThingSpeak
		delete lastSample.id; // we don't post the id
        postBulkDataToThingSpeak(samples);
    } else {
        console.log('got data from alternate sensor ' + lastSample.id);
    }
}
//
// end of parse sensor data and send to ThingSpeak
//

//
//  water level monitor net server
//
var waterLevelMonitorServer = net.createServer(function(sock) {
    // Send current time to monitor module
    // The format is seconds since start of GPS epoch
    var now = new Date();
    console.log('CONNECTED ' + now.toDateString() + " " + now.toLocaleTimeString() + ': ' +
        sock.remoteAddress +':'+ sock.remotePort);

    sock["incomingData"] = '';
    sock["incomingDataLineNumber"] = 0;
    mainsock = sock;
    // Add a 'data' event handler to this instance of socket
    sock.on('data', function(data) {
        var dataString = data.toString('utf8');
        var now = new Date();
        console.log('DATA ' + now.toDateString() + " " + now.toLocaleTimeString() + ': ' +
            dataString + ', line: ' + sock.incomingDataLineNumber);
        for (chIndex in dataString) {
            if (dataString.charCodeAt(chIndex) == 10) {
                // end of line
                sock.incomingDataLineNumber++;
                if (sock.incomingDataLineNumber == 1) {
                    // first line is sensor data
                    // validate
                    if ((sock.incomingData.charAt(0) == 'I') && 
                        (sock.incomingData.charAt(sock.incomingData.length-1) == 'Z')) {
                        // parse the data
                        parseSensorDataFeed(sock.incomingData);
                    } else {
                        console.log('>>>> unexpected input - destroy socket');
                        sock.destroy('unexpected');
                        break;
                    }
                }
                sock.incomingData = '';
            } else {
                sock.incomingData += dataString.charAt(chIndex);
            }
        }                
    });
    
    // Add a 'close' event handler to this instance of socket
    sock.on('close', function(data) {
        var now = new Date();
        console.log('CLOSED ' + now.toDateString() + " " + now.toLocaleTimeString() + ': '+
            sock.remotePort);
        mainsock = undefined;
        pendingCommand = '';
    });

    sock.on('error', function(exception) {
        var now = new Date();
        console.log('ERROR ' + now.toDateString() + " " + now.toLocaleTimeString() + ': ' +
            exception);
        pendingCommand = '';
    });

    var setTimeCmd = 'tset ' + gpsTime(now);
    if (pendingCommand.length > 0) {
        console.log('sending block start');
        sock.write('[\r');
        pendingCommand = '';
    }
    sock.write(setTimeCmd + '\r');
});
//
//  end of water level monitor net server
//

//
// parse sensor data and send to ThingSpeak
//

var pumpFieldDescriptors = {
   "M" : {fieldName : "field2",   divisor : 1    },
   "P" : {fieldName : "field1",   divisor : 1    },
   "T" : {fieldName : "field3",   divisor : 1    },
   "B" : {fieldName : "field4",   divisor : 1000 },
   "Q" : {fieldName : "field5",   divisor : 1    },
   "C" : {fieldName : "field6",   divisor : 1    },
   "I" : {fieldName : "id",       divisor : 1    }
   };

function parsePumpDataFeed(pumpDataStr) {
    var fieldRE = new RegExp("\\w-?\\d+", "g");
    var pumpData = {};
    var pumpDataFields = pumpDataStr.match(fieldRE);
    for (x in pumpDataFields) {
        parseField(pumpDataFields[x], pumpFieldDescriptors, pumpData);
    }
    
    if (pumpData.id == LoJADisplayUnitId) {
        delete pumpData.id; // we don't post the id
        // send to ThingSpeak
        postDataToThingSpeak(pumpData);
    } else {
        console.log('got data from alternate display ' + pumpData.id);
    }
}
//
// end of parse pump data and send to ThingSpeak
//

//
//  water level display net server
//
var waterLevelDisplayServer = net.createServer(function(sock) {
    // Send current time to display module
    // The format is seconds since start of GPS epoch
    var now = new Date();
    console.log('display CONNECTED ' + now.toDateString() + " " + now.toLocaleTimeString() + ': ' +
        sock.remoteAddress +':'+ sock.remotePort);

    sock["incomingData"] = '';
    sock["incomingDataLineNumber"] = 0;
    mainsock = sock;
    // Add a 'data' event handler to this instance of socket
    sock.on('data', function(data) {
        var dataString = data.toString('utf8');
        var now = new Date();
        console.log('display DATA ' + now.toDateString() + " " + now.toLocaleTimeString() + ': ' +
            dataString + ', line: ' + sock.incomingDataLineNumber);
        for (chIndex in dataString) {
            if (dataString.charCodeAt(chIndex) == 10) {
                // end of line
                sock.incomingDataLineNumber++;
                if (sock.incomingDataLineNumber == 1) {
                    // first line is sensor data
                    // validate
                    if ((sock.incomingData.charAt(0) == 'I') && 
                        (sock.incomingData.charAt(sock.incomingData.length-1) == 'Z')) {
                        // parse the data
                        parsePumpDataFeed(sock.incomingData);
                    } else {
                        console.log('>>>> unexpected input - destroy socket');
                        sock.destroy('unexpected');
                        break;
                    }
                }
                sock.incomingData = '';
            } else {
                sock.incomingData += dataString.charAt(chIndex);
            }
        }                
    });
    
    // Add a 'close' event handler to this instance of socket
    sock.on('close', function(data) {
        var now = new Date();
        console.log('display CLOSED ' + now.toDateString() + " " + now.toLocaleTimeString() + ': '+
            sock.remotePort);
        mainsock = undefined;
        pendingCommand = '';
    });

    sock.on('error', function(exception) {
        var now = new Date();
        console.log('display ERROR ' + now.toDateString() + " " + now.toLocaleTimeString() + ': ' +
            exception);
        pendingCommand = '';
    });
    
    var dataCmd = 'data ' + latestWaterLevel.level + ' ' + latestWaterLevel.timestamp + ' ' + gpsTime(now);
    console.log(dataCmd);
    if (pendingCommand.length > 0) {
        console.log('sending block start');
        sock.write('[\r');
        pendingCommand = '';
    }
    sock.write(dataCmd + '\r');
});
//
//  end of net server
//

//
// Command line interface
//
var rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout,
  terminal: false
});

rl.on('line', function(line){
    if (mainsock) {
        console.log('sending command: ' + line);
        mainsock.write(line + '\r');
    } else {
        console.log('buffering pending command: ' + line);
        pendingCommand = line;
    }
});
//
// End of Command line interface
//

//
//  read latest water level from ThingSpeak upon startup
//  of this server
//
function readLatestWaterLevelFromThingSpeak() {
    // An object of options to indicate where to get from
    var get_options = {
        host: 'api.thingspeak.com',
        port: '80',
        path: '/channels/8203/feeds.json?results=100',
        method: 'GET',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
            'Cache-Control': 'no-cache'
        }
    };
  var responseData = '';
  // Set up the request
  var get_req = http.request(get_options, function(res) {
      res.setEncoding('utf8');
      res.on('data', function (chunk) {
          responseData += chunk;
      });
      res.on('end', function () {
         // console.log('Response: ' + responseData);
         responseObj = JSON.parse(responseData);

		// search backwards for the most recent non-null water level field
		var searchIndex = responseObj.feeds.length - 1;
		while ((searchIndex >= 0) &&
		       (responseObj.feeds[searchIndex].field6 == null)) {
			--searchIndex;
		}
		  
		if (searchIndex >= 0) {
			// found non-null water level field
			var waterLevelData = responseObj.feeds[searchIndex];

			// set latest water level
            latestWaterLevel.level = parseInt(waterLevelData.field6);
            console.log('   Water Level: ' + latestWaterLevel.level);
            var latestWaterLevelDate = new Date(waterLevelData.created_at);
            latestWaterLevel.timestamp = gpsTime(latestWaterLevelDate);

			// set latest valid water distance
			latestValidWaterDistance.distance = parseFloat(waterLevelData.field1);
            console.log('Water Distance: ' + latestValidWaterDistance.distance);
			latestValidWaterDistance.timestamp = latestWaterLevel.timestamp;
            console.log('         as of: ' + latestWaterLevelDate);
		} else {
			// did not find water level field
			latestWaterLevel.level = -1;
			latestWaterLevel.timestamp = 0;
		}
      });
  });
  
  // get the data
  console.log("Getting latest data from ThingSpeak");
  get_req.end();
}
readLatestWaterLevelFromThingSpeak();

waterLevelMonitorServer.listen(SENSOR_PORT, function() {
  console.log('water level monitor server bound on port ' + SENSOR_PORT);
});
waterLevelDisplayServer.listen(DISPLAY_PORT, function() {
  console.log('water level display server bound on port ' + DISPLAY_PORT);
});
