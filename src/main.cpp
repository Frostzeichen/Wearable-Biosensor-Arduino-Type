#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_MLX90614.h>
#include <Wire.h>
#include <MAX30105.h>
#include <env.h>

// Library for disabling brownout connector
// #include "soc/soc.h"
// #include "soc/rtc_cntl_reg.h"

int led = 8;
char bReadings[8][270];
int readingsCount = 0;

// Grove GSR Sensor Pins
int groveGsr = A0;

// GY-906 Infrared Sensor
Adafruit_MLX90614 gy906 = Adafruit_MLX90614();

// MAX30105 Pulse Oximeter
MAX30105 max30105;
const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg;

// AD8232 Heart Rate Monitor
int ad8232 = A1;

// For analog calibration
// This is a floating pin used for measuring changes in nearby magnetic waves and
// readings in analog noise.
int analogCalibrationPin = A3;
void rWifi() {
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
}

void rSetupAd8232() {
  pinMode(ad8232, INPUT);
}

void rSetupGroveGsr() {
  pinMode(groveGsr, INPUT);
}

void rSetupGy906() {
  if (!gy906.begin()) {
    Serial.println("GY-906 IR thermometer did not acknowledge! Freezing!");
    while (!gy906.begin()) {
      Serial.print(".");
      delay(100);
    }
  }
}

void rSetupMax30105() {
  if (!max30105.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30105 pulse oximeter did not acknowledge! Freezing!");
    while (!max30105.begin(Wire, I2C_SPEED_FAST)) {
      Serial.print(".");
      delay(100);
    }
  }
}

void rSendHttpRequest(char payload[2048]) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    WiFiClientSecure client;

    client.setInsecure();

    char url[2048];
    strcpy(url, apiUrlEsrand);
    strcat(url, payload);

    http.begin(client, url); // Replace with your URL
    Serial.println(url);
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

int sAd8232() {
  return analogRead(ad8232);
}

int sGroveGsr() {
  return analogRead(groveGsr);
}

unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return(0);
  }
  time(&now);
  return now;
}

boolean initTime() {
  struct tm timeinfo;

  log_i("Synchronizing time.");
  // Connect to NTP server with 0 TZ offset, call setTimezone() later
  configTime(0, 0, "pool.ntp.org");
  // getLocalTime uses a default timeout of 5s -> the loop takes at most 3*5s to complete
  for (int i = 0; i < 3; i++) {
    if (getLocalTime(&timeinfo)) {
      log_i("UTC time: %s.", getCurrentTimestamp(SYSTEM_TIMESTAMP_FORMAT).c_str());
      return true;
    }
  }

  log_e("Failed to obtain time.");
  return false;
}

void setup() {
  Serial.begin(9600);
  delay(5000); // add delay for slow serial race condition

  rWifi();
  pinMode(led, OUTPUT);
  pinMode(analogCalibrationPin, INPUT);

  rSetupAd8232();
  rSetupGroveGsr();
  // rSetupMax30105();
  // rSetupGy906();

  rSendHttpRequest("");
  initTime();
}

void loop() {
  digitalWrite(led, HIGH);
  delay(100);
  digitalWrite(led, LOW);
  delay(100);

  char data[270];
  // unsigned long now = getTime();
  // Serial.println(now);
  sprintf(data, "{\"ad8232\": %d, \"groveGsr\": %d, \"analog calibration pin\": %d}", sAd8232(), sGroveGsr(), analogRead(analogCalibrationPin));

  if (readingsCount < 4) {
    strcpy(bReadings[readingsCount], data);
    readingsCount++;
  } else {
    char jsonReadings[readingsCount * 270];
    strcpy(jsonReadings, "[");
    for (int i = 0; i < readingsCount - 1; i++) {
      strcat(jsonReadings, bReadings[i]);
      if (i == readingsCount - 2) {
        strcat(jsonReadings, "]");
      } else {
        strcat(jsonReadings, ",");
      }
      bReadings[i][0] = '\0';
    }
    rSendHttpRequest(jsonReadings);

    readingsCount = 0;
  }
  
  // rSendHttpRequest(data);

  // Serial.println(gy906.readObjectTempC());
  
  // max30105.setup();
  // max30105.setPulseAmplitudeRed(0x0A);
  // max30105.setPulseAmplitudeGreen(0);

  // long irValue = max30105.getIR();

  // if (irValue > 50000) {
  //   //We sensed a beat!
  //   long delta = millis() - lastBeat;
  //   lastBeat = millis();

  //   beatsPerMinute = 60 / (delta / 1000.0);

  //   if (beatsPerMinute < 255 && beatsPerMinute > 20)
  //   {
  //     rates[rateSpot++] = (byte)beatsPerMinute; //Store this reading in the array
  //     rateSpot %= RATE_SIZE; //Wrap variable

  //     //Take average of readings
  //     beatAvg = 0;
  //     for (byte x = 0 ; x < RATE_SIZE ; x++)
  //       beatAvg += rates[x];
  //     beatAvg /= RATE_SIZE;
  //   }
  // }

  // Serial.print("IR=");
  // Serial.print(irValue);
  // Serial.print(", BPM=");
  // Serial.print(beatsPerMinute);
  // Serial.print(", Avg BPM=");
  // Serial.print(beatAvg);

  // if (irValue < 50000)
  //   Serial.print(" No finger?");

  // max30105.shutDown();

  // Serial.println();

  // byte error, address;
  // int nDevices;
  // Serial.println("Scanning...");
  // nDevices = 0;
  // for(address = 1; address < 127; address++ ) {
  //   Wire.beginTransmission(address);
  //   error = Wire.endTransmission();
  //   if (error == 0) {
  //     Serial.print("I2C device found at address 0x");
  //     if (address<16) {
  //       Serial.print("0");
  //     }
  //     Serial.println(address,HEX);
  //     nDevices++;
  //   }
  //   else if (error==4) {
  //     Serial.print("Unknow error at address 0x");
  //     if (address<16) {
  //       Serial.print("0");
  //     }
  //     Serial.println(address,HEX);
  //   }    
  // }
  // if (nDevices == 0) {
  //   Serial.println("No I2C devices found\n");
  // }
  // else {
  //   Serial.println("done\n");
  // }
  // delay(5000);          
}