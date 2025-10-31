#include <Arduino.h>
#include <Servo.h>
#include "PuzzleManager.h"
#include "SevenSegCodePuzzle.h"
#include "TiltButtonPuzzle.h"

// ---- Hardware mapping ----
// TM1637 on D2 (CLK) and D3 (DIO)
constexpr uint8_t TM_CLK = 2;
constexpr uint8_t TM_DIO = 3;
// PCF8574 address for puzzle controls (A0..A2 = GND → 0x20)
// - P0..P6 control 7-segment switches for digits 0-9
// - P7 is button input (to GND, uses internal pull-up)
constexpr uint8_t PCF_ADDR = 0x20;
// PCF8574 address for puzzle status LEDs (A0=VCC, A1,A2=GND → 0x21)
// - P0..P7 control status LEDs for puzzles 0-7
constexpr uint8_t PCF_LED_ADDR = 0x21;

// Correct code for this puzzle
constexpr int SAFE_CODE = 8888;

// Servo configuration
constexpr size_t NUM_PUZZLES = 2;        // bump to 3–4 as you add more
const uint8_t SERVO_PIN = 9;
const uint8_t LOCK_ANGLE = 0, UNLOCK_ANGLE = 140;

// Tilt button pin
constexpr uint8_t TILT_PIN = 4;

// Instantiate puzzles
SevenSegCodePuzzle pSafeDial(TM_CLK, TM_DIO, PCF_ADDR, SAFE_CODE);
TiltButtonPuzzle pTiltButton(TILT_PIN);

// Register them
Puzzle* puzzles[NUM_PUZZLES] = { &pSafeDial, &pTiltButton };

// Manager with PCF8574-based LED control
PuzzleManager<NUM_PUZZLES> manager(PCF_LED_ADDR, SERVO_PIN, LOCK_ANGLE, UNLOCK_ANGLE);

void setup() {
  Serial.begin(115200);
  delay(1000); // Give serial time to initialize
  Serial.println(F("*** SERIAL DEBUG TEST - SETUP STARTING ***"));
  Serial.println(F("=== SintBox Puzzle System Starting ==="));
  
  Serial.println(F("About to attach puzzles..."));
  manager.attach(puzzles);
  Serial.println(F("Puzzles attached, calling manager.begin()..."));
  manager.begin();
  Serial.println(F("manager.begin() completed!"));
  
  Serial.print(F("Initialized "));
  Serial.print(NUM_PUZZLES);
  Serial.println(F(" puzzle(s)"));
  Serial.println(F("System ready!"));
}

