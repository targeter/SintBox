#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <TM1637Display.h>
#include "Puzzle.h"

// Calculator-style code entry with rightmost "cursor" driven by 7 toggles on a PCF8574.
// - P0..P6 control 7-segment display segments a..g
// - P7 is the button input (to GND, with internal pull-up)
// - Cursor always shows live segments on rightmost digit
// - Button press: shift stored digits left, stuff snapshot into the stored tail (visual "double")
// - On 4th press: evaluate code; success -> celebrate + lock solid, failure -> angry flash + 0000 + reset
class SevenSegCodePuzzle : public Puzzle {
public:
  SevenSegCodePuzzle(
    // TM1637 pins
    uint8_t pinCLK, uint8_t pinDIO,
    // PCF8574 I2C address (e.g., 0x20 with A0/A1/A2 to GND)
    uint8_t pcfAddr,
    // Correct 4-digit code
    int correctCode
  )
  : _display(pinCLK, pinDIO), _pcfAddr(pcfAddr), _correct(correctCode) {}

  void begin() override {
    Serial.println(F("SevenSegCodePuzzle: Starting initialization..."));
    
    Wire.begin();
    Serial.println(F("  Wire initialized"));
    
    // PCF8574 inputs with pull-ups
    Wire.beginTransmission(_pcfAddr); 
    Wire.write(0xFF); 
    uint8_t result = Wire.endTransmission();
    Serial.print(F("  PCF8574 setup (addr=0x"));
    Serial.print(_pcfAddr, HEX);
    Serial.print(F("), result="));
    Serial.println(result);

    _display.setBrightness(7,true);
    _display.clear();
    Serial.println(F("  TM1637 display initialized"));

    // reset model
    _stored[0]=_stored[1]=_stored[2]=-1;
    _nStored=0;
    _cursorOn=true;
    _lastCursorBlink=millis();
    _state = State::PREVIEW;
    _solved=false;
    
    Serial.println(F("SevenSegCodePuzzle: Initialization complete"));
  }

  void update(uint32_t now) override {
    if (_state == State::LOCKED) { return; } // solid display, ignore input when solved

    const uint8_t btn = readButtonState();
    const uint8_t liveMask = readSwitchSegments();

    // edge-debounce for button (falling edge)
    if (btn != _lastBtn) { _lastChange = now; _lastBtn = btn; }
    if ((now - _lastChange) >= DEBOUNCE_MS && btn != _stableBtn) {
      _stableBtn = btn;
      if (_stableBtn == LOW && _state == State::PREVIEW) {
        _snapshotMask = liveMask;
        _state = State::VALIDATE;
        _stateSince = now;
      }
    }

    // cursor blink
    if (_state == State::PREVIEW && (now - _lastCursorBlink) >= cursorBlinkMs) {
      _cursorOn = !_cursorOn;
      _lastCursorBlink = now;
    }

    switch (_state) {
      case State::PREVIEW:
        renderPreview(liveMask, _cursorOn);
        break;

      case State::VALIDATE: {
        uint8_t d;
        if (maskToDigit(_snapshotMask, d)) {
          if (_nStored < 3) {
            // accept: shift stored left, drop snapshot at end
            _stored[0] = _stored[1];
            _stored[1] = _stored[2];
            _stored[2] = (int8_t)d;
            _nStored++;
            _cursorOn = true;
            renderPreview(liveMask, true);
            _state = State::PREVIEW;
          } else {
            // 4th accept: evaluate code
            const int code = buildCodeWith(d);
            int8_t digits[4]; getCodeDigits(d, digits);

            if (code == _correct) {
              celebrateSuccessAndLock(digits); // sets _state=LOCKED
              _solved = true;
            } else {
              failureRitualAndReset(digits, liveMask);
            }
          }
        } else {
          // invalid -> blink rightmost off once
          uint8_t out[4] = {0,0,0,0};
          for (uint8_t i=0;i<3;i++) if (_stored[i] >= 0) out[i] = _display.encodeDigit(_stored[i]);
          out[3] = 0x00;
          _display.setSegments(out);
          _state = State::INVALID_BLINK;
          _stateSince = now;
        }
      } break;

      case State::INVALID_BLINK:
        if (now - _stateSince >= INVALID_BLINK_MS) {
          _cursorOn = true;
          _lastCursorBlink = now;
          renderPreview(liveMask, true);
          _state = State::PREVIEW;
        }
        break;

      case State::LOCKED:
        // nothing
        break;
    }
  }

