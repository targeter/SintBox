#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Servo.h>
#include "Puzzle.h"

template<size_t N>
class PuzzleManager {
public:
  // Constructor for Arduino pin-based LEDs
  PuzzleManager(uint8_t const (&ledPins)[N], uint8_t servoPin, uint8_t lockedAngle=0, uint8_t unlockedAngle=90)
  : _servoPin(servoPin), _lockedAngle(lockedAngle), _unlockedAngle(unlockedAngle), _usePCF8574(false) {
    for (size_t i=0;i<N;i++) _ledPins[i]=ledPins[i];
  }
  
  // Constructor for PCF8574-based LEDs
  PuzzleManager(uint8_t pcfAddr, uint8_t servoPin, uint8_t lockedAngle=0, uint8_t unlockedAngle=90)
  : _servoPin(servoPin), _lockedAngle(lockedAngle), _unlockedAngle(unlockedAngle), _usePCF8574(true), _pcfAddr(pcfAddr) {
    // LEDs will be on PCF8574 pins P0..P7
  }

  void attach(Puzzle* const (&puzzles)[N]) {
    for (size_t i=0;i<N;i++) _puzzles[i]=puzzles[i];
  }

  void begin() {
    Serial.println(F("PuzzleManager: Initializing..."));
    
    if (_usePCF8574) {
      Wire.begin();
      Serial.print(F("  PCF8574 LED controller at address 0x"));
      Serial.print(_pcfAddr, HEX);
      Serial.println();
      
      // Initialize all LEDs to OFF (active LOW: HIGH = OFF)
      _pcfLedState = 0xFF;
      updatePCF8574();
    }
    
    for (size_t i=0;i<N;i++) {
      if (!_usePCF8574) {
        pinMode(_ledPins[i], OUTPUT);
        digitalWrite(_ledPins[i], LOW);
        Serial.print(F("  LED "));
        Serial.print(i);
        Serial.print(F(" on pin "));
        Serial.println(_ledPins[i]);
      } else {
        Serial.print(F("  LED "));
        Serial.print(i);
        Serial.print(F(" on PCF8574 pin P"));
        Serial.println(i);
      }
      
      _puzzles[i]->begin();
      Serial.print(F("  Puzzle "));
      Serial.print(i);
      Serial.print(F(" ("));
      Serial.print(_puzzles[i]->name());
      Serial.println(F(") initialized"));
    }
    Serial.print(F("  Attaching servo to pin "));
    Serial.print(_servoPin);
    Serial.print(F(" (lock="));
    Serial.print(_lockedAngle);
    Serial.print(F(", unlock="));
    Serial.print(_unlockedAngle);
    Serial.println(F(")"));
    
    _servo.attach(_servoPin);
    Serial.println(F("  Servo attached successfully"));
    
    lock();
    Serial.println(F("PuzzleManager: Box locked, ready!"));
  }

  void update(uint32_t now) {
    
    uint8_t solvedCount=0;
    for (size_t i=0;i<N;i++) {
      _puzzles[i]->update(now);
      const bool s = _puzzles[i]->isSolved();

      // Debug puzzle state changes
      static bool prevSolved[N] = {};
      if (s != prevSolved[i]) {
        Serial.print(F("Puzzle "));
        Serial.print(i);
        Serial.print(F(" ("));
        Serial.print(_puzzles[i]->name());
        Serial.print(F("): "));
        Serial.println(s ? F("SOLVED!") : F("Reset"));
        prevSolved[i] = s;
      }

      const int lvl = _puzzles[i]->ledBrightness();
      if (lvl >= 0) {
        // Puzzle wants to control its own PWM
        if (!_usePCF8574) {
          analogWrite(_ledPins[i], (uint8_t)lvl);
        } else {
          // PCF8574 doesn't support PWM, use simple on/off based on threshold
          setLED(i, lvl > 127);
        }
      } else {
        // Default on/off based on solved state
        if (!_usePCF8574) {
          digitalWrite(_ledPins[i], s ? HIGH : LOW);
        } else {
          setLED(i, s);
        }
      }

      if (s) solvedCount++;
    }
    if (!_allSolved && solvedCount==N) {
      _allSolved = true;
      Serial.println(F("*** ALL PUZZLES SOLVED - UNLOCKING! ***"));
      unlock();
    }
  }

