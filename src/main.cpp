#include <Wire.h>
#include <Adafruit_BMP085.h>

/* --- PIN CONFIGURATION --- */
// I2C Pins (BMP180)
const int SDA_PIN = 26;
const int SCL_PIN = 25;

// Ultrasonic Pins (HC-SR04)
const int TRIG_PIN = 5;
const int ECHO_PIN = 18;

/* --- OBJECTS & VARIABLES --- */
Adafruit_BMP085 bmp;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n--- GROUP A1: SENSOR DIAGNOSTIC START ---");

  // 1. Initialize I2C for BMP180
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!bmp.begin()) {
    Serial.println("❌ BMP180 not found! Check pins 25(SCL) & 26(SDA).");
  } else {
    Serial.println("✅ BMP180 Initialized.");
  }

  // 2. Initialize Ultrasonic Pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  Serial.println("✅ HC-SR04 Pins Configured.");
  
  Serial.println("-----------------------------------------\n");
}

void loop() {
  // BMP180 DATA ---
  float temp = bmp.readTemperature();
  float pressure = bmp.readPressure() / 100.0; //

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);

  // EDGE PROCESSING (Physics Calculation) ---
  float speedOfSound = 331.3 + (0.606 * temp); 
  float distanceCm = (duration / 2.0) * (speedOfSound / 10000.0);

  // SERIAL OUTPUT ---
  Serial.print("🌡️ Temp: "); Serial.print(temp); Serial.print(" C | ");
  Serial.print("☁️ Press: "); Serial.print(pressure); Serial.print(" hPa | ");
  
  if (duration == 0) {
    Serial.println("📏 Distance: ERROR (No pulse)");
  } else {
    Serial.print("📏 Distance: "); Serial.print(distanceCm); Serial.println(" cm");
  }

  delay(1500);
}