  bool isSolved() const override { return _solved; }

  void reset() override {
    // Only reset if not locked (manager may call this)
    _stored[0]=_stored[1]=_stored[2]=-1;
    _nStored=0;
    _cursorOn=true;
    _lastCursorBlink=millis();
    _display.setBrightness(7,true);
    _display.clear();
    _state = State::PREVIEW;
    _solved=false;
  }

  const __FlashStringHelper* name() const override { return F("TM1637 Safe Dial"); }

  // ———— Tunables you can change per instance ————
  uint16_t cursorBlinkMs = 450;   // public if you want to tweak at runtime
  uint16_t successBreathPeriodMs = 1000;
  uint8_t  successBreathCycles   = 4;
  uint8_t  angryFlashes          = 5;
  uint16_t angryFlashMs          = 120;
  uint16_t countdownStepMs       = 50;
  uint16_t zeroHoldMs            = 1000;

private:
  // ===== pins / deps =====
  TM1637Display _display;
  uint8_t _pcfAddr;
  int     _correct;

  // ===== timings =====
  static constexpr uint16_t DEBOUNCE_MS       = 35;
  static constexpr uint16_t INVALID_BLINK_MS  = 140;

  // ===== state =====
  enum class State { PREVIEW, VALIDATE, INVALID_BLINK, LOCKED };
  State _state = State::PREVIEW;
  unsigned long _stateSince = 0;

  // button debounce
  uint8_t _lastBtn = HIGH, _stableBtn = HIGH;
  unsigned long _lastChange = 0;

  // cursor blink
  unsigned long _lastCursorBlink = 0;
  bool _cursorOn = true;

  // model: three stored digits (left 3 slots), rightmost is always live preview
  int8_t _stored[3] = {-1,-1,-1};
  uint8_t _nStored = 0;

  // snapshot on press
  uint8_t _snapshotMask = 0;

  // solved flag exposed to manager
  bool _solved = false;

  // ===== helpers =====
  static constexpr uint8_t DIGIT_MASKS[10] = {
    SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F,            // 0
    SEG_B|SEG_C,                                    // 1
    SEG_A|SEG_B|SEG_D|SEG_E|SEG_G,                  // 2
    SEG_A|SEG_B|SEG_C|SEG_D|SEG_G,                  // 3
    SEG_B|SEG_C|SEG_F|SEG_G,                        // 4
    SEG_A|SEG_C|SEG_D|SEG_F|SEG_G,                  // 5
    SEG_A|SEG_C|SEG_D|SEG_E|SEG_F|SEG_G,            // 6
    SEG_A|SEG_B|SEG_C,                              // 7
    SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F|SEG_G,      // 8
    SEG_A|SEG_B|SEG_C|SEG_D|SEG_F|SEG_G             // 9
  };

  uint8_t readSwitchSegments() {
    Wire.requestFrom((int)_pcfAddr, 1);
    if (!Wire.available()) return 0;
    uint8_t raw = Wire.read();
    uint8_t m = 0;
    if ((raw & (1<<0)) == 0) m |= SEG_A; // P0..P6 → a..g
    if ((raw & (1<<1)) == 0) m |= SEG_B;
    if ((raw & (1<<2)) == 0) m |= SEG_C;
    if ((raw & (1<<3)) == 0) m |= SEG_D;
    if ((raw & (1<<4)) == 0) m |= SEG_E;
    if ((raw & (1<<5)) == 0) m |= SEG_F;
    if ((raw & (1<<6)) == 0) m |= SEG_G;
    return m;
  }

  uint8_t readButtonState() {
    Wire.requestFrom((int)_pcfAddr, 1);
    if (!Wire.available()) return HIGH;
    uint8_t raw = Wire.read();
    // Button is on P7 (bit 7), return LOW when pressed (input pulled low)
    return (raw & (1<<7)) ? HIGH : LOW;
  }