  void resetAll() {
    Serial.println(F("PuzzleManager: Resetting all puzzles..."));
    for (size_t i=0;i<N;i++) _puzzles[i]->reset();
    _allSolved=false;
    lock();
    Serial.println(F("PuzzleManager: All puzzles reset, box locked"));
  }

  bool allSolved() const { return _allSolved; }

  void detachServo() {
    Serial.println(F("Servo: Detaching servo (stops ticking)"));
    _servo.detach();
  }
  
  void attachServo() {
    Serial.println(F("Servo: Re-attaching servo"));
    _servo.attach(_servoPin);
  }

  void testLEDs() {
    if (!_usePCF8574) {
      Serial.println(F("LED test not available - using Arduino pins"));
      return;
    }
    
    Serial.println(F("Testing PCF8574 LEDs..."));
    
    // Test each LED individually up to the number of puzzles we have
    for (size_t i = 0; i < N; i++) {
      Serial.print(F("  Testing puzzle LED "));
      Serial.print(i);
      Serial.print(F(" (P"));
      Serial.print(i);
      Serial.println(F(")"));
      setLED(i, true);
      delay(500);
      setLED(i, false);
      delay(200);
    }
    
    // Test all puzzle LEDs on
    Serial.print(F("  All "));
    Serial.print(N);
    Serial.println(F(" puzzle LEDs ON"));
    for (size_t i = 0; i < N; i++) {
      setLED(i, true);
    }
    delay(1000);
    
    // Test all puzzle LEDs off
    Serial.println(F("  All puzzle LEDs OFF"));
    for (size_t i = 0; i < N; i++) {
      setLED(i, false);
    }
    
    Serial.println(F("LED test complete"));
  }

  void lock() { 
    Serial.print(F("DEBUG: lock() called at "));
    Serial.print(millis());
    Serial.println(F("ms"));
    if (_currentAngle != _lockedAngle) {
      Serial.print(F("Servo: Moving to LOCK position "));
      Serial.print(_lockedAngle);
      Serial.print(F(" at "));
      Serial.print(millis());
      Serial.println(F("ms"));
      _servo.write(_lockedAngle);
      _currentAngle = _lockedAngle;
      delay(500); // Give servo time to move
      Serial.println(F("Servo: Lock position reached"));
    } else {
      Serial.print(F("Servo: Already at LOCK position ("));
      Serial.print(_lockedAngle);
      Serial.println(F(")"));
    }
  }
  
  void unlock() { 
    Serial.print(F("DEBUG: unlock() called at "));
    Serial.print(millis());
    Serial.println(F("ms"));
    if (_currentAngle != _unlockedAngle) {
      Serial.print(F("Servo: Moving to UNLOCK position "));
      Serial.print(_unlockedAngle);
      Serial.print(F(" at "));
      Serial.print(millis());
      Serial.println(F("ms"));
      _servo.write(_unlockedAngle);
      _currentAngle = _unlockedAngle;
      delay(500); // Give servo time to move
      Serial.println(F("Servo: Unlock position reached"));
    } else {
      Serial.print(F("Servo: Already at UNLOCK position ("));
      Serial.print(_unlockedAngle);
      Serial.println(F(")"));
    }
  }

private:
  void setLED(size_t index, bool state) {
    if (!_usePCF8574) return; // Only for PCF8574 mode
    if (index >= 8) return;   // PCF8574 only has 8 pins
    
    // Invert logic for active LOW LEDs (cathode connected to PCF8574)
    if (state) {
      _pcfLedState &= ~(1 << index);  // Clear bit (LOW = LED ON)
    } else {
      _pcfLedState |= (1 << index);   // Set bit (HIGH = LED OFF)
    }
    updatePCF8574();
  }
  
  void updatePCF8574() {
    if (!_usePCF8574) return;
    
    Wire.beginTransmission(_pcfAddr);
    Wire.write(_pcfLedState);
    uint8_t result = Wire.endTransmission();
    
    if (result != 0) {
      Serial.print(F("PCF8574 LED update failed: "));
      Serial.println(result);
    }
  }

private:
  Puzzle* _puzzles[N]{};
  uint8_t _ledPins[N]{};
  uint8_t _servoPin;
  uint8_t _lockedAngle, _unlockedAngle;
  uint8_t _currentAngle = 255; // Invalid initial value to force first move
  bool _allSolved=false;
  Servo _servo;
  
  // PCF8574 LED support
  bool _usePCF8574;
  uint8_t _pcfAddr;
  uint8_t _pcfLedState = 0xFF; // Active LOW: start with all LEDs OFF
};