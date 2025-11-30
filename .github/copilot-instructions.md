# SintBox - AI Coding Agent Instructions

## Project Overview
SintBox is an Arduino-based escape room puzzle box with multiple puzzles that must be solved to unlock a servo-controlled lock. The system uses a templated C++ architecture with polymorphic puzzle classes and I2C hardware management.

## Architecture Patterns

### Core Components
- **PuzzleManager<N>**: Template class managing up to 5 puzzles with MCP23017 LED control and servo lock
- **Puzzle Interface**: Pure virtual base class defining puzzle lifecycle (`begin()`, `update()`, `isSolved()`, `reset()`)
- **Hardware Abstraction**: I2C expanders (PCF8574, MCP23017) handle GPIO and LED control

### Key Design Decisions
- **Template-based puzzle count**: `PuzzleManager<NUM_PUZZLES>` enforces compile-time puzzle limits
- **Polymorphic puzzle array**: `Puzzle* puzzles[]` allows different puzzle types in unified management
- **Non-blocking update loop**: All puzzles use `update(millis())` for time-based state machines
- **Sticky solve state**: Once `isSolved()` returns true, puzzles remain solved until explicit reset

### Hardware Pin Mapping (Critical)
```cpp
// Fixed pin assignments in main.cpp
TM_CLK = 10, TM_DIO = 11       // TM1637 display
SERVO_PIN = 9                   // Lock servo
TILT_PIN = 4                   // Tilt sensor
BUZZER_PIN = 5                 // Simon Says passive buzzer
KEY_PIN = 12                   // Key switch (power-on activation)
MCP_LED_ADDR = 0x20            // Dual purpose: Status LEDs (A3-A7) + Simon Says (B0-B7)
NFC_I2C_ADDR = 0x24            // PN532 NFC module (I2C-only mode)
PCF_ADDR = 0x25                // 7-segment switches (P0-P6: segments, P7: button)
ADXL345_ADDR = 0x53            // ADXL345 accelerometer (knock detection)
```

## Development Workflows

### Building and Testing
```bash
# Build only - DO NOT upload (Arduino interaction is unreliable)
platformio run

# Upload and monitor must be done manually by user
# Use VS Code task "Upload and Monitor" when ready
```

### Serial Commands (Runtime Testing)
```
RESET      - Reset all puzzles and lock box
UNLOCK     - Manual unlock (bypass puzzles)  
LOCK       - Manual lock
STATUS     - Show puzzle states and solution progress (includes NFC puzzle state)
LEDTEST    - Test all puzzle status LEDs (A3-A7)
SIMONTEST  - Test Simon Says buttons/LEDs (B0-B7) and buzzer
```

### Adding New Puzzles
1. Inherit from `Puzzle` class in new header file
2. Implement all virtual methods (especially `update()` state machine)
3. Add to `puzzles[]` array in `main.cpp` 
4. Increment `NUM_PUZZLES` constant (currently 4)
5. Update LED mapping comments (puzzles map to MCP23017 pins A3-A7)

### MCP23017 Shared Access Pattern (Critical for SimonSaysPuzzle)
```cpp
// 1. Initialize puzzle with nullptr MCP in main.cpp
SimonSaysPuzzle simonPuzzle(nullptr, BUZZER_PIN);

// 2. Manager initializes MCP, then provide reference to puzzle
manager.begin();
simonPuzzle.setMCP(manager.getMCP());

// 3. Manually call puzzle begin() after MCP is available
simonPuzzle.begin();
```

## Puzzle Implementation Patterns

