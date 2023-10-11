#include <SPI.h>
#include <WiFiEsp.h>
#include <WiFiEspClient.h>
#include <WiFiEspServer.h>
#include <DHT.h>
#include "DFRobot_PH.h"
#include "DFRobot_EC10.h"
#include <arduino-timer.h>

// WiFi network settings
char ssid[] = "SmartHydro";       // newtork SSID (name). 8 or more characters
char password[] = "Password123";  // network password. 8 or more characters
String message = "";

#include "EC.h"
#include "pH.h"
#include "Humidity.h"
#include "Temperature.h"
Eloquent::ML::Port::RandomForestEC ForestEC;
Eloquent::ML::Port::RandomForestpH ForestPH;
Eloquent::ML::Port::RandomForestHumidity ForestHumidity;
Eloquent::ML::Port::RandomForestTemperature ForestTemperature;

WiFiEspServer server(80);
RingBuffer buf(8);

#define FLOW_PIN 2
#define LIGHT_PIN A7
#define EC_PIN A8
#define PH_PIN A9
#define DHTTYPE DHT22
#define LED_PIN 4
#define FAN_PIN 5
#define PUMP_PIN 6
#define EXTRACTOR_PIN 7  
#define DHT_PIN 8
#define PH_UP_PIN 9
#define PH_DOWN_PIN 10
#define EC_UP_PIN 11
#define EC_DOWN_PIN 12


DFRobot_PH ph;
DHT dht = DHT(DHT_PIN, DHTTYPE);

DFRobot_EC10 ec;
auto timer = timer_create_default();
float temperature;
float humidity;
float ecLevel;
float phLevel;
float lightLevel;
float flowRate;
volatile int pulseCount = 0;
unsigned long currentTime, cloopTime;

void setup() {
  Serial.begin(9600);
  Serial1.begin(115200);
  WiFi.init(&Serial1);  // Initialize ESP module using Serial1
  
  // Check for the presence of the ESP module
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi module not detected. Please check wiring and power.");
    while (true)
      ;  // Don't proceed further
  }

  Serial.print("Attempting to start AP ");
  Serial.println(ssid);

  IPAddress localIp(192, 168, 8, 14);  // create an IP address
  WiFi.configAP(localIp);              // set the IP address of the AP

  // start access point
  // channel is the number. Ranges from 1-14, defaults to 1
  // last comma is encryption type
  WiFi.beginAP(ssid, 11, password, ENC_TYPE_WPA2_PSK);

  Serial.print("Access point started");

  // Start the server
  server.begin();
  ec.begin();
  dht.begin();
  ph.begin();

  pinMode(FLOW_PIN, INPUT);

   attachInterrupt(0, incrementPulseCounter, RISING);
   sei();

    for (int i = 4; i <= 12; i++)
     {
      if (i != 8) {
        pinMode(i, OUTPUT);
        togglePin(i);
      }
     }

  // turning on equipment that should be on by default
  togglePin(LED_PIN);
  togglePin(FAN_PIN);
  togglePin(PUMP_PIN);
  togglePin(EXTRACTOR_PIN);

  Serial.println("Server started");
  timer.every(10000, estimateTemperature);
}


