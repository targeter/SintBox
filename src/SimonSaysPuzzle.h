#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include "Puzzle.h"

// Simon Says puzzle with 3 rounds of melodies
// 4 buttons (B0-B3) with corresponding LEDs (B4-B7) on MCP23017
// Buzzer on Arduino pin 5
// Each round plays a different melody sequence that players must copy
class SimonSaysPuzzle : public Puzzle {
public:
  SimonSaysPuzzle(Adafruit_MCP23X17* mcp, uint8_t buzzerPin) 
    : _mcp(mcp), _buzzerPin(buzzerPin) {}

  void begin() override {
    if (_mcp == nullptr) {
      _mcpInitialized = false;
      reset(); // Safe reset without MCP calls
      return;
    }
    
    // Set MCP as available BEFORE calling reset (so reset can use LEDs)
    _mcpInitialized = true;
    
    // Initialize puzzle state first
    reset();
    
    // Configure pins B0-B3 as inputs (buttons), B4-B7 as outputs (LEDs)
    for (int i = 8; i <= 11; i++) { // B0-B3 (pins 8-11)
      _mcp->pinMode(i, INPUT_PULLUP);
    }
    for (int i = 12; i <= 15; i++) { // B4-B7 (pins 12-15)
      _mcp->pinMode(i, OUTPUT);
      _mcp->digitalWrite(i, HIGH);     // LEDs off (active low)
    }
    
    Serial.println(F("Simon Says B pins configured"));
    
    pinMode(_buzzerPin, OUTPUT);
    digitalWrite(_buzzerPin, LOW);
    noTone(_buzzerPin);  // Ensure tone generator is off
    
    Serial.println(F("Simon Says puzzle initialized"));
    Serial.println(F("Round 1: Zie ginds komt de stoomboot"));
    Serial.println(F("Round 2: Sinterklaas kapoentje")); 
    Serial.println(F("Round 3: O, kom er eens kijken"));
  }

  void update(uint32_t now) override {
    if (_solved || !_mcpInitialized) return;
    
    // Debounce buttons
    _updateButtons(now);
    
    switch (_state) {
      case State::WAITING_TO_START:
        // Check if buttons 1, 2, and 4 are pressed simultaneously to start the game
        if (_buttonState[0] && _buttonState[1] && _buttonState[3]) { // Buttons 1, 2, 4 (indices 0, 1, 3)
          Serial.println(F("Buttons 1, 2, 4 pressed simultaneously - Simon Says starting! Get ready..."));
          _playStartChime();
          _state = State::IDLE;
          _stateTimer = now;
          return; // Exit early to avoid processing other states
        }
        break;
        
      case State::IDLE:
        if (now - _stateTimer >= 2000) { // 2 second delay before starting
          _startRound();
        }
        break;
        
      case State::PLAYING_SEQUENCE:
        _playSequenceStep(now);
        break;
        
      case State::WAITING_INPUT:
        _handlePlayerInput(now);
        break;
        
      case State::SUCCESS_FEEDBACK:
        if (now - _stateTimer >= 1000) { // 1 second success feedback
          _nextRound();
        }
        break;
        
      case State::FAILURE_FEEDBACK:
        if (now - _stateTimer >= 1500) { // 1.5 second failure feedback
          _replaySequence();
        }
        break;
    }
  }

  bool isSolved() const override { return _solved; }

  void reset() override {
    _solved = false;
    _currentRound = 0;
    _sequenceIndex = 0;
    _playerIndex = 0;
    _currentLength = _round1Part1Length;  // Start with first part of round 1
    _timeoutCount = 0;   // Reset timeout counter
    _state = State::WAITING_TO_START;
    _stateTimer = millis();
    
    // Stop any ongoing tone
    noTone(_buzzerPin);
    
    // Only call LED functions if MCP is initialized
    if (_mcpInitialized) {
      _allLedsOff();
    }
    
    // Initialize button states
    for (int i = 0; i < 4; i++) {
      _buttonState[i] = false;
      _lastRawState[i] = false;
      _lastPressedState[i] = false;
      _buttonChangeTime[i] = 0;
    }
    
    Serial.println(F("Simon Says puzzle reset"));
    if (_mcpInitialized) {
      Serial.println(F("Press buttons 1, 2, and 4 simultaneously to start the game!"));
    }
  }

  const __FlashStringHelper* name() const override { return F("Simon Says"); }

