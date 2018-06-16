//var querystring = require('querystring'); // only need this for url-encoded CSV format
var http = require('http');
var net = require('net');
var readline = require('readline');

var HOST = 'localhost';
var PORT = 3000;
var ThingSpeakWritekey = "DD7TVSCZEHKZLAQP";

var mainsock;
var pendingCommand = '';

var tankEmptyDistance = 300;
var tankFullDistance = 30;
var latestWaterLevel = -1;  // -1 means unknown
var latestWaterLevelTimestamp = 0;

var gpsStart = Date.UTC(1980, 0, 6);
function gpsTime (date)
{
    return Math.round((date.getTime() - gpsStart) / 1000);
}

//
//  HTTP post to ThingSpeak
//
function postSensorDataToThingSpeak(sensorData) {
    // Build the post string from an object
    var postData = {
       "write_api_key" : ThingSpeakWritekey,
       "time_format" : "relative",
       "updates" : sensorData
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

//
//  End of HTTP post to ThingSpeak
//

//
// parse sensor data and send to ThingSpeak
//

var fieldDescriptors = {
   "D" : {fieldName : "delta_t",  divisor : 1   },
   "W" : {fieldName : "field1",   divisor : 10  },
   "T" : {fieldName : "field2",   divisor : 1   },
   "B" : {fieldName : "field3",   divisor : 100 },
   "Q" : {fieldName : "field4",   divisor : 1   },
   "C" : {fieldName : "field5",   divisor : 1   },
   };

// sets a field of sample from the given fieldStr. fieldStr
// is expected to be an uppercase letter followed by a number
function parseField(fieldStr, sample)  {
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
           parseField(sampleFields[x], sample);
        }
        samples.push(sample);
    }
    // set connection data on last sample
    var lastSample = samples[samples.length-1];
    var connFields = connDataStr.match(fieldRE);
    for (x in connFields) {
        parseField(connFields[x], lastSample);
    }
    
    // capture water level to send to display
    var latestSampleDistance = lastSample.field1;
    latestWaterLevel = Math.round(
        ((tankEmptyDistance - latestSampleDistance) * 100) /
        (tankEmptyDistance - tankFullDistance));
    
    // send to ThingSpeak
    postSensorDataToThingSpeak(samples);
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
                //parseSensorDataFeed(dataString);
                
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


waterLevelMonitorServer.listen(3000, function() {
  console.log('water level monitor server bound');
});
waterLevelDisplayServer.listen(3010, function() {
  console.log('water level display server bound');
});
