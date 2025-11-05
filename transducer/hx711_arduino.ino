/**
 * HX711 Force Transducer Interface for Pressboi
 * 
 * Reads HX711 load cell amplifier and sends force data to ClearCore via serial.
 * 
 * Hardware:
 * - Ruggeduino-SE ST (Screw Terminal version - ruggedized Arduino Uno)
 * - HX711 load cell amplifier module
 * - Load cell (configured for force measurement)
 * 
 * Connections (using screw terminals):
 * HX711 -> Ruggeduino-SE:
 *   VCC  -> 5V screw terminal
 *   GND  -> GND screw terminal
 *   DOUT -> D3 screw terminal
 *   SCK  -> D2 screw terminal
 *   RATE -> 5V screw terminal (enables 80Hz mode; leave disconnected for 10Hz)
 * 
 * Ruggeduino-SE -> ClearCore COM-0 (TTL mode):
 *   TX  -> COM-0 Pin 8 (Data Out / COM0 RX)
 *   RX  -> COM-0 Pin 5 (Data In / COM0 TX)
 *   GND -> COM-0 Pin 4 (GND)
 *   VIN or 5V -> COM-0 Pin 6 (5V Power) - ClearCore can power the Ruggeduino!
 * 
 * Serial Protocol:
 * - Baud: 115200
 * - Output: Force in kg as floating point (e.g., "12.34\n")
 * - Update rate: 80Hz (synchronized with HX711 samples, no duplicates/missed values)
 * - Commands from ClearCore:
 *   'T' or 't' -> Tare/zero the scale
 *   'C' or 'c' -> Calibrate (future feature)
 */

 #include "HX711.h"

// Pin definitions
#define DOUT_PIN 3
#define SCK_PIN 2

// HX711 configuration
HX711 scale;

// Calibration factor - ADJUST THIS AFTER TESTING WITH KNOWN WEIGHTS
// This converts the raw ADC value to kg
// Procedure:
// 1. Upload this code with calibration_factor = 1.0
// 2. Place a known weight (e.g., 10 kg) on the load cell
// 3. Read the output value
// 4. calibration_factor = raw_reading / known_weight_kg
// 5. Update this value and re-upload
float calibration_factor = -4333.33;  // Calibrated: -52000 raw reading = 12.0 kg

// NOTE: For 80Hz operation, connect HX711 RATE pin to VCC
// RATE pin LOW/disconnected = 10 Hz, RATE pin HIGH = 80 Hz
 
 void setup() {
   // Initialize serial communication with ClearCore
   Serial.begin(115200);
   
   // Initialize HX711
   scale.begin(DOUT_PIN, SCK_PIN);
   scale.set_scale(calibration_factor);
   
   // Wait for scale to stabilize
   delay(1000);
   
   // Tare/zero the scale on startup
   scale.tare();
   
   // Send startup message
   Serial.println("# HX711 Force Sensor Ready");
   Serial.print("# Calibration Factor: ");
   Serial.println(calibration_factor, 4);
 }
 
void loop() {
  // Check for commands from ClearCore
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    handleCommand(cmd);
  }
  
  // Send force reading immediately when new data is available
  // This synchronizes serial output with HX711 updates (80Hz if RATE pin = VCC)
  if (scale.is_ready()) {
    // Read force in kg
    float force_kg = scale.get_units();
    
    // Send to ClearCore
    // Format: Simple floating point value
    Serial.println(force_kg, 2);  // 2 decimal places
    
    // Alternative format with more detail (comment out above, uncomment below):
    // Serial.print("FORCE:");
    // Serial.print(force_kg, 2);
    // Serial.print(",RAW:");
    // Serial.println(scale.get_value());
  }
}
 
 void handleCommand(char cmd) {
   switch (cmd) {
     case 'T':
     case 't':
       // Tare/zero the scale
       scale.tare();
       Serial.println("# Tared");
       break;
       
     case 'C':
     case 'c':
       // Calibration mode (future feature)
       Serial.println("# Calibration not implemented");
       break;
       
     case 'R':
     case 'r':
       // Get raw ADC value
       Serial.print("# RAW: ");
       Serial.println(scale.get_value());
       break;
       
     default:
       // Unknown command
       break;
   }
 }
 
 