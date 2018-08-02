//var querystring = require('querystring'); // only need this for url-encoded CSV format
var http = require('http');
var net = require('net');
var readline = require('readline');

var HOST = 'localhost';
var PORT = 3000;
var ThingSpeakSensorWritekey = "DD7TVSCZEHKZLAQP";
var ThingSpeakPumpWritekey = "P55J3PTLW77TJFLB";

var mainsock;
var pendingCommand = '';

var tankEmptyDistance = 300;
var tankFullDistance = 30;
var latestWaterLevel = -1;  // -1 means unknown
var latestWaterLevelTimestamp = 0;

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
        for (x in sampleFields) {
           parseField(sampleFields[x], sensorFieldDescriptors, sample);
        }
		var waterLevel = waterLevelFromDistance(sample.field1);
		sample.field6 = waterLevel.toString();
        samples.push(sample);
    }
    // set connection data on last sample
    var lastSample = samples[samples.length-1];
    var connFields = connDataStr.match(fieldRE);
    for (x in connFields) {
        parseField(connFields[x], sensorFieldDescriptors, lastSample);
    }
    
    // capture water level to send to display
    latestWaterLevel = lastSample.field6;
    
    // send to ThingSpeak
    postBulkDataToThingSpeak(samples);
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
    var setTimeCmd = 'tset ' + gpsTime(now);
    if (pendingCommand.length > 0) {
        console.log('sending block start');
        sock.write('[\r');
        pendingCommand = '';
    }
    sock.write(setTimeCmd + '\r');

    sock["mycounter"] = 0;	
    sock["incomingData"] = '';
    mainsock = sock;
    // Add a 'data' event handler to this instance of socket
    sock.on('data', function(data) {
        sock.mycounter++;
        var dataString = data.toString('utf8');
        var now = new Date();
        console.log('DATA ' + now.toDateString() + " " + now.toLocaleTimeString() + ': ' +
            dataString + ', count: ' + sock.mycounter);
        latestWaterLevelTimestamp = gpsTime(now);
        if (sock.mycounter == 1) {
            // first line is sensor data
            // validate
            if ((dataString.charAt(0) == 'I') && 
                ((dataString.charAt(dataString.length-2) == 'Z') ||
                 (dataString.charAt(dataString.length-1) == 'Z'))) {
                // parse the data
                parseSensorDataFeed(dataString);
                
            } else {
                console.log('>>>> unexpected input - destroy socket');
                sock.destroy('unexpected');
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
    var dataCmd = 'data ' + latestWaterLevel + ' ' + latestWaterLevelTimestamp + ' ' + gpsTime(now);
    console.log(dataCmd);
    if (pendingCommand.length > 0) {
        console.log('sending block start');
        sock.write('[\r');
        pendingCommand = '';
    }
    sock.write(dataCmd + '\r');

    sock["mycounter"] = 0;
    sock["incomingData"] = '';
    mainsock = sock;
    // Add a 'data' event handler to this instance of socket
    sock.on('data', function(data) {
        sock.mycounter++;
        var dataString = data.toString('utf8');
        var now = new Date();
        console.log('display DATA ' + now.toDateString() + " " + now.toLocaleTimeString() + ': ' +
            dataString + ', count: ' + sock.mycounter);
        if (sock.mycounter == 1) {
            // first line is sensor data
            // validate
            if ((dataString.charAt(0) == 'I') && 
                ((dataString.charAt(dataString.length-2) == 'Z') ||
                 (dataString.charAt(dataString.length-1) == 'Z'))) {
                // parse the data
                parsePumpDataFeed(dataString);                
            } else {
                console.log('>>>> unexpected input - destroy socket');
                sock.destroy('unexpected');
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
        path: '/channels/8203/fields/6.json?results=1',
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
          latestWaterLevel = parseInt(responseObj.feeds[0].field6);
          console.log('Water Level: ' + latestWaterLevel);
          var latestWaterLevelDate = new Date(responseObj.feeds[0].created_at);
          console.log('      as of: ' + latestWaterLevelDate);
          latestWaterLevelTimestamp = gpsTime(latestWaterLevelDate);
      });
  });
  
  // get the data
  console.log("Getting field from ThingSpeak");
  get_req.end();
}
readLatestWaterLevelFromThingSpeak();

waterLevelMonitorServer.listen(3000, function() {
  console.log('water level monitor server bound');
});
waterLevelDisplayServer.listen(3010, function() {
  console.log('water level display server bound');
});
