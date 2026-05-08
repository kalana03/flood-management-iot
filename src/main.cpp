#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

/* -----------------------------------------------
   CHANGE ONLY THESE THREE LINES
----------------------------------------------- */
const char *WIFI_SSID = "Wokwi-GUEST";
const char *WIFI_PASS = "";
const char *MQTT_BROKER = "broker.hivemq.com";
/* ----------------------------------------------- */

const int MQTT_PORT = 1883;
const char *MQTT_TOPIC = "flood/a1/sensor01";
const char *DEVICE_ID = "esp32-a1-01";

/* --- PIN CONFIGURATION (unchanged) --- */
const int SDA_PIN = 26;
const int SCL_PIN = 25;
const int TRIG_PIN = 5;
const int ECHO_PIN = 18;

/* --- OBJECTS --- */
Adafruit_BMP085 bmp;
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

void connectWifi()
{
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi OK: " + WiFi.localIP().toString());
}

void connectMqtt()
{
  while (!mqtt.connected())
  {
    Serial.print("Connecting to broker...");
    if (mqtt.connect(DEVICE_ID))
    {
      Serial.println("connected.");
    }
    else
    {
      Serial.print("failed rc=");
      Serial.println(mqtt.state());
      delay(3000);
    }
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  /* --- YOUR ORIGINAL SETUP (unchanged) --- */
  Serial.println("\n--- GROUP A1: SENSOR DIAGNOSTIC START ---");

  Wire.begin(SDA_PIN, SCL_PIN);
  if (!bmp.begin())
  {
    Serial.println("❌ BMP180 not found! Check pins 25(SCL) & 26(SDA).");
  }
  else
  {
    Serial.println("✅ BMP180 Initialized.");
  }

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  Serial.println("✅ HC-SR04 Pins Configured.");
  Serial.println("-----------------------------------------\n");
  /* --- END ORIGINAL SETUP --- */

  connectWifi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
    connectWifi();
  if (!mqtt.connected())
    connectMqtt();
  mqtt.loop();

  /* --- YOUR ORIGINAL SENSOR READING (unchanged) --- */
  float temp = bmp.readTemperature();
  float pressure = bmp.readPressure() / 100.0;

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH);
  float speedOfSound = 331.3 + (0.606 * temp);
  float distanceCm   = (duration / 2.0) * (speedOfSound / 10000.0);

  float speedOfSound = 331.3 + (0.606 * temp);
  float distanceCm = (duration / 2.0) * (speedOfSound / 10000.0);

  /* --- YOUR ORIGINAL SERIAL OUTPUT (unchanged) --- */
  Serial.print("🌡️ Temp: ");
  Serial.print(temp);
  Serial.print(" C | ");
  Serial.print("☁️ Press: ");
  Serial.print(pressure);
  Serial.print(" hPa | ");

  if (duration == 0)
  {
    Serial.println("📏 Distance: ERROR (No pulse)");
  }
  else
  {
    Serial.print("📏 Distance: ");
    Serial.print(distanceCm);
    Serial.println(" cm");
  }
  /* --- END ORIGINAL CODE --- */

  /* --- NEW: publish to MQTT --- */
  StaticJsonDocument<256> doc;
  doc["device_id"] = DEVICE_ID;
  doc["temp_c"] = round(temp * 10.0) / 10.0;
  doc["pressure_hpa"] = round(pressure * 10.0) / 10.0;
  doc["distance_cm"] = (duration == 0) ? -1 : round(distanceCm * 10.0) / 10.0;
  doc["rssi_dbm"] = WiFi.RSSI();
  doc["timestamp_ms"] = millis();

  char payload[256];
  serializeJson(doc, payload);

  bool ok = mqtt.publish(MQTT_TOPIC, payload);
  Serial.println(ok ? "Published OK:" : "Publish FAILED:");
  Serial.println(payload);

  delay(1500); // your original delay kept
}