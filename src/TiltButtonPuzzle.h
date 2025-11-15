#pragma once
#include <Arduino.h>
#include "Puzzle.h"

// Tilt sensor puzzle - solved when tilt sensor is triggered for 10 seconds.
// LED: OFF when inactive, BLINKING during countdown, ON when solved.
// Wiring: one leg -> GND, other -> pin (INPUT_PULLUP). activeLow=false means HIGH == active (right-side up).
class TiltButtonPuzzle : public Puzzle {
public:
  TiltButtonPuzzle(uint8_t pin, bool activeLow = true,
                   uint16_t debounceMs = 30, uint16_t holdMs = 10000)
  : _pin(pin), _activeLow(activeLow),
    _debounceMs(debounceMs), _holdMs(holdMs) {}

  void begin() override {
    pinMode(_pin, INPUT_PULLUP);
    _solved = false;
    uint32_t now = millis();
    _last = digitalRead(_pin);
    _stable = !_last;  // Force initial state evaluation by making stable different
    _tEdge = now;
    _tStartActive = now;
    _lastCountdownOutput = 0;
    _active = false;
  }

  void update(uint32_t now) override {
    if (!_solved) {
      // Debounce + hold logic
      int r = digitalRead(_pin);
      if (r != _last) { 
        _last = r; 
        _tEdge = now;
      }
      
      if ((now - _tEdge) >= _debounceMs && r != _stable) {
        _stable = r;
        bool wasActive = _active;
        _active = isActive(_stable);
        
        if (_active && !wasActive) {
          // Just became active - start countdown
          _tStartActive = now;
          _lastCountdownOutput = 0;
          Serial.println(F("Tilt sensor activated! Hold for 10 seconds..."));
        } else if (!_active && wasActive) {
          // Just became inactive
          Serial.println(F("Tilt sensor deactivated"));
        }
      }
      
      // If active, show countdown and check completion
      if (_active) {
        uint32_t elapsed = now - _tStartActive;
        uint32_t remaining = (_holdMs > elapsed) ? (_holdMs - elapsed) : 0;
        
        // Show countdown every second
        uint32_t secondsRemaining = (remaining + 999) / 1000; // Round up
        if (secondsRemaining != _lastCountdownOutput && remaining > 0) {
          Serial.print(F("Tilt countdown: "));
          Serial.print(secondsRemaining);
          Serial.println(F(" seconds remaining"));
          _lastCountdownOutput = secondsRemaining;
        }
        
        // Check if held for required duration (10 seconds)
        if (elapsed >= _holdMs) {
          _solved = true;
          Serial.println(F("*** Tilt sensor puzzle SOLVED! ***"));
        }
      }
    }
  }

  bool isSolved() const override { return _solved; }

  void reset() override {
    _solved = false;
    uint32_t now = millis();
    _last = digitalRead(_pin);
    _stable = !_last;  // Force initial state evaluation
    _tEdge = now;
    _tStartActive = now;
    _lastCountdownOutput = 0;
    _active = false;
  }

  const __FlashStringHelper* name() const override { return F("Tilt Sensor"); }

  // LED control: blink when active (countdown running), solid when solved, off when inactive
  int ledBrightness() const override {
    if (_solved) {
      return -1;  // Let manager handle solid ON
    } else if (_active) {
      // Blink during countdown - use a simple on/off pattern based on time
      uint32_t blinkCycle = millis() / 250;  // 250ms on/off cycles = 2Hz blink
      return (blinkCycle % 2) ? 255 : 0;     // Alternate between full brightness and off
    } else {
      return 0;   // Off when inactive
    }
  }

private:
  bool isActive(int level) const { return _activeLow ? (level == LOW) : (level == HIGH); }

  // Configuration
  uint8_t  _pin;
  bool     _activeLow;
  uint16_t _debounceMs, _holdMs;

  // Runtime state
  bool     _solved = false, _active = false;
  int      _last = HIGH, _stable = HIGH;
  uint32_t _tEdge = 0, _tStartActive = 0;
  uint32_t _lastCountdownOutput = 0;
};