#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

/* --- NETWORK SETTINGS --- */
const char* WIFI_SSID   = "Wokwi-GUEST";
const char* WIFI_PASS   = "";
const char* MQTT_BROKER = "broker.hivemq.com";

const int   MQTT_PORT  = 1883;
const char* MQTT_TOPIC = "flood/a1/sensor01";
const char* DEVICE_ID  = "NODE_01";

/* --- PIN CONFIGURATION --- */
const int SDA_PIN  = 26;
const int SCL_PIN  = 25;
const int TRIG_PIN = 5;    // River Sensor
const int ECHO_PIN = 18;   // River Sensor
const int RAIN_TRIG = 19;  // Rain Gauge Sensor
const int RAIN_ECHO = 21;  // Rain Gauge Sensor
const int BATT_PIN  = 34;  // Battery Voltage Divider

/* --- CONSTANTS --- */
const float TUBE_HEIGHT = 30.0; 
const float R1 = 27000.0;       
const float R2 = 10000.0;       

/* --- GLOBAL VARIABLES --- */
float lastRainLevel = 0;
unsigned long lastRainCheck = 0;

/* --- OBJECTS --- */
Adafruit_BMP085 bmp;
WiFiClient      wifiClient;
PubSubClient    mqtt(wifiClient);

/* --- FUNCTION PROTOTYPES --- */
float getDistance(int trig, int echo);
float readBatteryPercentage();
void connectWifi();
void connectMqtt();

float getDistance(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long duration = pulseIn(echo, HIGH);
  if (duration == 0) return -1.0f;
  return (duration / 2.0) * 0.0343;
}

float readBatteryPercentage() {
  int raw = analogRead(BATT_PIN);
  float vOut = (raw / 4095.0) * 3.3;
  float vBatt = vOut * ((R1 + R2) / R2);
  int pct = map((int)(vBatt * 100), 320, 420, 0, 100);
  return (float)constrain(pct, 0, 100);
}

void connectWifi() {
  Serial.print("📡 WiFi: Connecting...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); 
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi OK");
}

void connectMqtt() {
  while (!mqtt.connected()) {
    Serial.print("🔗 MQTT: Connecting...");
    String clientId = String(DEVICE_ID) + "-" + String(random(0xffff), HEX);
    if (mqtt.connect(clientId.c_str())) {
      Serial.println("connected.");
    } else {
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  
  if (!bmp.begin()) Serial.println("⚠️ BMP sensor error!");
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RAIN_TRIG, OUTPUT);
  pinMode(RAIN_ECHO, INPUT);
  
  connectWifi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  lastRainCheck = millis();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWifi();
  if (!mqtt.connected()) connectMqtt();
  mqtt.loop();

  /* --- SENSOR READINGS --- */
  float temp = bmp.readTemperature();
  float pressure = bmp.readPressure() / 100.0;
  float riverDist = getDistance(TRIG_PIN, ECHO_PIN);
  float rainDist = getDistance(RAIN_TRIG, RAIN_ECHO);
  
  float currentRainLevel = (rainDist == -1.0f) ? lastRainLevel : (TUBE_HEIGHT - rainDist);
  unsigned long currentTime = millis();
  float timeDiffHours = (float)(currentTime - lastRainCheck) / 3600000.0f;
  float rainIntensity = (timeDiffHours > 0) ? (currentRainLevel - lastRainLevel) / timeDiffHours : 0;
  
  if (rainDist != -1.0f) {
    lastRainLevel = currentRainLevel;
    lastRainCheck = currentTime;
  }

  /* --- ARDUINOJSON V7 SYNTAX --- */
  JsonDocument doc; // V7 uses a single JsonDocument type
  doc["device_id"] = DEVICE_ID;
  doc["timestamp"] = millis();
  doc["temperature"] = round(temp * 100.0) / 100.0;
  doc["pressure"] = round(pressure * 100.0) / 100.0;
  doc["water_level_cm"] = (riverDist == -1.0f) ? -1 : round(riverDist * 100.0) / 100.0;
  doc["rainfall_intensity_mmh"] = (rainIntensity < 0) ? 0 : round(rainIntensity * 100.0) / 100.0;
  doc["flow_velocity_ms"] = 0.0; 

  // Modern V7 way to create nested objects
  JsonObject status = doc["device_status"].to<JsonObject>(); 
  status["battery_charge"] = readBatteryPercentage();
  status["signal_strength_dbm"] = WiFi.RSSI();

  char payload[512];
  serializeJson(doc, payload);
  
  if (mqtt.publish(MQTT_TOPIC, payload)) {
    Serial.println("📤 Published: " + String(payload));
  } else {
    Serial.println("❌ Publish Failed");
  }

  delay(2000);
}