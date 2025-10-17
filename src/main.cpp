#include <Arduino.h>
#include <Servo.h>
#include "PuzzleManager.h"
#include "SevenSegCodePuzzle.h"
#include "TiltButtonPuzzle.h"

// ---- Hardware mapping ----
// TM1637 on D2 (CLK) and D3 (DIO)
constexpr uint8_t TM_CLK = 2;
constexpr uint8_t TM_DIO = 3;
// PCF8574 address (A0..A2 = GND → 0x20)
// - P0..P6 control 7-segment switches for digits 0-9
// - P7 is button input (to GND, uses internal pull-up)
constexpr uint8_t PCF_ADDR = 0x20;

// Correct code for this puzzle
constexpr int SAFE_CODE = 5555;

// Servo + LED indicators
constexpr size_t NUM_PUZZLES = 1;        // bump to 3–4 as you add more
const uint8_t PUZZLE_LED_PINS[NUM_PUZZLES] = { 13 }; // LED for this puzzle
const uint8_t SERVO_PIN = 9;
const uint8_t LOCK_ANGLE = 0, UNLOCK_ANGLE = 140;

// Instantiate puzzles
SevenSegCodePuzzle pSafeDial(TM_CLK, TM_DIO, PCF_ADDR, SAFE_CODE);

// Register them
Puzzle* puzzles[NUM_PUZZLES] = { &pSafeDial  };

// Manager
PuzzleManager<NUM_PUZZLES> manager(PUZZLE_LED_PINS, SERVO_PIN, LOCK_ANGLE, UNLOCK_ANGLE);

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
    } else if (command == "HELP") {
      Serial.println(F("Available commands:"));
      Serial.println(F("  RESET  - Reset all puzzles"));
      Serial.println(F("  UNLOCK - Force unlock box"));
      Serial.println(F("  LOCK   - Force lock box"));
      Serial.println(F("  SERVO  - Show servo diagnostics"));
      Serial.println(F("  DETACH - Detach servo (stops ticking)"));
      Serial.println(F("  ATTACH - Re-attach servo"));
      Serial.println(F("  STOP   - Force stop servo completely"));
      Serial.println(F("  STATUS - Show current status"));
      Serial.println(F("  HELP   - Show this help"));
    } else if (command.length() > 0) {
      Serial.print(F("Unknown command: "));
      Serial.print(command);
      Serial.println(F(" (type HELP for commands)"));
    }
  }
  
  manager.update(now);
  
  // add DFPlayer tick, other housekeeping here later
}