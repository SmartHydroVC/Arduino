#include <SPI.h>
#include <WiFiEsp.h>
#include <WiFiEspClient.h>
#include <WiFiEspServer.h>
#include <DHT.h>
#include "DFRobot_PH.h"
#include "DFRobot_EC.h"

// WiFi network settings
char ssid[] = "SmartHydro";       // newtork SSID (name). 8 or more characters
char password[] = "Password123";  // network password. 8 or more characters
String message = "";

WiFiEspServer server(80);
RingBuffer buf(8);

#define LIGHT_PIN A7
#define DHT_PIN 8
#define PH_PIN A9
#define EC_PIN A8
#define FLOW_PIN A3

#define DHTTYPE DHT22
DFRobot_PH ph;
DHT dht = DHT(DHT_PIN, DHTTYPE);

DFRobot_EC ec;
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
  Serial.println("Server started");
}


void loop() {
  WiFiEspClient client = server.available();  // Check if a client has connected


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
          sendHttpResponse(client, message);
          break;
        }

        //Appending to URL returns the data
        if (buf.endsWith("/M")) {
          float temperature = dht.readTemperature();
          float humidity = dht.readHumidity();
          float lightLevel = getLightLevel();

          float ecLevel = getEC(temperature);
          float phLevel = getPH(temperature);

          message = "[\n {\n  \"PH\": \"" + String(phLevel) + "\",\n \"Light Sensor\": \"" + String(lightLevel) + "\",\n  \"EC\": \"" + String(ecLevel) + "\",\n  \"Humidity\": \"" + String(humidity) + "\",\n  \"Temperature\": \"" + String(temperature) + "\"\n }\n]\n\n";
          ec.calibration(ecLevel,temperature); 
        }

        //Toggles LED
        if (buf.endsWith("/T")) {
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
    client.print(message);
  }
}


//Need assistance to confirm if calculations are correct.
float getLightLevel() {
  return analogRead(LIGHT_PIN);
}

float getEC(float temperature) {
  float ecVoltage = (float)analogRead(EC_PIN)/1024.0*5000.0; 
  return ec.readEC(ecVoltage,temperature);
}

float getPH(float temperature) {
  float phVoltage = analogRead(PH_PIN)/1024.0*5000; 
  return ph.readPH(phVoltage, temperature);
}