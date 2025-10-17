#pragma once
#include <Arduino.h>
#include <Servo.h>
#include "Puzzle.h"

template<size_t N>
class PuzzleManager {
public:
  PuzzleManager(uint8_t const (&ledPins)[N], uint8_t servoPin, uint8_t lockedAngle=0, uint8_t unlockedAngle=90)
  : _servoPin(servoPin), _lockedAngle(lockedAngle), _unlockedAngle(unlockedAngle) {
    for (size_t i=0;i<N;i++) _ledPins[i]=ledPins[i];
  }

  void attach(Puzzle* const (&puzzles)[N]) {
    for (size_t i=0;i<N;i++) _puzzles[i]=puzzles[i];
  }

  void begin() {
    Serial.println(F("PuzzleManager: Initializing..."));
    for (size_t i=0;i<N;i++) {
      pinMode(_ledPins[i], OUTPUT);
      digitalWrite(_ledPins[i], LOW);
      Serial.print(F("  LED "));
      Serial.print(i);
      Serial.print(F(" on pin "));
      Serial.println(_ledPins[i]);
      
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
        analogWrite(_ledPins[i], (uint8_t)lvl);   // puzzle drives its own PWM
      } else {
        digitalWrite(_ledPins[i], s ? HIGH : LOW); // default on/off
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
  Puzzle* _puzzles[N]{};
  uint8_t _ledPins[N]{};
  uint8_t _servoPin;
  uint8_t _lockedAngle, _unlockedAngle;
  uint8_t _currentAngle = 255; // Invalid initial value to force first move
  bool _allSolved=false;
  Servo _servo;
};