  // Set the MCP reference (called after PuzzleManager initializes MCP)
  void setMCP(Adafruit_MCP23X17* mcp) {
    _mcp = mcp;
  }

  // Test all Simon Says LEDs in sequence
  void testLEDs() {
    if (!_mcpInitialized) {
      Serial.println(F("ERROR: Cannot test LEDs - MCP23017 not available"));
      return;
    }
    
    Serial.println(F("Testing Simon Says LEDs..."));
    
    // Test each LED individually
    for (int i = 0; i < 4; i++) {
      Serial.print(F("LED "));
      Serial.print(i);
      Serial.println(F(" ON"));
      _setLED(i, true);
      delay(500);
      _setLED(i, false);
      delay(200);
    }
    
    // Test all LEDs together
    Serial.println(F("All LEDs ON"));
    _allLedsOn();
    delay(1000);
    
    Serial.println(F("All LEDs OFF"));
    _allLedsOff();
    delay(500);
    
    // Quick flash pattern
    Serial.println(F("Flash pattern"));
    for (int cycle = 0; cycle < 3; cycle++) {
      _allLedsOn();
      delay(200);
      _allLedsOff();
      delay(200);
    }
    
    Serial.println(F("Simon Says LED test complete"));
  }

private:
  enum class State {
    WAITING_TO_START,
    IDLE,
    PLAYING_SEQUENCE,
    WAITING_INPUT,
    SUCCESS_FEEDBACK,
    FAILURE_FEEDBACK
  };

  // Musical note frequencies (Hz)
  enum Note {
    NOTE_C4 = 262,
    NOTE_D4 = 294,
    NOTE_F4 = 349,
    NOTE_A4 = 440,
    NOTE_G4 = 392,
    NOTE_Ab4 = 466,
    NOTE_E4 = 330,
    NOTE_B4 = 494,  // B4
    NOTE_C = 523,  // C5
    NOTE_D = 587,  // D5
    NOTE_E = 659,  // E5
    NOTE_F = 698,  // F5
    NOTE_G = 784,  // G5
    NOTE_A = 880   // A5
  }; 

  // Song definitions - each array contains the sequence of buttons (0-3) to press
  // Button 0=G, Button 1=C, Button 2=E, Button 3=D (for round 1)
  static const uint8_t _round1Sequence[];  // "Zie ginds komt de stoomboot": G C C E D D
  static const uint8_t _round2Sequence[];  // "Sinterklaas kapoentje": G G A A G E  
  static const uint8_t _round3Sequence[];  // "O, kom er eens kijken": E F E F G C
  
  static const uint8_t _round1Length = 11;
  static const uint8_t _round2Length = 12;
  static const uint8_t _round3Length = 15;
  
  // Part lengths for two-part structure
  static const uint8_t _round1Part1Length = 6;
  static const uint8_t _round1Part2Length = 5;
  static const uint8_t _round2Part1Length = 6;
  static const uint8_t _round2Part2Length = 6;
  static const uint8_t _round3Part1Length = 8;
  static const uint8_t _round3Part2Length = 7;

  // Note mapping for each button in each round
  // Part 1 (first 6 notes) mappings
  static const Note _round1Notes[4];  // G, C, E, D
  static const Note _round2Notes[4];  // G, A, E, (unused)
  static const Note _round3Notes[4];  // E, F, G, C
  
  // Part 2 (last 6 notes) mappings - different notes for same buttons
  static const Note _round1NotesPart2[4];
  static const Note _round2NotesPart2[4];
  static const Note _round3NotesPart2[4];

  Adafruit_MCP23X17* _mcp;
  uint8_t _buzzerPin;
  
  bool _solved = false;
  bool _mcpInitialized = false;
  State _state = State::WAITING_TO_START;
  uint32_t _stateTimer = 0;
  
  uint8_t _currentRound = 0;        // 0, 1, or 2
  uint8_t _sequenceIndex = 0;       // Current position in sequence being played/learned
  uint8_t _playerIndex = 0;         // Current position player needs to input
  uint8_t _currentLength = 1;       // Current build-up length (starts at 1, grows to full sequence)
  uint8_t _timeoutCount = 0;        // Number of consecutive timeouts (resets puzzle after 4)
  
  // Button debouncing - simplified approach
  bool _buttonState[4] = {false};           // Current debounced state
  bool _lastRawState[4] = {false};          // Last raw read for change detection
  bool _lastPressedState[4] = {false};      // Last state we checked for press
  uint32_t _buttonChangeTime[4] = {0};      // Time of last state change
  static const uint32_t DEBOUNCE_MS = 5;    // Very fast debounce - aggressive for responsiveness
  
