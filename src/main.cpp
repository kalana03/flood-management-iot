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
const int TRIG_PIN = 5;
const int ECHO_PIN = 18;

/* --- OBJECTS --- */
Adafruit_BMP085 bmp;
WiFiClient      wifiClient;
PubSubClient    mqtt(wifiClient);

void connectWifi() {
  Serial.print("📡 WiFi: Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); 
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi OK: Connected to Internet");
}

void connectMqtt() {
  while (!mqtt.connected()) {
    Serial.print("🔗 MQTT: Attempting connection to ");
    Serial.println(MQTT_BROKER);
    
    // Unique client ID to avoid collisions
    String clientId = String(DEVICE_ID) + "-" + String(random(0xffff), HEX);
    
    if (mqtt.connect(clientId.c_str())) {
      Serial.println("✅ MQTT: Connected to Broker");
    } else {
      Serial.print("❌ MQTT: Connection Failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" (Retrying in 3s...)");
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- 🌊 FLOOD NODE STARTING ---");
  
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!bmp.begin()) {
    Serial.println("⚠️ BMP180: Sensor not found! Check I2C wiring.");
  } else {
    Serial.println("✅ BMP180: Initialized");
  }
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  Serial.println("✅ HC-SR04: Ultrasonic pins configured");

  connectWifi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWifi();
  if (!mqtt.connected())             connectMqtt();
  mqtt.loop();

  /* --- SENSOR READINGS --- */
  float temp     = bmp.readTemperature();
  float pressure = bmp.readPressure() / 100.0;

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH);
  float speedOfSound = 331.3 + (0.606 * temp);
  float distanceCm   = (duration / 2.0) * (speedOfSound / 10000.0);

  /* --- FORMATTED JSON OUTPUT --- */
  StaticJsonDocument<512> doc;
  doc["device_id"]    = DEVICE_ID;
  doc["timestamp"]    = millis(); 
  doc["temperature"]  = round(temp * 100.0) / 100.0;
  doc["pressure"]     = round(pressure * 100.0) / 100.0;
  doc["water_level_cm"] = (duration == 0) ? -1 : round(distanceCm * 100.0) / 100.0;
  doc["rainfall_intensity_mmh"] = 0.0; 
  doc["flow_velocity_ms"]       = 0.0;

  JsonObject device_status = doc.createNestedObject("device_status");
  device_status["battery_charge"]      = 100;
  device_status["signal_strength_dbm"] = WiFi.RSSI();

  char payload[512];
  serializeJson(doc, payload);

  // Check if Publish was successful
  Serial.println("📤 Attempting to Publish Data...");
  if (mqtt.publish(MQTT_TOPIC, payload)) {
    Serial.println("✅ MQTT: Publish Successful");
  } else {
    Serial.println("❌ MQTT: Publish Failed (Packet too large or broker disconnected)");
  }
  
  Serial.print("📝 Payload: ");
  Serial.println(payload);
  Serial.println("--------------------------------");

  delay(2000);
}