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
int sclPin = 6;
int sdaPin = 5;

// Grove GSR Sensor Pins
int groveGsr = A0;

// GY-906 Infrared Sensor
Adafruit_MLX90614 gy906 = Adafruit_MLX90614();
int gy906EnablePin = 10;

// MAX30105 Pulse Oximeter
int max30105EnablePin = 8;
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
  digitalWrite(gy906EnablePin, HIGH);
  if (!gy906.begin()) {
    Serial.println("GY-906 IR thermometer did not acknowledge! Freezing!");
    while (!gy906.begin()) {
      Serial.print(".");
      delay(100);
    }
  }
  Serial.println("GY-906 IR thermometer acknowledged.");
}

void rSetupMax30105() {
  digitalWrite(max30105EnablePin, HIGH);
  if (!max30105.begin()) {
    Serial.println("MAX30105 pulse oximeter did not acknowledge! Freezing!");
    while (!max30105.begin()) {
      Serial.print(".");
      delay(100);
    }
  }
  Serial.println("MAX30105 pulse oximeter acknowledged.");
  max30105.setup();
  max30105.setPulseAmplitudeRed(0x0A);
  max30105.setPulseAmplitudeGreen(0);
  delay(2000);
}

void rSendHttpRequest(char payload[2048], bool verbose = false) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    WiFiClientSecure client;

    client.setInsecure();

    http.begin(client, apiUrlEsrand); // Replace with your URL
    if (verbose) {
      Serial.println(apiUrlEsrand);
      Serial.println(payload);
    }
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST((uint8_t*)payload, strlen(payload));

    if (httpResponseCode > 0 && verbose) {
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

  Serial.println("Synchronizing time.");
  // Connect to NTP server with 0 TZ offset, call setTimezone() later
  configTime(0, 0, "time.google.com");
  if (getLocalTime(&timeinfo)) {
    Serial.println("Time synchronized.");
    Serial.println("Current time is: " + String(asctime(&timeinfo)));
    return true;
  }

  Serial.println("Failed to obtain time.");
  return false;
}

void insertTwoChars(char *str, int pos, char a, char b, char c) {
  int len = strlen(str);
  for (int i = len; i >= pos; i--) {
    str[i + 2] = str[i]; // shift right by 2
  }

  str[pos] = a;
  str[pos + 1] = b;
  str[pos + 2] = c;
}

bool i2cScanner(bool verbose = false) {
  byte error, address;
  int nDevices;

  if (verbose)
    Serial.println("Scanning...");

  nDevices = 0;
  for (address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      if (verbose) {
        Serial.print("I2C device found at address 0x");
        if (address < 16)
          Serial.print("0");
        Serial.print(address, HEX);
        if (verbose) {
          Serial.print(" (acknowledged)");
        }
        Serial.println("  !");
      }

      nDevices++;
    }
    else if (error == 4) {
      if (verbose) {
        Serial.print("Unknown error at address 0x");
        if (address < 16)
          Serial.print("0");
        Serial.println(address, HEX);
      }
    }
    else if (verbose) {
      Serial.print("No device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0) {
    if (verbose)
      Serial.println("No I2C devices found\n");
    return false;
  }
  else if (nDevices != 2) { // When adding a new I2C module, increment this by number of I2C devices added.
    if (verbose)
      Serial.println("Not all I2C devices found\n");
    return false;
  }
  else {
    Serial.println("done\n");
    return true;
  }
}

void setup() {
  Serial.begin(9600);
  delay(5000); // add delay for slow serial race condition

  rWifi();
  pinMode(led, OUTPUT);
  pinMode(analogCalibrationPin, INPUT);
  pinMode(gy906EnablePin, OUTPUT);
  pinMode(max30105EnablePin, OUTPUT);

  Wire.begin(sdaPin, sclPin);

  rSetupAd8232();
  rSetupGroveGsr();
  // rSetupGy906();
  rSetupMax30105();
  initTime();
}

void loop() {
  // float gy906Temp = gy906.readObjectTempC();
  long irValue = -1;

  if (i2cScanner()) {
    irValue = max30105.getIR();

    if (irValue > 50000) {
      //We sensed a beat!
      long delta = millis() - lastBeat;
      lastBeat = millis();

      beatsPerMinute = 60 / (delta / 1000.0);

      if (beatsPerMinute < 255 && beatsPerMinute > 20)
      {
        rates[rateSpot++] = (byte)beatsPerMinute; //Store this reading in the array
        rateSpot %= RATE_SIZE; //Wrap variable

        //Take average of readings
        beatAvg = 0;
        for (byte x = 0 ; x < RATE_SIZE ; x++)
          beatAvg += rates[x];
        beatAvg /= RATE_SIZE;
      }
    }
  }

  digitalWrite(led, HIGH);
  delay(100);
  digitalWrite(led, LOW);
  delay(100);

  char data[270];
  // sprintf(data, "{\"ad8232\": %d, \"groveGsr\": %d, \"analog calibration pin\": %d, \"time\": %lu, \"gy906\": %f, \"max30105\": {\"ir\": %d, \"bpm\": %d, \"avg bpm\": %d}}", sAd8232(), sGroveGsr(), analogRead(analogCalibrationPin), getTime(), gy906Temp, irValue, beatsPerMinute, beatAvg);
  sprintf(data, "{\"ad8232\": %d, \"groveGsr\": %d, \"analog calibration pin\": %d, \"time\": %lu, \"max30105\": {\"ir\": %d, \"bpm\": %d, \"avg bpm\": %d}}", sAd8232(), sGroveGsr(), analogRead(analogCalibrationPin), getTime(), irValue, beatsPerMinute, beatAvg);

  if (readingsCount < 8) {
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

    // Uncomment this only when you need to encode jsonReadings into RFC 3986 compliant URL format.
    // int loc = 0;
    // while (true) {
    //   if (jsonReadings[loc] == '[') {
    //     insertTwoChars(jsonReadings, loc, '%', '5', 'B');
    //     loc += 3;
    //   } else if (jsonReadings[loc]== ']') {
    //     insertTwoChars(jsonReadings, loc, '%', '5', 'D');
    //     loc += 3;
    //   } else if (jsonReadings[loc]== '{') {
    //     insertTwoChars(jsonReadings, loc, '%', '7', 'B');
    //     loc += 3;
    //   } else if (jsonReadings[loc]== '}') {
    //     insertTwoChars(jsonReadings, loc, '%', '7', 'D');
    //     loc += 3;
    //   } else if (jsonReadings[loc]== ' ') {
    //     insertTwoChars(jsonReadings, loc, '%', '2', '0');
    //     loc += 3;
    //   } else if (jsonReadings[loc]== '"') {
    //     insertTwoChars(jsonReadings, loc, '%', '2', '2');
    //     loc += 3;
    //   } else if (jsonReadings[loc]== ':') {
    //     insertTwoChars(jsonReadings, loc, '%', '3', 'A');
    //     loc += 3;
    //   } else if (jsonReadings[loc]== ',') {
    //     insertTwoChars(jsonReadings, loc, '%', '2', 'C');
    //     loc += 3;
    //   } else if (jsonReadings[loc]== '\0') {
    //     break;
    //   } else {
    //     loc++;
    //   }
    // }

    Serial.println(jsonReadings);
    // rSendHttpRequest(jsonReadings);

    readingsCount = 0;
  }

  Serial.println();
}