  // Timing
  static const uint32_t NOTE_DURATION = 400;  // How long each note/LED plays
  static const uint32_t NOTE_PAUSE = 200;     // Pause between notes
  static const uint32_t INPUT_TIMEOUT = 5000; // 5 seconds to make a move
  uint32_t _inputTimer = 0;

  void _updateButtons(uint32_t now) {
    for (int i = 0; i < 4; i++) {
      bool rawState = !_mcp->digitalRead(i + 8); // B0-B3 are pins 8-11, Active low (pullup)
      
      // Detect state change
      if (rawState != _lastRawState[i]) {
        _lastRawState[i] = rawState;
        _buttonChangeTime[i] = now;
        
        // Accept button PRESS immediately (rising edge) for max responsiveness
        if (rawState && !_buttonState[i]) {
          _buttonState[i] = true;
        }
      }
      
      // Use debounce only for releases to prevent false positives
      if ((now - _buttonChangeTime[i]) >= DEBOUNCE_MS) {
        bool oldState = _buttonState[i];
        _buttonState[i] = rawState;
        
        // Clear the "pressed" flag when button is released
        // This allows the same button to be pressed again
        if (oldState && !rawState) {
          _lastPressedState[i] = false;
        }
      }
    }
  }

  void _startRound() {
    _sequenceIndex = 0;
    _playerIndex = 0;
    _state = State::PLAYING_SEQUENCE;
    _stateTimer = millis();
    
    Serial.print(F("Round "));
    Serial.print(_currentRound + 1);
    Serial.print(F(" - Length "));
    Serial.print(_currentLength);
    Serial.print(F("/"));
    Serial.print(_getCurrentSequenceLength());
    Serial.println(F(" - Watch and listen..."));
  }

  void _playSequenceStep(uint32_t now) {
    const uint8_t* sequence = _getCurrentSequence();
    // Use current build-up length instead of full sequence
    uint8_t sequenceLength = _currentLength;
    
    uint32_t elapsed = now - _stateTimer;
    uint32_t stepDuration = NOTE_DURATION + NOTE_PAUSE;
    
    if (elapsed < NOTE_DURATION) {
      // Play note and show LED
      if (_sequenceIndex < sequenceLength) {
        uint8_t button = sequence[_sequenceIndex];
        _playNote(button);
        _setLED(button, true);
      }
    } else if (elapsed < stepDuration) {
      // Pause - turn off LED and buzzer
      _allLedsOff();
      noTone(_buzzerPin);
    } else {
      // Move to next step
      _sequenceIndex++;
      if (_sequenceIndex >= sequenceLength) {
        // Finished playing sequence
        _state = State::WAITING_INPUT;
        _playerIndex = 0;
        _inputTimer = now;
        Serial.println(F("Your turn! Repeat the sequence..."));
      }
      _stateTimer = now;
    }
  }

  void _handlePlayerInput(uint32_t now) {
    // Check for timeout
    if (now - _inputTimer > INPUT_TIMEOUT) {
      _timeoutCount++;
      Serial.print(F("Timeout! ("));
      Serial.print(_timeoutCount);
      Serial.println(F("/4)"));
      
      if (_timeoutCount >= 4) {
        Serial.println(F("Too many timeouts - resetting puzzle to start"));
        reset();
        return;
      } else {
        Serial.println(F("Try again..."));
        _failure();
        return;
      }
    }
    
    // Check for button presses
    for (int i = 0; i < 4; i++) {
      if (_buttonPressed(i)) {
        _handleButtonPress(i, now);
        return;
      }
    }
  }

  bool _buttonPressed(int button) {
    // Detect rising edge: button is pressed now AND wasn't pressed in last check
    // Only check the edge, don't update state here
    return _buttonState[button] && !_lastPressedState[button];
  }

