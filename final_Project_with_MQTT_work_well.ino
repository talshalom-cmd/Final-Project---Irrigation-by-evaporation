#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_SHT31.h" 
#include <Adafruit_ADS1X15.h>
#include <WiFi.h>
#include "ThingSpeak.h" 
#include <time.h> 
#include <PubSubClient.h> 

// --- ThingSpeak Settings ---
unsigned long myChannelNumber = 3229286;
const char* myWriteAPIKey = "SWCDL2P4GXBJI2L2";
unsigned long readChannelNumber = 3229296;
const char* readAPIKey = "UZEGA0FA76O2C7J2";

// --- WiFi credentials ---
const char* ssid = "YOUR_WIFI";          
const char* password = "YOUR_WIFI_PASSWORD";

WiFiClient client; // This client is used for ThingSpeak
unsigned long wait_between_uploads = 15000; // 15 seconds

// --- Sensors ---
Adafruit_SHT31 sht31 = Adafruit_SHT31();
Adafruit_ADS1115 ads; 

// --- Rain Gauge ---
float totalRainMM = 0;
const int rainPin = D2;      
volatile int rainTips = 0;   
unsigned long lastRainTime = 0;

// --- Irrigation & Logic Variables ---
float savedIrrigationTime = 0; 
bool irrigationCheckedToday = false; // Flag for the 19:00 check

// variables for timed irrigation
bool isIrrigating = false;               // Is irrigation currently active?
unsigned long irrigationStartTime = 0;   // When did it start?
unsigned long irrigationDurationMillis = 0; // How long should it run?

// --- MQTT Settings ---
const char* mqtt_server = "192.168.0.102";
const int mqtt_port = 1883;                   
const char* mqtt_user = "mqtt-user";          
const char* mqtt_password = "1234";           
const char* mqtt_topic = "/greenhouse/outside/irrigation/solenoid5"; 

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// --- Interrupt Function ---
void IRAM_ATTR onRainTip() {
  if (millis() - lastRainTime > 200) { 
    rainTips++;
    lastRainTime = millis();
  }
}

// --- Helper to print time ---
void printCurrentTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.print("Current Time: ");
  Serial.println(&timeinfo, "%H:%M:%S");
}

// --- MQTT Reconnect Function ---
void reconnect() {
  // Loop until we're reconnected
  if (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" (will try again in main loop)");
    }
  }
}

void setup() {
  Serial.begin(115200);
  while(!Serial);
  
  Serial.println("\n--- FINAL TEST START ---");
  
  // --- WiFi Connection ---
  WiFi.disconnect();
  delay(10);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to "); Serial.println(ssid);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nOnline! IP: " + WiFi.localIP().toString());

  // --- Time Config ---
  // Setting Israel time zone
  configTzTime("IST-2IDT,M3.4.4/26,M10.5.0", "pool.ntp.org", "time.nist.gov");
  Serial.println("Time synced.");

  // --- Initialize ThingSpeak ---
  ThingSpeak.begin(client);

  // --- Initialize Sensors ---
  pinMode(rainPin, INPUT_PULLUP); 
  attachInterrupt(digitalPinToInterrupt(rainPin), onRainTip, FALLING);

  Wire.begin();
  if (!sht31.begin(0x44)) {
    if(!sht31.begin(0x45)) Serial.println("SHT31: ERROR");
  }

  ads.setGain(GAIN_ONE); 
  if (!ads.begin()) Serial.println("ADS1115: ERROR");

  // --- MQTT Setup ---
  mqttClient.setServer(mqtt_server, mqtt_port);
  reconnect();
}

