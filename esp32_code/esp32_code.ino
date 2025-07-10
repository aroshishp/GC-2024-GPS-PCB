//FIREBASE, SD CARD FUNCTIONALITIES VERIFIED TO BE WORKING FOR GPS DATA AFTER FIX. ISSUES: TIMESTAMP IS PROBABLY UTC/GMT AND PRINTS ONLY WHEN FIRST
//FIX IS ESTABLISHED. SERIAL NOS START FROM 1 EVERYTIME. BOTH ISSUES ARE NON-CRITICAL.
#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif
#include <Firebase_ESP_Client.h>
// #include <FirebaseESP32.h>
#include <stdlib.h>
// #include <TinyGPS++.h>
#include <TinyGPSPlus.h>
#include <SPI.h>
#include <SD.h>

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// INSERT/CHOOSE NETWORK CREDENTIALS
#define WIFI_SSID "Expecto Nexum"
#define WIFI_PASSWORD "aroshishn-910"

// #define WIFI_SSID "AROS-INSPIRON-5 7104"
// #define WIFI_PASSWORD "venchita"

// #define WIFI_SSID "realme GT Master Edition"
// #define WIFI_PASSWORD "x5bt44tz"

// #define WIFI_SSID "OnePlus Nord CE 3 Lite 5G"
// #define WIFI_PASSWORD "1234567890"

// Insert Firebase project API Key
// #define API_KEY "AIzaSyB8Mcs2ovwsid9nIzTKozB_Hl0CVzBvnv8" //og
#define API_KEY "AIzaSyAjNNHkKbmqwE2tJPHJ3MkCaUYEe5VLbgA" //venky
// #define API_KEY "AIzaSyDWfGkEepByUsbDf2FBf98J9u2mBwSeDpQ" //avyaaz
// #define API_KEY "AIzaSyAdLZ237Vgt0i0D_ZjqYRpjmigrMeIzBwI" //2k25


// #define FIRESTORE_PROJECT_ID "gpsapp-ef676" //avyaaz
#define FIRESTORE_PROJECT_ID "gps-app-13423" //venky
// #define FIRESTORE_PROJECT_ID "gps-coordinates-3ec39" //og
// Third acc:
// #define FIRESTORE_PROJECT_ID "gps-firebase-bb3a5" //2k25
#define COLLECTION_NAME "users"
// #define COLLECTION_NAME "esp_ids"
#define ESP_ID "ESP9494"
// #define ESP_ID "ESP12345"

// SD card configurations
#define SD_CS 5
#define CSV_FILENAME "/gps_data.csv"
#define FILE_SIZE_THRESHOLD 1024 * 1024 * 1024 // 1 GB

TinyGPSPlus gps;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

HardwareSerial SerialGPS(2);
File dataFile;

void initializeSDCard() {
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card initialization failed!");
    while (true);
  }
  Serial.println("SD card initialized.");

  // Check if the file exists, if not create it with headers
  if (!SD.exists(CSV_FILENAME)) {
    dataFile = SD.open(CSV_FILENAME, FILE_WRITE);
    if (dataFile) {
      dataFile.println("serial no.,timestamp,ESP_ID,latitude,longitude,altitude,speed,satellites");
      dataFile.close();
    } else {
      Serial.println("Failed to create file on SD card.");
    }
  }
}

void writeDataToSD(int serialNo, const String& timestamp, const String& espId, float latitude, float longitude, float altitude, float speed, int satellites) {
  dataFile = SD.open(CSV_FILENAME, FILE_APPEND);
  if (dataFile) {
    dataFile.printf("%d,%s,%s,%.6f,%.6f,%.2f,%.2f,%d\n", serialNo, timestamp.c_str(), espId.c_str(), latitude, longitude, altitude, speed, satellites);
    dataFile.close();
  } else {
    Serial.println("Failed to open file for appending.");
  }

  // Check file size and truncate if necessary
  if (SD.exists(CSV_FILENAME) && SD.open(CSV_FILENAME).size() > FILE_SIZE_THRESHOLD) {
    Serial.println("File size exceeded threshold. Truncating...");
    truncateSDFile();
  }
}

