#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Adafruit_MLX90614.h>
#include <env.h>

// Library for disabling brownout connector
// #include "soc/soc.h"
// #include "soc/rtc_cntl_reg.h"

int led = 2;

// Grove GSR Sensor Pins
int groveGsr = A0;

// GY-906 Infrared Sensor
Adafruit_MLX90614 gy906 = Adafruit_MLX90614();

void setup() {
  Serial.begin(9600);

  WiFi.begin(wSsid, wPassword);

  Serial.print("Connecting to WiFi router");
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  // server.begin();
  Serial.println();

  Serial.print("Connected! @ address: ");
  Serial.println(WiFi.localIP());

  pinMode(led, OUTPUT);

  pinMode(groveGsr, INPUT);
  // if (gy906.begin() == false){ // Can take a while to initialize. Rewrite this to wait until it's ready.
  //   Serial.println("Qwiic IR thermometer did not acknowledge! Freezing!");
  //   while(1);
  // }

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    WiFiClientSecure client;

    client.setInsecure();

    http.begin(client, apiUrlEsrand); // Replace with your URL
    int httpResponseCode = http.GET(); // Send the request

    if (httpResponseCode > 0) {
      String payload = http.getString(); // Get the response payload
      Serial.println(httpResponseCode); // Print HTTP response code
      Serial.println(payload); // Print the response payload
    } else {
      Serial.print("Error on HTTP request: ");
      Serial.println(httpResponseCode);
    }
    
    http.end(); // Free resources
  }
}

void loop() {
  digitalWrite(led, HIGH);
  delay(100);
  digitalWrite(led, LOW);
  delay(100);

  // float sensorValue = analogRead(groveGsr);
  // Serial.println(sensorValue);
  // Serial.println(gy906.readObjectTempC());
}
