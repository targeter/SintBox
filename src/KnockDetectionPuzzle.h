#pragma once
#include "Puzzle.h"
#include <Wire.h>

// ADXL345 I2C Address (default with SDO/ALT grounded)
#define ADXL345_ADDR 0x53

// ADXL345 Registers
#define ADXL345_REG_POWER_CTL 0x2D
#define ADXL345_REG_DATA_FORMAT 0x31
#define ADXL345_REG_DATAX0 0x32

class KnockDetectionPuzzle : public Puzzle {
public:
  /**
   * @param requiredKnocks Number of knocks required to solve (default 4)
   * @param knockThreshold Acceleration deviation from gravity in m/s^2 (default 5.0)
   * @param knockWindowMs Time window for knocks to count as a sequence (default 2000ms)
   * @param quietPeriodMs Minimum time between individual knocks (default 100ms)
   */
  KnockDetectionPuzzle(
    uint8_t requiredKnocks = 4,
    float knockThreshold = 3.0,
    uint32_t knockWindowMs = 2000,
    uint32_t quietPeriodMs = 50
  )
    : _requiredKnocks(requiredKnocks)
    , _knockThreshold(knockThreshold)
    , _knockWindowMs(knockWindowMs)
    , _quietPeriodMs(quietPeriodMs)
    , _state(State::WAITING_TO_START)
    , _knockCount(0)
    , _sequenceStartTime(0)
    , _lastKnockTime(0)
    , _lastMagnitude(0.0)
    , _knockArmed(true)
  {}

  void begin() override {
    Wire.begin();
    
    // Initialize ADXL345
    if (!initADXL345()) {
      Serial.println(F("[Knock] ERROR: ADXL345 initialization failed!"));
      return;
    }
    
    Serial.println(F("[Knock] ADXL345 initialized"));
    Serial.print(F("[Knock] Requires "));
    Serial.print(_requiredKnocks);
    Serial.print(F(" knocks within "));
    Serial.print(_knockWindowMs);
    Serial.print(F(" ms, threshold "));
    Serial.print(_knockThreshold, 2);
    Serial.println(F(" m/s^2"));
    
    _state = State::IDLE;
  }

  void update(uint32_t now) override {
    if (_state == State::SOLVED || _state == State::WAITING_TO_START) {
      return;
    }

    // Read accelerometer data
    int16_t x, y, z;
    if (!readAcceleration(x, y, z)) {
      return;  // Read failed, skip this update
    }

    // Calculate magnitude using proper Euclidean distance
    // At ±16g range, scale is ~3.9 mg/LSB, so LSB to m/s^2: value * 0.0039 * 9.81
    float x_ms2 = x * 0.03827;  // Combined scale factor
    float y_ms2 = y * 0.03827;
    float z_ms2 = z * 0.03827;
    float magnitude = sqrt(x_ms2 * x_ms2 + y_ms2 * y_ms2 + z_ms2 * z_ms2);
    
    // Calculate deviation from gravity (rest state is ~9.81 m/s^2)
    float delta = abs(magnitude - 9.81);
    
    // Debug output when activity is detected
    if (delta > 2.0) {
      Serial.print(F("[Knock] mag="));
      Serial.print(magnitude, 2);
      Serial.print(F(" delta="));
      Serial.print(delta, 2);
      Serial.print(F(" thresh="));
      Serial.print(_knockThreshold);
      Serial.print(F(" armed="));
      Serial.print(_knockArmed ? "Y" : "N");
      Serial.print(F(" time="));
      Serial.println(now - _lastKnockTime);
    }
    
    // State machine: must go below threshold before next knock can trigger
    // This prevents shaking (sustained high delta) from counting as multiple knocks
    bool isKnock = false;
    
    if (delta < (_knockThreshold * 0.5)) {
      // Below hysteresis threshold - arm the knock detector
      _knockArmed = true;
    } else if (delta > _knockThreshold && _knockArmed && (now - _lastKnockTime >= _quietPeriodMs)) {
      // Above threshold, armed, and cooldown expired - register knock
      isKnock = true;
      _knockArmed = false;  // Disarm until delta drops again
    }

    // Handle knock detection
    if (isKnock) {
      _lastKnockTime = now;
      
      // Start new sequence if idle or window expired
      if (_state == State::IDLE || (now - _sequenceStartTime) > _knockWindowMs) {
        _knockCount = 1;
        _sequenceStartTime = now;
        _state = State::DETECTING;
        Serial.print(F("[Knock] Sequence started (1/"));
        Serial.print(_requiredKnocks);
        Serial.println(F(")"));
      }
      // Continue sequence
      else if (_state == State::DETECTING) {
        _knockCount++;
        Serial.print(F("[Knock] Knock detected ("));
        Serial.print(_knockCount);
        Serial.print(F("/"));
        Serial.print(_requiredKnocks);
        Serial.println(F(")"));
        
        // Check if solved
        if (_knockCount >= _requiredKnocks) {
          _state = State::SOLVED;
          Serial.println(F("[Knock] ✓ SOLVED! Correct knock sequence detected"));
          return;
        }
      }
    }

    // Check for sequence timeout
    if (_state == State::DETECTING && (now - _sequenceStartTime) > _knockWindowMs) {
      Serial.print(F("[Knock] Sequence timed out with "));
      Serial.print(_knockCount);
      Serial.print(F("/"));
      Serial.print(_requiredKnocks);
      Serial.println(F(" knocks"));
      _knockCount = 0;
      _state = State::IDLE;
    }
  }

