#include <Arduino.h>
#include <Servo.h>
#include <Adafruit_MCP23X17.h>
#include "PuzzleManager.h"
#include "SevenSegCodePuzzle.h"
#include "TiltButtonPuzzle.h"
#include "SimonSaysPuzzle.h"
#include "NFCAmiiboPuzzle.h"

// ---- Hardware Configuration ----
// 7-Segment Display (TM1637)
constexpr uint8_t TM_CLK = 2;           // TM1637 CLK pin
constexpr uint8_t TM_DIO = 3;           // TM1637 DIO pin

// I2C Addresses
constexpr uint8_t PCF_ADDR = 0x25;      // PCF8574 for 7-segment switches (P0-P6: segments, P7: button)
constexpr uint8_t MCP_LED_ADDR = 0x20;  // MCP23017 for puzzle status LEDs (A3-A7) AND Simon Says (B0-B7)

// Puzzle Configuration
constexpr int SAFE_CODE = 8888;         // Correct code for 7-segment puzzle
constexpr size_t NUM_PUZZLES = 4;       // 7-segment + tilt sensor + simon says + NFC goomba

// Servo Configuration
const uint8_t SERVO_PIN = 9;
const uint8_t LOCK_ANGLE = 0;
const uint8_t UNLOCK_ANGLE = 140;

// Tilt Sensor Configuration
constexpr uint8_t TILT_PIN = 4;         // Tilt sensor digital input pin

// Simon Says Configuration
constexpr uint8_t BUZZER_PIN = 5;       // Passive buzzer for Simon Says

// NFC Configuration
constexpr uint8_t NFC_IRQ_PIN = 6;      // PN532 IRQ pin
constexpr uint8_t NFC_RESET_PIN = 7;    // PN532 Reset pin

// Puzzle Instances
SevenSegCodePuzzle sevenSegPuzzle(TM_CLK, TM_DIO, PCF_ADDR, SAFE_CODE);
TiltButtonPuzzle tiltPuzzle(TILT_PIN, false, 100, 10000);  // activeLow=false, debounce=100ms, hold=10s
SimonSaysPuzzle simonPuzzle(nullptr, BUZZER_PIN);          // MCP will be provided after manager initialization
NFCAmiiboPuzzle nfcPuzzle(NFC_IRQ_PIN, NFC_RESET_PIN);     // Goomba amiibo recognition

// Puzzle Array (order determines LED assignment on MCP23017: A3, A4, A5, A6...)
Puzzle* puzzles[NUM_PUZZLES] = { &sevenSegPuzzle, &tiltPuzzle, &simonPuzzle, &nfcPuzzle };

// Puzzle Manager with MCP23017-based LED control and servo
PuzzleManager<NUM_PUZZLES> manager(MCP_LED_ADDR, SERVO_PIN, LOCK_ANGLE, UNLOCK_ANGLE, true);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("=== SintBox Puzzle System Starting ==="));
  
  manager.attach(puzzles);
  manager.begin();
  
  // Provide MCP reference to Simon Says puzzle after manager initializes it
  Serial.print(F("Setting Simon Says MCP: "));
  Adafruit_MCP23X17* mcpPtr = manager.getMCP();
  Serial.println(mcpPtr != nullptr ? F("Valid pointer") : F("NULL pointer"));
  simonPuzzle.setMCP(mcpPtr);
  
  // Manually call Simon Says begin() now that MCP is available
  Serial.println(F("Calling Simon Says begin() with MCP available"));
  simonPuzzle.begin();
  
  Serial.print(F("Initialized "));
  Serial.print(NUM_PUZZLES);
  Serial.println(F(" puzzles:"));
  Serial.println(F("  1. 7-Segment Code Entry"));
  Serial.println(F("  2. Tilt Sensor Hold"));
  Serial.println(F("  3. Simon Says Melodies"));
  Serial.println(F("  4. Goomba Amiibo Recognition"));
  Serial.println(F("Commands: RESET, UNLOCK, LOCK, STATUS, LEDTEST, SIMONTEST"));
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
      Serial.println(F("*** Manual reset triggered ***"));
      manager.resetAll();
    } else if (command == "UNLOCK") {
      Serial.println(F("*** Manual unlock triggered ***"));
      manager.unlock();
    } else if (command == "LOCK") {
      Serial.println(F("*** Manual lock triggered ***"));
      manager.lock();
    } else if (command == "STATUS") {
      Serial.print(F("System status: "));
      if (manager.allSolved()) {
        Serial.println(F("ALL SOLVED! Box unlocked."));
      } else {
        Serial.println(F("Puzzles in progress..."));
        for (size_t i = 0; i < NUM_PUZZLES; i++) {
          Serial.print(F("  Puzzle "));
          Serial.print(i);
          Serial.print(F(" ("));
          Serial.print(puzzles[i]->name());
          Serial.print(F("): "));
          Serial.println(puzzles[i]->isSolved() ? F("SOLVED") : F("Active"));
        }
      }
    } else if (command == "LEDTEST") {
      Serial.println(F("*** Testing puzzle status LEDs ***"));
      manager.testLEDs();
    } else if (command == "SIMONTEST") {
      Serial.println(F("*** Testing Simon Says LEDs ***"));
      simonPuzzle.testLEDs();
    } else if (command.length() > 0) {
      Serial.print(F("Unknown command: "));
      Serial.println(command);
      Serial.println(F("Available: RESET, UNLOCK, LOCK, STATUS, LEDTEST, SIMONTEST"));
    }
  }
  
  // Update puzzle manager
  manager.update(now);
}