  static bool maskToDigit(uint8_t mask, uint8_t &dOut) {
    for (uint8_t d=0; d<10; ++d) if (mask == DIGIT_MASKS[d]) { dOut = d; return true; }
    return false;
  }

  void renderPreview(uint8_t liveMask, bool showPreview) {
    uint8_t out[4] = {0,0,0,0};
    for (uint8_t i=0;i<3;i++) if (_stored[i] >= 0) out[i] = _display.encodeDigit(_stored[i]);
    if (showPreview) out[3] = liveMask;
    _display.setSegments(out);
  }

  int buildCodeWith(uint8_t lastDigit) {
    int v = 0;
    for (uint8_t i=0;i<3;i++) v = v*10 + (_stored[i] >= 0 ? _stored[i] : 0);
    return v*10 + lastDigit;
  }

  void getCodeDigits(uint8_t lastDigit, int8_t out[4]) {
    out[0] = (_stored[0] >= 0 ? _stored[0] : 0);
    out[1] = (_stored[1] >= 0 ? _stored[1] : 0);
    out[2] = (_stored[2] >= 0 ? _stored[2] : 0);
    out[3] = lastDigit;
  }

  void celebrateSuccessAndLock(const int8_t d[4]) {
    // breathing brightness cycles
    for (uint8_t c = 0; c < successBreathCycles; ++c) {
      unsigned long start = millis();
      while (millis() - start < successBreathPeriodMs) {
        float phase = (millis() - start) / (float)successBreathPeriodMs; // 0..1
        float level = 0.5f - 0.5f * cos(phase * TWO_PI);                 // 0..1
        uint8_t brightness = 1 + (uint8_t)(level * 6);                   // 1..7
        _display.setBrightness(brightness, true);
        renderDigits(d[0], d[1], d[2], d[3]);
        delay(10);
      }
    }
    _display.setBrightness(7, true);
    renderDigits(d[0], d[1], d[2], d[3]);
    _state = State::LOCKED;
  }

  void failureRitualAndReset(const int8_t d[4], uint8_t liveMask) {
    // Angry flashes
    for (uint8_t i=0;i<angryFlashes; ++i) {
      uint8_t blank[4] = {0,0,0,0};
      _display.setSegments(blank);
      delay(angryFlashMs);
      renderDigits(d[0], d[1], d[2], d[3]);
      delay(angryFlashMs);
    }

    // Rapid per-digit countdown to 0000 (left → right)
    int8_t cur[4] = { d[0], d[1], d[2], d[3] };
    for (uint8_t pos=0; pos<4; ++pos) {
      for (int val = cur[pos]; val > 0; --val) {
        cur[pos] = val - 1;
        renderDigits(cur[0],cur[1],cur[2],cur[3]);
        delay(countdownStepMs);
      }
      cur[pos] = 0;
      renderDigits(cur[0],cur[1],cur[2],cur[3]);
      delay(countdownStepMs);
    }

    // Hold 0000
    renderDigits(0,0,0,0);
    delay(zeroHoldMs);

    // Reset model, resume preview blinking
    _stored[0]=_stored[1]=_stored[2]=-1;
    _nStored=0;
    _display.setBrightness(7,true);
    _cursorOn=true;
    _lastCursorBlink=millis();
    renderPreview(liveMask, true);
    _state = State::PREVIEW;
  }

  void renderDigits(const int8_t d0, const int8_t d1, const int8_t d2, const int8_t d3) {
    uint8_t out[4] = {0,0,0,0};
    if (d0 >= 0) out[0] = _display.encodeDigit(d0);
    if (d1 >= 0) out[1] = _display.encodeDigit(d1);
    if (d2 >= 0) out[2] = _display.encodeDigit(d2);
    if (d3 >= 0) out[3] = _display.encodeDigit(d3);
    _display.setSegments(out);
  }
};

constexpr uint8_t SevenSegCodePuzzle::DIGIT_MASKS[10];