void truncateSDFile() {
  File tempFile = SD.open("/temp.csv", FILE_WRITE);
  dataFile = SD.open(CSV_FILENAME, FILE_READ);

  if (tempFile && dataFile) {
    // Skip the first 1000 lines
    int linesToSkip = 1000;
    int currentLine = 0;
    String line;

    while (dataFile.available()) {
      line = dataFile.readStringUntil('\n');
      if (currentLine++ > linesToSkip) {
        tempFile.println(line);
      }
    }

    dataFile.close();
    tempFile.close();

    // Replace the old file with the truncated one
    SD.remove(CSV_FILENAME);
    SD.rename("/temp.csv", CSV_FILENAME);
  } else {
    Serial.println("Failed to truncate the file.");
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  SerialGPS.begin(9600, SERIAL_8N1, 27, 32);
  Serial.println("GPS Module Testing");

  // initializeSDCard();

  config.api_key = API_KEY;
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("ok");
    signupOK = true;
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

int serialNo = 1;
void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input == "restart") {
      delay(100);
      ESP.restart();
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi not connected, attempting to reconnect...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();
    Serial.println("Wi-Fi reconnected!");
  }

  while (SerialGPS.available() > 0) {

    Serial.println("Serial.GPS is available.");
    gps.encode(SerialGPS.read());
    Serial.print("Latitude: ");
    Serial.println(gps.location.lat(), 6);  // 6 decimal places for precision
    Serial.print("Longitude: ");
    Serial.println(gps.location.lng(), 6);  // 6 decimal places for precision
    Serial.print("Altitude (meters): ");
    Serial.println(gps.altitude.meters());
    Serial.print("Speed (km/h): ");
    Serial.println(gps.speed.kmph());
    Serial.print("Satellites: ");
    Serial.println(gps.satellites.value());
    Serial.println();

    float latitude = gps.location.lat();
    float longitude = gps.location.lng();
    float altitude = gps.altitude.meters();
    float speed = gps.speed.kmph();
    int satellites = gps.satellites.value();

    String timestamp = "";
    // if (gps.time.isUpdated()) {
    //   timestamp = String(gps.time.hour()) + ":" + String(gps.time.minute()) + ":" + String(gps.time.second());
    // } else {
    //   timestamp = "N/A";
    // }

    // Serial.printf("Latitude: %.6f, Longitude: %.6f, Altitude (in m): %.2f, Speed (km/h): %.2f, Satellites: %d\n",
                  // latitude, longitude, altitude, speed, satellites);

      String latitudeStr = String(latitude, 6); 
      String longitudeStr = String(longitude, 6); 
      String altitudeStr = String(altitude, 6); 
      String speedStr = String(speed, 3);
      String satellitesStr = String(satellites); 

      // String latitudeStr = "17.5986572";  // 6 decimal places for precision
      // String longitudeStr = "78.1224119";  // 6 decimal places for precision

      if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 10000 || sendDataPrevMillis == 0)){
        sendDataPrevMillis = millis();

        FirebaseJson jsonData;
        jsonData.set("fields/latitude/stringValue", latitudeStr);
        jsonData.set("fields/longitude/stringValue", longitudeStr);
        jsonData.set("fields/deviceId/stringValue", ESP_ID);
        jsonData.set("fields/altitude/stringValue", altitudeStr);
        jsonData.set("fields/speed/stringValue", speedStr);
        jsonData.set("fields/satellites/stringValue", satellitesStr);

        String documentPath = String(COLLECTION_NAME) + "/" + ESP_ID;

        String projectId = FIRESTORE_PROJECT_ID;
        String databaseId = "(default)";

        if (Firebase.Firestore.patchDocument(&fbdo, projectId.c_str(), databaseId.c_str(), documentPath.c_str(), jsonData.raw(), "latitude,longitude,deviceId,altitude,speed,satellites")) {
          Serial.println("Data sent successfully to Firestore!");
        } else {
          Serial.println("Failed to send data to Firestore");
          Serial.println(fbdo.errorReason());
        }
      }

    // writeDataToSD(serialNo++, timestamp, ESP_ID, latitude, longitude, altitude, speed, satellites);

    delay(2000);
  }
}