### State Machine Structure
```cpp
// All puzzles use millis()-based timing and puzzle-specific enum states
// SevenSegCodePuzzle: PREVIEW, VALIDATE, INVALID_BLINK, LOCKED
// SimonSaysPuzzle: WAITING_TO_START, IDLE, PLAYING_SEQUENCE, WAITING_INPUT, SUCCESS_FEEDBACK, FAILURE_FEEDBACK
// NFCAmiiboPuzzle: WAITING_TO_START, IDLE, READING_NFC, SUCCESS_FEEDBACK, SOLVED
enum class State { WAITING_TO_START, IDLE, ACTIVE, SOLVED };
State _state = State::WAITING_TO_START;
uint32_t _stateTimer = 0;

void update(uint32_t now) override {
    // Standard debouncing pattern for inputs
    if (input != _lastInput) { _changeTime = now; _lastInput = input; }
    if ((now - _changeTime) >= DEBOUNCE_MS && input != _stableInput) {
        _stableInput = input;
        // Handle state transitions in switch statement
    }
}
```

### LED Control Options
- Return `-1` from `ledBrightness()` for default on/off LED control
- Return `0-255` for custom PWM control (converted to on/off for MCP23017)
- LEDs are active-low: `LOW = ON, HIGH = OFF`

## Hardware Integration Patterns

### MCP23017 Shared Resource Pattern
- **Critical**: SimonSaysPuzzle requires MCP pointer from PuzzleManager after initialization
- Use `setMCP(manager.getMCP())` + delayed `begin()` call pattern
- MCP pins: A3-A7 for status LEDs, B0-B7 for Simon Says buttons/LEDs

### I2C Communication
- Always call `Wire.begin()` in puzzle `begin()` methods if using I2C directly
- Use `Wire.beginTransmission(addr)` + `Wire.write()` + `Wire.endTransmission()` pattern
- Check return codes: `0 = success`, non-zero indicates I2C errors

### TM1637 Display (SevenSegCodePuzzle)
- Use local TM1637 library in `lib/TM1637/` (not external dependency)
- Display updates are immediate, no buffering needed
- Brightness: `setBrightness(0-7, bool on)`

### Audio Integration (SimonSaysPuzzle)
- Uses `tone(pin, frequency, duration)` for musical sequences
- Always call `noTone(pin)` to stop ongoing tones
- Buzzer pin must be configured as OUTPUT in puzzle `begin()`

### NFC Integration (NFCAmiiboPuzzle)
- Uses Adafruit PN532 library in I2C-only mode
- Constructor pattern: `Adafruit_PN532(-1, -1)` for I2C without IRQ/Reset pins
- Always call `Wire.begin()` and `_nfc.begin()` in puzzle `begin()` method
- Use 800ms debounce delay between tag reads to prevent re-reads
- UID validation: Compare 7-byte array from `_nfc.readPassiveTargetID()`
- Known Goomba amiibo UID: `{0x04, 0xA6, 0x89, 0x72, 0x3C, 0x4D, 0x80}`

## Project Structure
```
src/
├── main.cpp              # Hardware config, puzzle instances, serial commands
├── PuzzleManager.h       # Template class for puzzle coordination
├── Puzzle.h              # Pure virtual interface
├── SevenSegCodePuzzle.h  # Complex calculator-style code entry
├── TiltButtonPuzzle.h    # Simple hold-to-solve puzzle
├── SimonSaysPuzzle.h     # Musical sequence memory game
└── NFCAmiiboPuzzle.h     # NFC amiibo recognition (Goomba UID: 04:A6:89:72:3C:4D:80)
lib/TM1637/              # Local TM1637 display library
WIRING.md                # Complete hardware connection guide
```

## Critical Debugging Notes
- Serial output at 115200 baud provides detailed state tracking
- MCP23017 initialization failures are common - check I2C addresses
- Puzzle state changes are logged automatically
- Use `STATUS` command to check current puzzle states without side effects

## Important Workflow Restrictions
- **NEVER run upload commands** - Arduino communication is unreliable in automated environments
- **Only use `platformio run`** for compilation/build verification
- **User must manually upload** using "Upload and Monitor" task when ready
- Build errors are fine to fix, but leave upload/deploy to user 