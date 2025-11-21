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
TM_CLK = 2, TM_DIO = 3         // TM1637 display
SERVO_PIN = 9                   // Lock servo
TILT_PIN = 4                   // Tilt sensor
PCF_ADDR = 0x24                // 7-segment switches
MCP_LED_ADDR = 0x20            // Status LEDs (A3-A7)
```

## Development Workflows

### Building and Upload
```bash
# Primary workflow - build, upload, and monitor
platformio run --target upload && platformio device monitor --baud 115200
# Or use the VS Code task: "Upload and Monitor"
```

### Serial Commands (Runtime Testing)
```
RESET      - Reset all puzzles and lock box
UNLOCK     - Manual unlock (bypass puzzles)  
LOCK       - Manual lock
STATUS     - Show puzzle states
LEDTEST    - Test all status LEDs
SIMONTEST  - Test Simon Says puzzle LEDs
```

### Adding New Puzzles
1. Inherit from `Puzzle` class in new header file
2. Implement all virtual methods (especially `update()` state machine)
3. Add to `puzzles[]` array in `main.cpp` 
4. Increment `NUM_PUZZLES` constant
5. Update LED mapping comments (puzzles map to MCP23017 pins A3-A7)

## Puzzle Implementation Patterns

### State Machine Structure
```cpp
// All puzzles use millis()-based timing and enum states
enum class State { IDLE, ACTIVE, SOLVED };
State _state = State::IDLE;
uint32_t _stateTimer = 0;

void update(uint32_t now) override {
    // Debouncing pattern
    if (input != _lastInput) { _changeTime = now; _lastInput = input; }
    if ((now - _changeTime) >= DEBOUNCE_MS && input != _stableInput) {
        _stableInput = input;
        // Handle state change
    }
}
```

### LED Control Options
- Return `-1` from `ledBrightness()` for default on/off LED control
- Return `0-255` for custom PWM control (converted to on/off for MCP23017)
- LEDs are active-low: `LOW = ON, HIGH = OFF`

## Hardware Integration Patterns

### I2C Communication
- Always call `Wire.begin()` in puzzle `begin()` methods
- Use `Wire.beginTransmission(addr)` + `Wire.write()` + `Wire.endTransmission()` pattern
- Check return codes: `0 = success`, non-zero indicates I2C errors

### TM1637 Display (SevenSegCodePuzzle)
- Use local TM1637 library in `lib/TM1637/` (not external dependency)
- Display updates are immediate, no buffering needed
- Brightness: `setBrightness(0-7, bool on)`

## Project Structure
```
src/
├── main.cpp              # Hardware config, puzzle instances, serial commands
├── PuzzleManager.h       # Template class for puzzle coordination
├── Puzzle.h              # Pure virtual interface
├── SevenSegCodePuzzle.h  # Complex calculator-style code entry
└── TiltButtonPuzzle.h    # Simple hold-to-solve puzzle
lib/TM1637/              # Local TM1637 display library
WIRING.md                # Complete hardware connection guide
```

## Critical Debugging Notes
- Serial output at 115200 baud provides detailed state tracking
- MCP23017 initialization failures are common - check I2C addresses
- Puzzle state changes are logged automatically
- Use `STATUS` command to check current puzzle states without side effects

Do not build and upload while working on the code, the interaction with the Arduino is flaky. Let the do it manually. 