  bool isSolved() const override {
    return _state == State::SOLVED;
  }

  void reset() override {
    Serial.println(F("[Knock] Reset"));
    _state = State::IDLE;
    _knockCount = 0;
    _sequenceStartTime = 0;
    _lastKnockTime = 0;
    _lastMagnitude = 0.0;
    _knockArmed = true;
  }

  const __FlashStringHelper* name() const override {
    return F("Knock Detection");
  }

private:
  enum class State {
    WAITING_TO_START,  // Before begin() is called
    IDLE,              // Ready for knocks
    DETECTING,         // Counting knocks in sequence
    SOLVED             // Puzzle completed
  };

  // Configuration
  const uint8_t _requiredKnocks;
  const float _knockThreshold;
  const uint32_t _knockWindowMs;
  const uint32_t _quietPeriodMs;

  // State
  State _state;
  uint8_t _knockCount;
  uint32_t _sequenceStartTime;
  uint32_t _lastKnockTime;
  float _lastMagnitude;
  bool _knockArmed;

  /**
   * Initialize ADXL345 accelerometer
   * @return true if successful, false otherwise
   */
  bool initADXL345() {
    // Check device ID (should be 0xE5)
    Wire.beginTransmission(ADXL345_ADDR);
    Wire.write(0x00);  // Device ID register
    if (Wire.endTransmission() != 0) {
      return false;
    }
    
    Wire.requestFrom((uint8_t)ADXL345_ADDR, (uint8_t)1);
    if (Wire.available()) {
      uint8_t deviceId = Wire.read();
      if (deviceId != 0xE5) {
        Serial.print(F("[Knock] Unexpected device ID: 0x"));
        Serial.println(deviceId, HEX);
        return false;
      }
    } else {
      return false;
    }

    // Set data format: ±16g range, full resolution
    Wire.beginTransmission(ADXL345_ADDR);
    Wire.write(ADXL345_REG_DATA_FORMAT);
    Wire.write(0x0B);  // Full resolution, ±16g
    if (Wire.endTransmission() != 0) {
      return false;
    }

    // Enable measurement mode
    Wire.beginTransmission(ADXL345_ADDR);
    Wire.write(ADXL345_REG_POWER_CTL);
    Wire.write(0x08);  // Measurement mode
    if (Wire.endTransmission() != 0) {
      return false;
    }

    delay(10);  // Wait for sensor to stabilize
    return true;
  }

  /**
   * Read acceleration data from ADXL345
   * @param x Output: X-axis acceleration (raw ADC value)
   * @param y Output: Y-axis acceleration (raw ADC value)
   * @param z Output: Z-axis acceleration (raw ADC value)
   * @return true if read successful, false otherwise
   */
  bool readAcceleration(int16_t& x, int16_t& y, int16_t& z) {
    Wire.beginTransmission(ADXL345_ADDR);
    Wire.write(ADXL345_REG_DATAX0);
    if (Wire.endTransmission() != 0) {
      return false;
    }

    Wire.requestFrom((uint8_t)ADXL345_ADDR, (uint8_t)6);
    if (Wire.available() < 6) {
      return false;
    }

    // Read 6 bytes (2 bytes per axis: X, Y, Z)
    uint8_t x0 = Wire.read();
    uint8_t x1 = Wire.read();
    uint8_t y0 = Wire.read();
    uint8_t y1 = Wire.read();
    uint8_t z0 = Wire.read();
    uint8_t z1 = Wire.read();

    // Combine bytes (little-endian)
    x = (int16_t)((x1 << 8) | x0);
    y = (int16_t)((y1 << 8) | y0);
    z = (int16_t)((z1 << 8) | z0);

    return true;
  }
};