void loop() {
  uint32_t now = millis();
  
  // Handle serial commands
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toUpperCase();
    
    if (command.length() > 0) {
      Serial.print(F("> "));
      Serial.println(command);
    }
    
    if (command == "RESET") {
      Serial.println(F("*** Manual reset triggered from console ***"));
      manager.resetAll();
    } else if (command == "UNLOCK") {
      Serial.println(F("*** Manual unlock triggered from console ***"));
      manager.unlock();
    } else if (command == "LOCK") {
      Serial.println(F("*** Manual lock triggered from console ***"));
      manager.lock();
    } else if (command == "SERVO") {
      Serial.print(F("Servo diagnostics - Pin: "));
      Serial.print(SERVO_PIN);
      Serial.print(F(", Lock angle: "));
      Serial.print(LOCK_ANGLE);
      Serial.print(F(", Unlock angle: "));
      Serial.println(UNLOCK_ANGLE);
    } else if (command == "DETACH") {
      Serial.println(F("*** Detaching servo ***"));
      manager.detachServo();
    } else if (command == "ATTACH") {
      Serial.println(F("*** Attaching servo ***"));
      manager.attachServo();
    } else if (command == "STOP") {
      Serial.println(F("*** Trying to stop servo ticking ***"));
      manager.detachServo();
      // Wait a moment
      delay(100);
      // Try writing to the pin directly to stop any PWM
      pinMode(SERVO_PIN, OUTPUT);
      digitalWrite(SERVO_PIN, LOW);
      Serial.println(F("Servo pin set to LOW"));
    } else if (command == "STATUS") {
      Serial.print(F("Current status: "));
      if (manager.allSolved()) {
        Serial.println(F("ALL SOLVED! Box unlocked."));
      } else {
        Serial.println(F("Puzzles active..."));
      }
    } else if (command == "LEDTEST") {
      Serial.println(F("*** Testing PCF8574 LEDs ***"));
      manager.testLEDs();
    } else if (command == "LEDRAW") {
      Serial.println(F("*** Raw PCF8574 LED test ***"));
      Wire.begin();
      
      // Test all LEDs OFF (all pins HIGH for active LOW LEDs)
      Serial.println(F("All LEDs OFF (0xFF)"));
      Wire.beginTransmission(0x21);
      Wire.write(0xFF);
      uint8_t result = Wire.endTransmission();
      Serial.print(F("Result: "));
      Serial.println(result);
      delay(1000);
      
      // Test all LEDs ON (all pins LOW for active LOW LEDs)
      Serial.println(F("All LEDs ON (0x00)"));
      Wire.beginTransmission(0x21);
      Wire.write(0x00);
      result = Wire.endTransmission();
      Serial.print(F("Result: "));
      Serial.println(result);
      delay(1000);
      
      // Test individual LEDs
      for (int i = 0; i < 8; i++) {
        uint8_t pattern = ~(1 << i); // Invert for active LOW
        Serial.print(F("LED "));
        Serial.print(i);
        Serial.print(F(" ON (0x"));
        Serial.print(pattern, HEX);
        Serial.println(F(")"));
        Wire.beginTransmission(0x21);
        Wire.write(pattern);
        result = Wire.endTransmission();
        Serial.print(F("Result: "));
        Serial.println(result);
        delay(500);
      }
      
      // Turn all OFF again
      Serial.println(F("All LEDs OFF again"));
      Wire.beginTransmission(0x21);
      Wire.write(0xFF);
      Wire.endTransmission();
    } else if (command == "I2CSCAN") {
      Serial.println(F("*** Scanning I2C bus ***"));
      Wire.begin();
      byte error, address;
      int nDevices = 0;
      
      Serial.println(F("Scanning..."));
      for(address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        
        if (error == 0) {
          Serial.print(F("I2C device found at address 0x"));
          if (address < 16) Serial.print(F("0"));
          Serial.print(address, HEX);
          Serial.println(F(" !"));
          nDevices++;
        }
        else if (error == 4) {
          Serial.print(F("Unknown error at address 0x"));
          if (address < 16) Serial.print(F("0"));
          Serial.println(address, HEX);
        }
      }
      if (nDevices == 0) {
        Serial.println(F("No I2C devices found"));
      } else {
        Serial.print(nDevices);
        Serial.println(F(" device(s) found"));
      }
      Serial.println(F("I2C scan done"));
    } else if (command == "HELP") {
      Serial.println(F("Available commands:"));
      Serial.println(F("  RESET   - Reset all puzzles"));
      Serial.println(F("  UNLOCK  - Force unlock box"));
      Serial.println(F("  LOCK    - Force lock box"));
      Serial.println(F("  SERVO   - Show servo diagnostics"));
      Serial.println(F("  DETACH  - Detach servo (stops ticking)"));
      Serial.println(F("  ATTACH  - Re-attach servo"));
      Serial.println(F("  STOP    - Force stop servo completely"));
      Serial.println(F("  STATUS  - Show current status"));
      Serial.println(F("  LEDTEST - Test PCF8574 status LEDs"));
      Serial.println(F("  LEDRAW  - Raw PCF8574 LED test"));
      Serial.println(F("  I2CSCAN - Scan I2C bus for devices"));
      Serial.println(F("  HELP    - Show this help"));
    } else if (command.length() > 0) {
      Serial.print(F("Unknown command: "));
      Serial.print(command);
      Serial.println(F(" (type HELP for commands)"));
    }
  }
  
  manager.update(now);
  
  // add DFPlayer tick, other housekeeping here later
}