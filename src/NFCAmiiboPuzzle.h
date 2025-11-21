#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include "Puzzle.h"

class NFCAmiiboPuzzle : public Puzzle {
public:
  enum class State {
    WAITING_TO_START,
    IDLE,
    READING_NFC,
    SUCCESS_FEEDBACK,
    SOLVED
  };

  NFCAmiiboPuzzle(uint8_t irqPin, uint8_t resetPin) 
    : _irqPin(irqPin), _resetPin(resetPin), _nfc(irqPin, resetPin),
      _state(State::WAITING_TO_START), _solved(false), _stateTimer(0),
      _lastSeenAt(0), _lastUIDLen(0) {
    // Goomba UID: 04:A6:89:72:3C:4D:80
    _targetUID[0] = 0x04;
    _targetUID[1] = 0xA6;
    _targetUID[2] = 0x89;
    _targetUID[3] = 0x72;
    _targetUID[4] = 0x3C;
    _targetUID[5] = 0x4D;
    _targetUID[6] = 0x80;
    _targetUIDLen = 7;
  }

  void begin() override {
    Serial.println(F("NFCAmiiboPuzzle: Initializing..."));
    
    Wire.begin();
    _nfc.begin();
    
    uint32_t versiondata = _nfc.getFirmwareVersion();
    if (!versiondata) {
      Serial.println(F("NFCAmiiboPuzzle: PN532 not found. Check wiring and I2C mode switch."));
      _state = State::WAITING_TO_START;
      return;
    }
    
    Serial.print(F("NFCAmiiboPuzzle: PN532 firmware 0x"));
    Serial.println((versiondata >> 16) & 0xFF, HEX);
    
    // Configure for normal read mode
    _nfc.SAMConfig();
    
    Serial.println(F("NFCAmiiboPuzzle: Ready! Waiting for Goomba amiibo..."));
    _state = State::IDLE;
  }

  void update(uint32_t now) override {
    if (_state == State::SOLVED || _state == State::WAITING_TO_START) {
      return; // Nothing to do
    }
    
    if (_state == State::SUCCESS_FEEDBACK) {
      // Show success feedback for 2 seconds
      if (now - _stateTimer >= 2000) {
        _state = State::SOLVED;
      }
      return;
    }
    
    // IDLE or READING_NFC - check for NFC tag
    uint8_t uid[10];
    uint8_t uidLen = 0;
    bool detected = _nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 50);
    
    if (!detected) {
      return; // No tag present
    }
    
    // Debounce same card hovering
    if (uidLen == _lastUIDLen && memcmp(uid, _lastUID, uidLen) == 0) {
      if ((now - _lastSeenAt) < 800) { // 800ms cooldown
        return;
      }
    }
    
    // Update last seen
    memcpy(_lastUID, uid, uidLen);
    _lastUIDLen = uidLen;
    _lastSeenAt = now;
    
    // Print detected UID
    Serial.print(F("NFCAmiiboPuzzle: Detected UID["));
    Serial.print(uidLen);
    Serial.print(F("]: "));
    for (uint8_t i = 0; i < uidLen; i++) {
      if (uid[i] < 0x10) Serial.print('0');
      Serial.print(uid[i], HEX);
      if (i + 1 < uidLen) Serial.print(':');
    }
    Serial.println();
    
    // Check if it matches the Goomba
    if (uidLen == _targetUIDLen && memcmp(uid, _targetUID, uidLen) == 0) {
      Serial.println(F("NFCAmiiboPuzzle: GOOMBA DETECTED! PUZZLE SOLVED!"));
      _solved = true;
      _state = State::SUCCESS_FEEDBACK;
      _stateTimer = now;
    } else {
      Serial.println(F("NFCAmiiboPuzzle: Wrong amiibo, need the Goomba!"));
    }
  }

  bool isSolved() const override {
    return _solved;
  }

  void reset() override {
    Serial.println(F("NFCAmiiboPuzzle: Reset"));
    _solved = false;
    _state = State::IDLE;
    _stateTimer = 0;
    _lastSeenAt = 0;
    _lastUIDLen = 0;
  }

  const __FlashStringHelper* name() const override {
    return F("Goomba Amiibo");
  }

  int ledBrightness() const override {
    // Custom LED behavior based on state
    switch (_state) {
      case State::WAITING_TO_START:
        return 0; // Off if not initialized
      case State::IDLE:
        return 50; // Dim when waiting
      case State::READING_NFC:
      case State::SUCCESS_FEEDBACK:
      case State::SOLVED:
        return 255; // Bright when active or solved
      default:
        return 50;
    }
  }

private:
  uint8_t _irqPin;
  uint8_t _resetPin;
  Adafruit_PN532 _nfc;
  State _state;
  bool _solved;
  uint32_t _stateTimer;
  
  uint8_t _targetUID[10];   // Target Goomba UID
  uint8_t _targetUIDLen;
  
  uint8_t _lastUID[10];     // Last seen UID for debouncing
  uint8_t _lastUIDLen;
  uint32_t _lastSeenAt;
};