void loop() {
  // 1. Maintain MQTT connection
  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop(); 

  static unsigned long lastUploadTime = 0;
  
  // --- 15 Second Timer: Read Sensors & Upload to ThingSpeak ---
  if (millis() - lastUploadTime > wait_between_uploads) {
    lastUploadTime = millis();
    Serial.println("\n--- READING SENSORS ---");
    
    // Read sensors
    float tempC = sht31.readTemperature();
    float humy = sht31.readHumidity();
    if (isnan(tempC)) { tempC = 0; humy = 0; }

    float vWind = ads.computeVolts(ads.readADC_SingleEnded(1));
    float vSolar = ads.computeVolts(ads.readADC_SingleEnded(2));
    float windSpeed = (vWind > 0.4) ? (vWind - 0.4) * 20.25 : 0;
    float solarRad = vSolar / 0.00025832;
    totalRainMM = rainTips * 0.5;

// Print results to Serial Monitor
    if (isnan(tempC)) {
    Serial.println("Temp/Hum: Error");
    tempC = 0; humy = 0; // Prevent sending NaN
  } else {
    Serial.print("Temp: "); Serial.print(tempC); Serial.print(" C | Hum: "); Serial.print(humy); Serial.println("%");
  }
  Serial.print("WIND: "); Serial.print(windSpeed); Serial.println(" m/s");
  Serial.print("SOLAR: "); Serial.print(solarRad); Serial.println(" W/m2");
  Serial.print("TOTAL RAIN: "); Serial.print(totalRainMM); Serial.println(" mm");
    
    // Send to ThingSpeak
    ThingSpeak.setField(1, tempC);
    ThingSpeak.setField(2, solarRad);
    ThingSpeak.setField(3, windSpeed);
    ThingSpeak.setField(4, humy);
    ThingSpeak.setField(5, totalRainMM);
    int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    if(x == 200) Serial.println("ThingSpeak Upload success.");
    else Serial.println("ThingSpeak Upload failed.");
  }

  // --- Daily Check Logic: Check at 19:00 ---
  struct tm timeinfo;
  if(getLocalTime(&timeinfo)){
    
    // Check: Is it 19:00?
    if (timeinfo.tm_hour == 19 && timeinfo.tm_min == 0) {
      
      // Check: Have we already performed the action today?
      if (!irrigationCheckedToday) {
        
        Serial.println("\n[DAILY CHECK 19:00] Reading from ThingSpeak...");
        printCurrentTime();

        // Read from ThingSpeak
        int status = ThingSpeak.readIntField(readChannelNumber, 2, readAPIKey);
        float duration = ThingSpeak.readFloatField(readChannelNumber, 3, readAPIKey);
        int statusCode = ThingSpeak.getLastReadStatus();

        if (statusCode == 200) {
          Serial.print("Data Received -> Status: "); Serial.print(status);
          Serial.print(" | Duration: "); Serial.println(duration);

          // Ensure MQTT connection
          if (!mqttClient.connected()) {
             reconnect(); 
          }

          if (status == 1) {
            // --- START IRRIGATION SEQUENCE ---
            Serial.print("Sending ON (1) to MQTT... ");
            if (mqttClient.publish(mqtt_topic, "1")) {
              Serial.println("Success!");
              
              // Set timer flags
              isIrrigating = true;
              irrigationStartTime = millis();
              // Calculate duration in millis (assuming duration is in MINUTES)
              irrigationDurationMillis = (unsigned long)(duration * 60 * 1000); 
              Serial.print("Irrigation timer started for ");
              Serial.print(duration);
              Serial.println(" minutes.");

            } else {
              Serial.println("FAILED sending packet.");
            }
          } 
          else {
            // If status is 0, ensure we are OFF and cancel any running timer
            Serial.print("Sending OFF (0) to MQTT... ");
            if (mqttClient.publish(mqtt_topic, "0")) {
               Serial.println("Success!");
            }
            isIrrigating = false; // Cancel active irrigation if forced off
          }
          
        } else {
          Serial.println("Error reading from ThingSpeak");
        }

        // Mark action as done for today
        irrigationCheckedToday = true;
      }
    } 
    else {
      // Reset flag when it's not 19:00
      irrigationCheckedToday = false;
    }
  }

  // --- IRRIGATION TIMER CHECK ---
  // This runs in every loop iteration to check if it's time to stop
  if (isIrrigating) {
    if (millis() - irrigationStartTime >= irrigationDurationMillis) {
      
      Serial.println("\n[TIMER] Irrigation duration ended. Sending OFF.");
      
      if (!mqttClient.connected()) {
        reconnect();
      }

      // Send OFF command
      if (mqttClient.publish(mqtt_topic, "0")) {
        Serial.println("Irrigation STOPPED successfully.");
      } else {
        Serial.println("Failed to send STOP command.");
      }

      // Reset state
      isIrrigating = false;
    }
  }
}