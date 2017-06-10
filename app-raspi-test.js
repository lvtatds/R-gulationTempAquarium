'use strict';

var webSocketUrl = "wss://api.artik.cloud/v1.1/websocket?ack=true";
var device_id = "044379fd21c34ab686947d8950f66a26";
var device_token = "f37a743d4f724302842abb0a917d9c5a";

var isWebSocketReady = false;
var ws = null;

var serialport = require("serialport");
var SerialPort = serialport.SerialPort;
var sp = new SerialPort("/dev/rfcomm0", {
  baudrate: 9600,
  parser: serialport.parsers.readline("\n")
});

var WebSocket = require('ws');

/**
 * Gets the current time in millis
 */
function getTimeMillis() {
  return parseInt(Date.now().toString());
}

/**
 * Create a /websocket device channel connection 
 */
function start() {
  //Create the websocket connection
  isWebSocketReady = false;
  ws = new WebSocket(webSocketUrl);
  ws.on('open', function() {
    console.log("Websocket - Connection is open ....");
    register();
  });
  ws.on('message', function(data, flags) {
    console.log("Websocket - Received message : " + data + '\n');
  });
  ws.on('close', function() {
    console.log("Websocket - Connection is closed ....");
  });
}

/**
 * Sends a register message to the websocket and starts the message flooder
 */
function register() {
  console.log("Websocket - Registering device on the websocket connection");
  try {
    var registerMessage = '{"type":"register", "sdid":"' + device_id + '", "Authorization":"bearer ' + device_token + '", "cid":"' + getTimeMillis() + '"}';
    console.log('Websocket - Sending register message ' + registerMessage + '\n');
    ws.send(registerMessage, {
      mask: true
    });
    isWebSocketReady = true;
  } catch (e) {
    console.error('Websocket - Failed to register messages. Error in registering message: ' + e.toString());
  }
}

/**
 * Send one message to ARTIK Cloud
 */
function sendData(data) {
  try {
    var ts = ', "ts": ' + getTimeMillis();
    var payload = '{"sdid":"' + device_id + '"' + ts + ', "data": ' + JSON.stringify(data) + ', "cid":"' + getTimeMillis() + '"}';
    console.log('Websocket - Sending payload ' + payload);
    ws.send(payload, {
      mask: true
    });
  } catch (e) {
    console.error('Websocket - Error in sending a message: ' + e.toString());
  }
}

sp.on("open", function() {
  sp.on('data', function(data) {
    console.log("SerialPort - received data:" + data);

    var parsedData;
    try {
      parsedData = JSON.parse(data);
    }
    catch(err) {
      console.log('received data is not in JSON format');
    }

    if(parsedData !== undefined) {
      var dataToSend = {
        temp: parsedData.temp,
        tempLimit: parsedData.tempLimit,
        tempMin: parsedData.tempMin,
        tempMax: parsedData.tempMax,
        isFanActivated: parsedData.fanStatus==='ON'?true:false,
        fanMode: parsedData.fanMode
      };
      
      if (!isWebSocketReady) {
        console.log("Websocket is not ready. Skip sending data to ARTIK Cloud (data:" + dataToSend + ")");
        return;
      }

      sendData(dataToSend);
    }
  });

  var interval = 1000 * 10; // 10s

  setInterval(function() {
    sp.write('3', function(err) {
      if (err)
        return console.log('SerialPort - Error on write ' + err.message);
    });
  }, interval);
});

/**
 * All start here
 */
start(); // create websocket connection