  void _handleButtonPress(int button, uint32_t now) {
    const uint8_t* sequence = _getCurrentSequence();
    // Use current build-up length instead of full sequence
    uint8_t sequenceLength = _currentLength;
    
    // Mark as handled FIRST to prevent double-triggering during feedback
    _lastPressedState[button] = true;
    
    // Visual and audio feedback
    _playNote(button);
    _setLED(button, true);
    
    // Continue updating buttons during feedback delay to catch releases
    uint32_t feedbackStart = millis();
    while (millis() - feedbackStart < 200) {
      _updateButtons(millis());
      delay(10); // Small delay to avoid hammering I2C
    }
    
    _allLedsOff();
    noTone(_buzzerPin);
    
    if (button == sequence[_playerIndex]) {
      // Correct button
      _playerIndex++;
      _inputTimer = now; // Reset timeout
      _timeoutCount = 0; // Reset timeout counter on successful input
      
      if (_playerIndex >= sequenceLength) {
        // Current length completed correctly
        Serial.print(F("Correct! Length "));
        Serial.print(_currentLength);
        Serial.println(F(" completed."));
        _success();
      }
    } else {
      // Wrong button
      _timeoutCount = 0; // Reset timeout counter on any input (even wrong)
      Serial.print(F("Wrong! Expected button "));
      Serial.print(sequence[_playerIndex]);
      Serial.print(F(", got "));
      Serial.print(button);
      Serial.println(F(" - repeating sequence..."));
      _failure();
    }
  }

  void _success() {
    _state = State::SUCCESS_FEEDBACK;
    _stateTimer = millis();
    
    uint8_t fullSequenceLength = _getCurrentSequenceLength();
    
    if (_currentLength >= fullSequenceLength) {
      // Completed the full song - move to next round
      Serial.print(F("Song "));
      Serial.print(_currentRound + 1);
      Serial.println(F(" completed!"));
      
      // Success pattern - all LEDs blink
      for (int i = 0; i < 3; i++) {
        _allLedsOn();
        _playSuccessSound();
        delay(200);
        _allLedsOff();
        delay(200);
      }
      
      _nextRound();
    } else {
      // Jump from part 1 to full length (part 1 + part 2)
      uint8_t part1Length;
      switch (_currentRound) {
        case 0: part1Length = _round1Part1Length; break;
        case 1: part1Length = _round2Part1Length; break;
        case 2: part1Length = _round3Part1Length; break;
        default: part1Length = 6; break;
      }
      
      if (_currentLength == part1Length) {
        _currentLength = fullSequenceLength;
        Serial.println(F("Part 1 done! Now both parts..."));
      } else {
        _currentLength++;
        Serial.print(F("Length "));
        Serial.println(_currentLength);
      }
      
      // Brief success feedback
      _playSuccessSound();
      delay(500);
      
      // Continue with current round at increased length
      _startRound();
    }
  }

  void _failure() {
    _state = State::FAILURE_FEEDBACK;
    _stateTimer = millis();
    
    // Failure sound and LED pattern
    _playFailureSound();
    _allLedsOn();
    delay(500);
    _allLedsOff();
    
    // Repeat the same build-up length (don't reset to 1)
    Serial.print(F("Repeating sequence of length "));
    Serial.println(_currentLength);
    _startRound();
  }

  void _nextRound() {
    _currentRound++;
    
    // Reset to part 1 length for the new round
    switch (_currentRound) {
      case 0: _currentLength = _round1Part1Length; break;
      case 1: _currentLength = _round2Part1Length; break;
      case 2: _currentLength = _round3Part1Length; break;
      default: _currentLength = 6; break;
    }
    
    if (_currentRound >= 3) {
      _solved = true;
      Serial.println(F("*** Simon Says puzzle SOLVED! All rounds completed! ***"));
    } else {
      Serial.print(F("Round "));
      Serial.print(_currentRound + 1);
      Serial.println(F(" ready - press buttons 1, 2, and 4 simultaneously to start!"));
      _state = State::WAITING_TO_START;
      _stateTimer = millis();
    }
  }

  void _replaySequence() {
    Serial.println(F("Let me show you again..."));
    _state = State::IDLE;
    _stateTimer = millis() - 1000; // Shorter delay for replay
  }

  const uint8_t* _getCurrentSequence() {
    switch (_currentRound) {
      case 0: return _round1Sequence;
      case 1: return _round2Sequence;
      case 2: return _round3Sequence;
      default: return _round1Sequence;
    }
  }

  uint8_t _getCurrentSequenceLength() {
    switch (_currentRound) {
      case 0: return _round1Length;
      case 1: return _round2Length;
      case 2: return _round3Length;
      default: return _round1Length;
    }
  }

