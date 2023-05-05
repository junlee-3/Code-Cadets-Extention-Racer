#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>
#include <Servo.h> // Add the Servo library

#define trigPin D5
#define echoPin D6

Servo steering;
Servo ESC;

// Replace with your network credentials
const char* ssid = "CC-Ext";
const char* password = "CCRasPiNetwork";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create a WebSocket object

AsyncWebSocket ws("/ws");

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
<title>ESP Car</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="icon" type="image/png" href="favicon.png">
<link rel="stylesheet" type="text/css" href="style.css">
</head>
<body>
<div class="topnav">
<h1>ESP Car</h1>
</div>
<div class="content">
<div class="card-grid">
<div class="card">
<button onclick="emergencyBreak()" style="background-color: red; color: white;">STOP!!!</button>
</div>
<div class="card">
<p class="card-title">Steering</p>
<p class="switch">
<input type="range" oninput="updateSliderPWM(this)" id="slider1" min="0" max="180" step="1" value ="0" class="slider">
</p>
<p class="state">Angle: <span id="sliderValue1"></span></p>
</div>
<div class="card">
<p class="card-title">Speed</p>
<p class="switch">
<input type="range" oninput="updateSliderPWM(this)" id="slider2" min="0" max="180" step="1" value ="0" class="slider" style="width: 500px;">
</p>
<p class="state">Speed: <span id="sliderValue2"></span></p>
</div>
</div>
</div>
<script>

var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener('load', onload);

function onload(event) {
initWebSocket();
}

function getValues(){
websocket.send("getValues");
}

function initWebSocket() {
console.log('Trying to open a WebSocket connection…');
websocket = new WebSocket(gateway);
websocket.onopen = onOpen;
websocket.onclose = onClose;
websocket.onmessage = onMessage;
}

function onOpen(event) {
console.log('Connection opened');
getValues();
}

function onClose(event) {
console.log('Connection closed');
setTimeout(initWebSocket, 2000);
}

function updateSliderPWM(element) {
var sliderNumber = element.id.charAt(element.id.length-1);
var sliderValue = document.getElementById(element.id).value;
document.getElementById("sliderValue"+sliderNumber).innerHTML = sliderValue;
console.log(sliderValue);
websocket.send(sliderNumber+"s"+sliderValue.toString());
}

function onMessage(event) {
console.log(event.data);
var myObj = JSON.parse(event.data);
var keys = Object.keys(myObj);

for (var i = 0; i < keys.length; i++){
var key = keys[i];
document.getElementById(key).innerHTML = myObj[key];
document.getElementById("slider"+ (i+1).toString()).value = myObj[key];
}
}

function emergencyBreak() {
websocket.send("Stop");
}
</script>
</body>
</html>
)rawliteral";

// Create Servo objects

String message = "";
String sliderValue1 = "0";
String sliderValue2 = "0";

int dutyCycle1;
int dutyCycle2;

long duration;
long distance;

//Json Variable to Hold Slider Values
JSONVar sliderValues;

//Get Slider Values
String getSliderValues(){
sliderValues["sliderValue1"] = String(sliderValue1);
sliderValues["sliderValue2"] = String(sliderValue2);

String jsonString = JSON.stringify(sliderValues);
return jsonString;
}

// Initialize WiFi
void initWiFi() {
WiFi.begin(ssid, password);
Serial.print("Connecting to WiFi ..");
while (WiFi.status() != WL_CONNECTED) {
Serial.print('.');
delay(1000);
}
Serial.println(WiFi.localIP());
}

void notifyClients(String sliderValues) {
ws.textAll(sliderValues);
Serial.println(sliderValues);
}

void updateSteering() {
steering.write(dutyCycle1);
}

void updateESC() {
ESC.write(dutyCycle2);
}

void emergencyBreak() {
ESC.write(0);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
AwsFrameInfo *info = (AwsFrameInfo*)arg;
if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
data[len] = 0;
message = (char*)data;
if (message.indexOf("1s") >= 0) {
sliderValue1 = message.substring(2);
dutyCycle1 = map(sliderValue1.toInt(), 0, 180, 0, 180); // modify the map function to match the servo range (0-180)
//Do something
Serial.println(dutyCycle1);
updateSteering();
Serial.print(getSliderValues());
notifyClients(getSliderValues());
}
if (message.indexOf("2s") >= 0) {
sliderValue2 = message.substring(2);
dutyCycle2 = map(sliderValue2.toInt(), 0, 180, 0, 180);
//Do something
Serial.println(dutyCycle2);
updateESC();
Serial.print(getSliderValues());
notifyClients(getSliderValues());
}
if (strcmp((char*)data, "getValues") == 0) {
notifyClients(getSliderValues());
}
if (message.indexOf("Stop") >= 0) {
emergencyBreak();
}
}
}
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
switch (type) {
case WS_EVT_CONNECT:
Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
break;
case WS_EVT_DISCONNECT:
Serial.printf("WebSocket client #%u disconnected\n", client->id());
break;
case WS_EVT_DATA:
handleWebSocketMessage(arg, data, len);
break;
case WS_EVT_PONG:
case WS_EVT_ERROR:
break;
}
}

void initWebSocket() {
ws.onEvent(onEvent);
server.addHandler(&ws);
Serial.println("WebSocket");
}

void setup() {
Serial.begin(9600);

// pinMode(trigPin, OUTPUT);
// pinMode(echoPin, INPUT);

// Set servo pins
const int servoPin1 = 5;
const int servoPin2 = 16;

// Set servo object

// Attach servos to pins
steering.attach(D1);
ESC.attach(D2, 1000, 2000);

initWiFi();
initWebSocket();

// Web Server Root URL
server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
request->send(200, "text/html", index_html);
});

// server.serveStatic("/", LittleFS, "/");

// Start server
server.begin();
}

void loop() {
// Reset trigPin
// digitalWrite(trigPin, LOW);
// delayMicroseconds(2);
// // Sets the trigPin HIGH (ACTIVE) for 10 microseconds
// digitalWrite(trigPin, HIGH);
// delayMicroseconds(10);
// digitalWrite(trigPin, LOW);
// // Reads the echoPin, returns the sound wave travel time in microseconds
// duration = pulseIn(echoPin, HIGH);
// // Calculating the distance
// distance = duration * 0.034 / 2; // Speed of sound wave (in cm/s) divided by 2 (go and back)
// Serial.print("Distance: ");
// Serial.println(distance);
// if (distance < 30) {
// emergencyBreak();
// }

// Cleanup WebSocket clients
ws.cleanupClients();
}
