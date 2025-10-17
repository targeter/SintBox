#pragma once
#include <Arduino.h>
#include "Puzzle.h"

// Minimal tilt-as-button puzzle that only starts fading AFTER it is solved.
// Wiring: one leg -> GND, other -> pin (INPUT_PULLUP). activeLow=true means LOW == closed (upside-down).
class TiltButtonPuzzle : public Puzzle {
public:
  TiltButtonPuzzle(uint8_t pin, bool activeLow = true,
                   uint16_t debounceMs = 30, uint16_t holdMs = 800)
  : _pin(pin), _activeLow(activeLow),
    _debounceMs(debounceMs), _holdMs(holdMs) {}

  // ---- Tunables for the fading trick ----
  uint16_t fadeDelayMs = 5000;   // NEW: wait this long after solved before starting to fade
  uint8_t  fadeStep    = 2;      // NEW: fade speed (PWM steps per update call). Increase for faster fade.

  void begin() override {
    pinMode(_pin, INPUT_PULLUP);
    _solved = false;
    uint32_t now = millis();
    _stable = _last = digitalRead(_pin);
    _tEdge = _tStartActive = now;

    // NEW: fade state reset
    _tSolved = 0;
    _fadeActive = false;
    _led = 0; // manager will do solid ON when solved (we only take over later)
  }

  void update(uint32_t now) override {
    if (!_solved) {
      // Debounce + hold
      int r = digitalRead(_pin);
      if (r != _last) { _last = r; _tEdge = now; }
      if ((now - _tEdge) >= _debounceMs && r != _stable) {
        _stable = r;
        _active = isActive(_stable);
        if (_active) _tStartActive = now;
      }
      if (_active && (now - _tStartActive) >= _holdMs) {
        _solved = true;
        _tSolved = now;          // NEW: mark solve time
        _fadeActive = false;     // NEW: not fading yet
        _led = 255;              // NEW: when we do start fading later, start from full
      }
    } else {
      // NEW: We’re solved. First act solid (manager drives LED),
      // then, AFTER fadeDelayMs, take over and start fading.
      if (!_fadeActive && (now - _tSolved >= fadeDelayMs)) {
        _fadeActive = true;
        // ensure we begin at full when taking over PWM
        _led = 255;
      }
      if (_fadeActive && _led > 0) {
        // simple linear fade down; you can do easing if you want
        _led = (_led > fadeStep) ? (_led - fadeStep) : 0;
      }
    }
  }

  bool isSolved() const override { return _solved; }

  void reset() override {
    _solved = false;
    uint32_t now = millis();
    _stable = _last = digitalRead(_pin);
    _tEdge = _tStartActive = now;

    // NEW: reset fade state
    _tSolved = 0;
    _fadeActive = false;
    _led = 0;
  }

  const __FlashStringHelper* name() const override { return F("Tilt Button"); }

  // NEW: Only drive PWM once we decide to fade; otherwise let manager do solid HIGH/LOW.
  int ledBrightness() const override {
    return _fadeActive ? _led : -1;  // -1 = manager handles it (solid ON when solved)
  }

private:
  bool isActive(int level) const { return _activeLow ? (level == LOW) : (level == HIGH); }

  // config
  uint8_t  _pin;  bool _activeLow;
  uint16_t _debounceMs, _holdMs;

  // runtime (button/tilt)
  bool     _solved = false, _active = false;
  int      _last = HIGH, _stable = HIGH;
  uint32_t _tEdge = 0, _tStartActive = 0;

  // NEW: fade control
  uint32_t _tSolved = 0;
  bool     _fadeActive = false;
  uint8_t  _led = 0; // 0..255 PWM level once fading starts
};