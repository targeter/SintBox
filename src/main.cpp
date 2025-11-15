#include <Arduino.h>
#include <Servo.h>
#include <Adafruit_MCP23X17.h>
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
constexpr uint8_t PCF_ADDR = 0x24;
// MCP23017 address for puzzle status LEDs (A0,A1,A2=GND → 0x20, but use different address to avoid conflict)
// - A3..A7 control status LEDs for puzzles 0-4
constexpr uint8_t MCP_LED_ADDR = 0x20;

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

// Manager with MCP23017-based LED control
PuzzleManager<NUM_PUZZLES> manager(MCP_LED_ADDR, SERVO_PIN, LOCK_ANGLE, UNLOCK_ANGLE, true);

// Global flag to pause manager updates for LED testing
bool managerPaused = false;

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
      Serial.println(F("*** Testing MCP23017 LEDs ***"));
      manager.testLEDs();
    } else if (command == "LEDRAW") {
      Serial.println(F("*** Raw MCP23017 LED test ***"));
      Wire.begin();
      
      // Create temporary MCP23017 instance for raw testing
      Adafruit_MCP23X17 testMcp;
      if (!testMcp.begin_I2C(MCP_LED_ADDR)) {
        Serial.println(F("Failed to initialize MCP23017 for raw test"));
      } else {
        Serial.println(F("MCP23017 initialized for raw test"));
        
        // Configure pins A3-A7 as outputs
        for (uint8_t pin = 3; pin <= 7; pin++) {
          testMcp.pinMode(pin, OUTPUT);
        }
        
        // Test all LEDs OFF (HIGH for active LOW LEDs)
        Serial.println(F("All LEDs OFF"));
        for (uint8_t pin = 3; pin <= 7; pin++) {
          testMcp.digitalWrite(pin, HIGH);
        }
        delay(1000);
        
        // Test all LEDs ON (LOW for active LOW LEDs)
        Serial.println(F("All LEDs ON"));
        for (uint8_t pin = 3; pin <= 7; pin++) {
          testMcp.digitalWrite(pin, LOW);
        }
        delay(1000);
        
        // Test individual LEDs (pins A3-A7)
        for (uint8_t pin = 3; pin <= 7; pin++) {
          Serial.print(F("LED A"));
          Serial.print(pin);
          Serial.println(F(" ON"));
          
          // Turn all OFF first
          for (uint8_t p = 3; p <= 7; p++) {
            testMcp.digitalWrite(p, HIGH);
          }
          // Turn this one ON
          testMcp.digitalWrite(pin, LOW);
          delay(500);
        }
        
        // Turn all OFF again
        Serial.println(F("All LEDs OFF again"));
        for (uint8_t pin = 3; pin <= 7; pin++) {
          testMcp.digitalWrite(pin, HIGH);
        }
      }
    } else if (command == "ALLON") {
      Serial.println(F("*** Turning ALL MCP23017 pins ON ***"));
      Wire.begin();
      
      // Create temporary MCP23017 instance for testing all pins
      Adafruit_MCP23X17 testMcp;
      if (!testMcp.begin_I2C(MCP_LED_ADDR)) {
        Serial.println(F("Failed to initialize MCP23017"));
      } else {
        // Pause manager to prevent it from interfering with LED testing
        managerPaused = true;
        Serial.println(F("Manager paused for LED testing"));
        Serial.println(F("MCP23017 initialized - turning on ALL pins"));
        
        // Configure all 16 pins as outputs (Port A: 0-7, Port B: 8-15)
        for (uint8_t pin = 0; pin <= 15; pin++) {
          testMcp.pinMode(pin, OUTPUT);
          testMcp.digitalWrite(pin, LOW); // Active LOW: LOW = LED ON
          Serial.print(F("Pin "));
          if (pin <= 7) {
            Serial.print(F("A"));
            Serial.print(pin);
          } else {
            Serial.print(F("B"));
            Serial.print(pin - 8);
          }
          Serial.println(F(" ON"));
          delay(100); // Small delay to see the progression
        }
        Serial.println(F("All 16 MCP23017 pins are now ON (LOW)"));
        Serial.println(F("Use ALLOFF or RESUME to restore normal operation"));
      }
    } else if (command == "ALLOFF") {
      Serial.println(F("*** Turning ALL MCP23017 pins OFF ***"));
      Wire.begin();
      
      // Create temporary MCP23017 instance
      Adafruit_MCP23X17 testMcp;
      if (!testMcp.begin_I2C(MCP_LED_ADDR)) {
        Serial.println(F("Failed to initialize MCP23017"));
      } else {
        Serial.println(F("MCP23017 initialized - turning off ALL pins"));
        
        // Turn all 16 pins OFF
        for (uint8_t pin = 0; pin <= 15; pin++) {
          testMcp.pinMode(pin, OUTPUT);
          testMcp.digitalWrite(pin, HIGH); // Active LOW: HIGH = LED OFF
        }
        Serial.println(F("All 16 MCP23017 pins are now OFF (HIGH)"));
        Serial.println(F("Use RESUME to restore normal puzzle operation"));
      }
    } else if (command == "PAUSE") {
      managerPaused = true;
      Serial.println(F("*** Manager PAUSED - LED testing mode ***"));
      Serial.println(F("Puzzles will not update LEDs until RESUME"));
    } else if (command == "RESUME") {
      managerPaused = false;
      Serial.println(F("*** Manager RESUMED - Normal operation ***"));
      Serial.println(F("Puzzle LEDs will now reflect puzzle states"));
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
      Serial.println(F("  LEDTEST - Test MCP23017 status LEDs"));
      Serial.println(F("  LEDRAW  - Raw MCP23017 LED test"));
      Serial.println(F("  ALLON   - Turn ALL 16 MCP23017 pins ON (auto-pauses)"));
      Serial.println(F("  ALLOFF  - Turn ALL 16 MCP23017 pins OFF"));
      Serial.println(F("  PAUSE   - Pause manager (for LED testing)"));
      Serial.println(F("  RESUME  - Resume normal puzzle operation"));
      Serial.println(F("  I2CSCAN - Scan I2C bus for devices"));
      Serial.println(F("  HELP    - Show this help"));
    } else if (command.length() > 0) {
      Serial.print(F("Unknown command: "));
      Serial.print(command);
      Serial.println(F(" (type HELP for commands)"));
    }
  }
  
  // Only update manager if not paused (for LED testing)
  if (!managerPaused) {
    manager.update(now);
  }
  
  // add DFPlayer tick, other housekeeping here later
}