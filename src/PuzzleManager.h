#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Servo.h>
#include <Adafruit_MCP23X17.h>
#include "Puzzle.h"

template<size_t N>
class PuzzleManager {
public:
  // Constructor for MCP23017-based puzzle status LEDs and servo control
  // Uses pins A3-A7 for 5 puzzle status LEDs, remaining pins available for future puzzles
  PuzzleManager(uint8_t mcpAddr, uint8_t servoPin, uint8_t lockedAngle, uint8_t unlockedAngle, bool useMCP23017)
  : _servoPin(servoPin), _lockedAngle(lockedAngle), _unlockedAngle(unlockedAngle), _mcpAddr(mcpAddr) {
    static_assert(N <= 5, "Maximum 5 puzzles supported (MCP23017 pins A3-A7)");
  }

  void attach(Puzzle* const (&puzzles)[N]) {
    for (size_t i=0;i<N;i++) _puzzles[i]=puzzles[i];
  }

  void begin() {
    Serial.println(F("PuzzleManager: Initializing..."));
    
    Wire.begin();
    Serial.print(F("  MCP23017 at address 0x"));
    Serial.print(_mcpAddr, HEX);
    Serial.println();
    
    if (!_mcp.begin_I2C(_mcpAddr)) {
      Serial.println(F("  ERROR: Failed to initialize MCP23017!"));
      return;
    }
    
    // Configure pins A3-A7 as outputs for puzzle status LEDs
    for (uint8_t pin = 3; pin <= 7; pin++) {
      _mcp.pinMode(pin, OUTPUT);
      _mcp.digitalWrite(pin, HIGH);  // Active LOW: HIGH = LED OFF
    }
    Serial.println(F("  MCP23017 pins A3-A7 configured for puzzle LEDs"));
    
    // Initialize puzzles
    for (size_t i = 0; i < N; i++) {
      _puzzles[i]->begin();
      Serial.print(F("  Puzzle "));
      Serial.print(i);
      Serial.print(F(" ("));
      Serial.print(_puzzles[i]->name());
      Serial.print(F(") -> LED A"));
      Serial.println(i + 3);
    }
    
    // Initialize servo
    Serial.print(F("  Servo on pin "));
    Serial.print(_servoPin);
    Serial.print(F(" (lock="));
    Serial.print(_lockedAngle);
    Serial.print(F(", unlock="));
    Serial.print(_unlockedAngle);
    Serial.println(F(")"));
    
    _servo.attach(_servoPin);
    lock();
    Serial.println(F("PuzzleManager: System ready!"));
  }

  void update(uint32_t now) {
    uint8_t solvedCount = 0;
    
    for (size_t i = 0; i < N; i++) {
      _puzzles[i]->update(now);
      const bool solved = _puzzles[i]->isSolved();

      // Debug puzzle state changes
      static bool prevSolved[N] = {};
      if (solved != prevSolved[i]) {
        Serial.print(F("Puzzle "));
        Serial.print(i);
        Serial.print(F(" ("));
        Serial.print(_puzzles[i]->name());
        Serial.print(F("): "));
        Serial.println(solved ? F("SOLVED!") : F("Reset"));
        prevSolved[i] = solved;
      }

      // Update puzzle LED - check if puzzle wants custom control
      const int brightness = _puzzles[i]->ledBrightness();
      if (brightness >= 0) {
        // Puzzle controls its own LED brightness
        setLED(i, brightness > 127);  // Convert PWM to on/off for MCP23017
      } else {
        // Default: LED on when solved
        setLED(i, solved);
      }

      if (solved) solvedCount++;
    }
    
    // Check if all puzzles are solved
    if (!_allSolved && solvedCount == N) {
      _allSolved = true;
      Serial.println(F("*** ALL PUZZLES SOLVED - UNLOCKING BOX! ***"));
      unlock();
    }
  }

  void resetAll() {
    Serial.println(F("*** Resetting all puzzles ***"));
    _allSolved = false;
    for (size_t i = 0; i < N; i++) {
      _puzzles[i]->reset();
      setLED(i, false);
    }
    lock();
    Serial.println(F("All puzzles reset, box locked"));
  }

  bool allSolved() const { return _allSolved; }

  void testLEDs() {
    Serial.println(F("Testing puzzle status LEDs..."));
    
    // Test individual LEDs
    for (size_t i = 0; i < N; i++) {
      Serial.print(F("  LED "));
      Serial.print(i);
      Serial.print(F(" ("));
      Serial.print(_puzzles[i]->name());
      Serial.println(F(") ON"));
      setLED(i, true);
      delay(500);
      
      Serial.print(F("  LED "));
      Serial.print(i);
      Serial.println(F(" OFF"));
      setLED(i, false);
      delay(300);
    }
    
    // Test all LEDs on
    Serial.print(F("  All "));
    Serial.print(N);
    Serial.println(F(" LEDs ON"));
    for (size_t i = 0; i < N; i++) {
      setLED(i, true);
    }
    delay(1000);
    
    // Test all LEDs off
    Serial.println(F("  All LEDs OFF"));
    for (size_t i = 0; i < N; i++) {
      setLED(i, false);
    }
    
    Serial.println(F("LED test complete"));
  }

  void lock() { 
    if (_currentAngle != _lockedAngle) {
      Serial.print(F("Locking box (servo angle "));
      Serial.print(_lockedAngle);
      Serial.println(F(")"));
      _servo.write(_lockedAngle);
      _currentAngle = _lockedAngle;
      delay(500);
    }
  }
  
  void unlock() { 
    if (_currentAngle != _unlockedAngle) {
      Serial.print(F("Unlocking box (servo angle "));
      Serial.print(_unlockedAngle);
      Serial.println(F(")"));
      _servo.write(_unlockedAngle);
      _currentAngle = _unlockedAngle;
      delay(500);
    }
  }

private:
  void setLED(size_t index, bool state) {
    if (index >= 5) return;   // Only 5 LEDs supported (A3-A7)
    
    uint8_t pin = index + 3;  // Map puzzle index 0-4 to MCP pins A3-A7
    // Active LOW LEDs: LOW = LED ON, HIGH = LED OFF
    _mcp.digitalWrite(pin, state ? LOW : HIGH);
  }

private:
  Puzzle* _puzzles[N]{};
  uint8_t _servoPin;
  uint8_t _lockedAngle, _unlockedAngle;
  uint8_t _currentAngle = 255; // Invalid initial value to force first move
  bool _allSolved = false;
  Servo _servo;
  
  // MCP23017 for puzzle status LEDs and future puzzle I/O
  uint8_t _mcpAddr;
  Adafruit_MCP23X17 _mcp;
};