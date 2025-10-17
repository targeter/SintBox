#pragma once
#include <Arduino.h>

class Puzzle {
public:
  virtual ~Puzzle() {}
  virtual void begin() = 0;                  // pinModes, init libs
  virtual void update(uint32_t now) = 0;     // non-blocking tick
  virtual bool isSolved() const = 0;         // sticky true after solved
  virtual void reset() = 0;                  // reset this puzzle only
  virtual const __FlashStringHelper* name() const = 0;
  // optional PWM level for the puzzle's LED (0..255). Return <0 to let the manager do HIGH/LOW.
  virtual int ledBrightness() const { return -1; }
};