  void _playNote(uint8_t button) {
    Note note;
    // Check _playerIndex if in input phase, _sequenceIndex if in playback phase
    uint8_t currentPosition = (_state == State::WAITING_INPUT) ? _playerIndex : _sequenceIndex;
    
    // Determine part boundary based on current round
    uint8_t part1Length;
    switch (_currentRound) {
      case 0: part1Length = _round1Part1Length; break;
      case 1: part1Length = _round2Part1Length; break;
      case 2: part1Length = _round3Part1Length; break;
      default: part1Length = 6; break;
    }
    
    bool isPart2 = (currentPosition >= part1Length);
    
    switch (_currentRound) {
      case 0: note = isPart2 ? _round1NotesPart2[button] : _round1Notes[button]; break;
      case 1: note = isPart2 ? _round2NotesPart2[button] : _round2Notes[button]; break;
      case 2: note = isPart2 ? _round3NotesPart2[button] : _round3Notes[button]; break;
      default: note = NOTE_C; break;
    }
    tone(_buzzerPin, note);
  }

  void _playSuccessSound() {
    tone(_buzzerPin, NOTE_C);
    delay(100);
    tone(_buzzerPin, NOTE_E);
    delay(100);
    tone(_buzzerPin, NOTE_G);
    delay(100);
    noTone(_buzzerPin);
  }

  void _playFailureSound() {
    tone(_buzzerPin, 200);
    delay(300);
    tone(_buzzerPin, 150);
    delay(300);
    noTone(_buzzerPin);
  }

  void _playStartChime() {
    // Short ascending chime to indicate game start
    tone(_buzzerPin, NOTE_E);
    delay(80);
    tone(_buzzerPin, NOTE_G);
    delay(80);
    tone(_buzzerPin, NOTE_C + 261); // C6 (higher octave)
    delay(120);
    noTone(_buzzerPin);
  }

  void _setLED(uint8_t button, bool on) {
    if (button < 4) {
      _mcp->digitalWrite(button + 12, on ? LOW : HIGH); // B4-B7 are pins 12-15, Active low LEDs
    }
  }

  void _allLedsOff() {
    for (int i = 0; i < 4; i++) {
      _mcp->digitalWrite(i + 12, HIGH); // B4-B7 are pins 12-15, Active low
    }
  }

  void _allLedsOn() {
    for (int i = 0; i < 4; i++) {
      _mcp->digitalWrite(i + 12, LOW); // B4-B7 are pins 12-15, Active low
    }
  }
};

// Static member definitions - arrays duplicated for two-part structure
const uint8_t SimonSaysPuzzle::_round1Sequence[] = {0, 1, 1, 2, 3, 3, 0, 1, 1, 3, 2}; // G C C E D D (x2)
const uint8_t SimonSaysPuzzle::_round2Sequence[] = {0, 0, 1, 1, 0, 2, 1, 1, 1, 0, 1, 1}; // G G A A G E (x2)
const uint8_t SimonSaysPuzzle::_round3Sequence[] = {0, 0, 1, 1, 1, 2, 3, 1,     2, 2, 2, 2, 3, 2, 1}; // D D G G G A B G + A A A A B A G (8+7 notes)

// Note mappings for each round - Part 1 (first 6 notes)
const SimonSaysPuzzle::Note SimonSaysPuzzle::_round1Notes[4] = {NOTE_C4, NOTE_F4, NOTE_A4, NOTE_G4};
const SimonSaysPuzzle::Note SimonSaysPuzzle::_round2Notes[4] = {NOTE_G4, NOTE_A4, NOTE_E4, NOTE_C};
const SimonSaysPuzzle::Note SimonSaysPuzzle::_round3Notes[4] = {NOTE_D4, NOTE_G4, NOTE_A4, NOTE_B4};   

// Note mappings for Part 2 (last 6 notes) - change these to your desired notes
const SimonSaysPuzzle::Note SimonSaysPuzzle::_round1NotesPart2[4] = {NOTE_Ab4, NOTE_E4, NOTE_F4, NOTE_G4};
const SimonSaysPuzzle::Note SimonSaysPuzzle::_round2NotesPart2[4] = {NOTE_D4, NOTE_F4, NOTE_E4, NOTE_C}; 
const SimonSaysPuzzle::Note SimonSaysPuzzle::_round3NotesPart2[4] = {NOTE_D4, NOTE_G4, NOTE_A4, NOTE_B4};  // A A A A B A G     // Same as part 1 for now

/* 
D D G G G A B G.AAAABAG 
G G C C C D E C DDDDED C
*/