void loop() {
  WiFiEspClient client = server.available();  // Check if a client has connected

  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  lightLevel = getLightLevel();
  ecLevel = getEC();
  phLevel = getPH();
  flowRate = getFlowRate();

  timer.tick();

  if (client) {  // If a client is available
    buf.init();
    message = "";
    while (client.connected()) {  // Loop while the client is connected
      if (client.available()) {   // Check if data is available from the client
        char c = client.read();
        //Serial.write(c); // Echo received data to Serial Monitor
        buf.push(c);
        // you got two newline characters in a row
        // that's the end of the HTTP request, so send a response
        if (buf.endsWith("\r\n\r\n")) {
          message = "{\n  \"PH\": \"" + String(phLevel) + "\",\n \"Light\": \"" + String(lightLevel) +  "\",\n  \"EC\": \"" + String(ecLevel) + "\",\n  \"FlowRate\": \"" + String(flowRate) + "\",\n  \"Humidity\": \"" + String(humidity) + "\",\n  \"Temperature\": \"" + String(temperature) +  "\"\n }"; 
          ec.calibration(ecLevel, temperature); 

          sendHttpResponse(client, message);
          break;
        }

        if (buf.endsWith("/light")) {
          togglePin(LED_PIN);
        } 

        if (buf.endsWith("/fan")) {
          togglePin(FAN_PIN);
        } 

        if (buf.endsWith("/extractor")) {
          togglePin(EXTRACTOR_PIN);
        } 

        if (buf.endsWith("/pump")) {
          togglePin(PUMP_PIN);
        } 

        if (buf.endsWith("/ph")) {
          if (digitalRead(PH_DOWN_PIN) == 0 && digitalRead(PH_UP_PIN) == 0) togglePin(PH_UP_PIN);
          if (digitalRead(PH_UP_PIN == 1) || digitalRead(PH_DOWN_PIN) == 1) togglePh();
        }

        if (buf.endsWith("/ec")) {
          if (digitalRead(EC_DOWN_PIN) == 0 && digitalRead(EC_UP_PIN) == 0) togglePin(EC_UP_PIN);
          if (digitalRead(EC_UP_PIN == 1) || digitalRead(EC_DOWN_PIN == 1) toggleEc();
        }

        if (buf.endsWith("/components")) {
          message = "{\n  \"PHPump\": \"" + String(digitalRead(PH_UP_PIN)) + "\",\n \"Light\": \"" + String(digitalRead(LIGHT_PIN)) +  "\",\n  \"ECPump\": \"" + String(EC_UP_PIN) + "\",\n  \"WaterPump\": \"" + String(digitalRead(PUMP_PIN)) + "\",\n  \"Exctractor\": \"" + String(digitalRead(EXTRACTOR_PIN)) + "\",\n  \"Fan\": \"" + String(digitalRead(FAN_PIN)) +  "\"\n }"; 
        }
      }
    }
    Serial.println("Client disconnected");
    client.stop();  // Close the connection with the client
  }
}


  /**
  * Inverts the reading of a pin.
  */
void togglePin(int pin) {
  digitalWrite(pin, !(digitalRead(pin)));
}
  /**
  * Sends a http response along with a message.
  */
void sendHttpResponse(WiFiEspClient client, String message) {
    client.print(
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Connection: close\r\n");

  if (message.length() > 0) {
    client.print("Content-Length:" + String(message.length()) + "\r\n\r\n");
    Serial.print(message.length());
    client.print(message);
  }
}

//Need assistance to confirm if calculations are correct.
float getLightLevel() {
  return analogRead(LIGHT_PIN);
}

float getEC() {
  float ecVoltage = (float)analogRead(EC_PIN)/1024.0*5000.0; 
  return ec.readEC(ecVoltage, temperature);
}

float getPH() {
  float phVoltage = analogRead(PH_PIN)/1024.0*5000; 
  return ph.readPH(phVoltage, temperature);
}

void estimateTemperature() {
  int result = ForestTemperature.predict(&temperature);
  int fanStatus = digitalRead(FAN_PIN);
  int lightStatus = digitalRead(LIGHT_PIN);
  Serial.println(result);

  switch(result) {
    case 0: //HIGH
      if (fanStatus == 1) togglePin(FAN_PIN);
      if (lightStatus == 1) togglePin(LIGHT_PIN);
      
    case 1:
      if (fanStatus == 0) togglePin(FAN_PIN);
      if (lightStatus == 0) togglePin(LIGHT_PIN);
  }
}

void estimateHumidity() {
  int result = ForestHumidity.predict(&humidity);
  int extractorStatus = digitalRead(EXTRACTOR_PIN);
  int fanStatus = digitalRead(FAN_PIN);

  switch(result) {
    case 0: 
      if (extractorStatus == 1) togglePin(EXTRACTOR_PIN);
      if (fanStatus == 1) togglePin(FAN_PIN);
    
    case 1:
      if (extractorStatus == 0) togglePin(EXTRACTOR_PIN);
      if (fanStatus == 0) togglePin(FAN_PIN);
  }
}

void estimatePH() {
  int result = ForestPH.predict(&phLevel);
  int phUpStatus = digitalRead(PH_UP_PIN);
  int phDownStatus = digitalRead(PH_DOWN_PIN);

  switch (result) {
    case 0:
      if (phDownStatus == 0) togglePin(PH_DOWN_PIN);
      if (phUpStatus == 1) togglePin(PH_UP_PIN);
    
    case 1:
      if (phDownStatus == 1) togglePin(PH_DOWN_PIN);
      if (phUpStatus == 0) togglePin(PH_UP_PIN);
    
    case 2:
      if (phDownStatus == 1) togglePin(PH_DOWN_PIN);
      if (phUpStatus == 1) togglePin(PH_UP_PIN);
  }
}

void estimateEC() {
  int result = ForestEC.predict(&ecLevel);
  int ecUpStatus = digitalRead(EC_UP_PIN);
  int ecDownStatus = digitalRead(EC_DOWN_PIN);

  switch (result) {
    case 0:
      if (ecDownStatus == 0) togglePin(EC_DOWN_PIN);
      if (ecUpStatus == 1) togglePin(EC_UP_PIN);
    
    case 1:
      if (ecDownStatus == 1) togglePin(EC_DOWN_PIN);
      if (ecUpStatus == 0) togglePin(EC_UP_PIN);
    
    case 2:
      if (ecUpStatus == 1) togglePin(EC_UP_PIN);
      if (ecDownStatus == 1) togglePin(EC_DOWN_PIN);
  }
}

void estimateFactors() {
  estimatePH();
  estimateTemperature();
  estimateHumidity();
  estimateEC( );
}

void togglePh() {
  togglePin(PH_UP_PIN);
  togglePin(PH_DOWN_PIN);
}

void toggleEc() {
  togglePin(EC_UP_PIN);
  togglePin(EC_DOWN_PIN);
}

void incrementPulseCounter() {
  pulseCount++;
}

float getFlowRate() {
  currentTime = millis();

  if (currentTime >= (cloopTime + 1000)) {
    cloopTime = currentTime;
    float flowRatePerHr = (pulseCount * 60 / 7.5);
    pulseCount = 0;
    Serial.println(flowRatePerHr);
    return flowRatePerHr;
  }
}


