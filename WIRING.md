# SintBox Wiring Guide

This document describes how to wire all components for the SintBox puzzle system.

## Arduino Board
- **Board**: Arduino Uno
- **Power**: Connect via USB or external power supply

## I2C Connection
An I2C hub is wired to the header soldered to the I2C pins on the Arduino:
- **SDA**: A4 (Analog pin 4) → I2C Hub SDA
- **SCL**: A5 (Analog pin 5) → I2C Hub SCL
- **VCC**: 5V → I2C Hub VCC
- **GND**: GND → I2C Hub GND

## Components Wiring

### 1. TM1637 7-Segment Display
Used for the code entry puzzle.

| TM1637 Pin | Arduino Pin | Description |
|------------|-------------|-------------|
| CLK        | D2          | Clock signal |
| DIO        | D3          | Data I/O signal |
| VCC        | 5V          | Power supply |
| GND        | GND         | Ground |

### 2. PCF8574 I2C Expander (Address: 0x24)
Controls the 7-segment switches and button for code entry.

| PCF8574 Pin | Function | Description |
|-------------|----------|-------------|
| P0-P6       | Segment Control | Toggle switches for 7-segment display (segments a-g) |
| P7          | Button Input | Push button (connect to GND, internal pull-up used) |
| SDA         | I2C Hub SDA | I2C data line |
| SCL         | I2C Hub SCL | I2C clock line |
| VCC         | 5V | Power supply |
| GND         | GND | Ground |

**Address Configuration**: A0, A1, A2 all connected to GND for address 0x24.

### 3. MCP23017 I2C Expander (Address: 0x20)
Controls puzzle status LEDs and has expansion pins for future puzzles.

| MCP23017 Pin | Function | Description |
|--------------|----------|-------------|
| A3           | Puzzle 0 LED | Status LED for SevenSegCodePuzzle (Active LOW) |
| A4           | Puzzle 1 LED | Status LED for TiltButtonPuzzle (Active LOW) |
| A5-A7        | Future LEDs | Reserved for additional puzzle status LEDs |
| B0-B7        | Future Expansion | Available for additional puzzle inputs/outputs |
| SDA          | I2C Hub SDA | I2C data line |
| SCL          | I2C Hub SCL | I2C clock line |
| VCC          | 5V | Power supply |
| GND          | GND | Ground |

**Address Configuration**: A0, A1, A2 all connected to GND for address 0x20.

**LED Connection**: Connect LEDs with appropriate current-limiting resistors. LEDs are active LOW (LOW = LED ON, HIGH = LED OFF).

### 4. Servo Motor
Controls the lock mechanism.

| Servo Wire | Arduino Pin | Description |
|------------|-------------|-------------|
| Signal     | D9          | PWM control signal |
| VCC        | 5V          | Power supply |
| GND        | GND         | Ground |

**Servo Angles**:
- **Locked**: 0°
- **Unlocked**: 140°

### 5. Tilt Sensor
Detects orientation for the tilt puzzle.

| Tilt Sensor Pin | Arduino Pin | Description |
|-----------------|-------------|-------------|
| Pin 1           | D4          | Digital input (INPUT_PULLUP) |
| Pin 2           | GND         | Ground connection |

**Operation**: HIGH = active (right-side up), requires 10-second hold to solve.

## Power Requirements
- **Arduino**: 5V via USB or DC jack
- **All components**: Powered from Arduino 5V and GND rails
- **Current draw**: Ensure adequate power supply capacity for servo motor operation

## I2C Address Summary
- **PCF8574**: 0x24 (7-segment switches and button)
- **MCP23017**: 0x20 (puzzle status LEDs and expansion)

## Pin Usage Summary
| Arduino Pin | Function | Component |
|-------------|----------|-----------|
| D2          | TM1637 CLK | 7-Segment Display |
| D3          | TM1637 DIO | 7-Segment Display |
| D4          | Digital Input | Tilt Sensor |
| D9          | PWM Output | Servo Motor |
| A4 (SDA)    | I2C Data | I2C Hub |
| A5 (SCL)    | I2C Clock | I2C Hub |
| 5V          | Power Rail | All Components |
| GND         | Ground Rail | All Components |

## Available Pins for Expansion
- Digital: D5, D6, D7, D8, D10, D11, D12, D13
- Analog: A0, A1, A2, A3, A6, A7
- MCP23017 expansion pins: B0-B7, A0-A2, A5-A7

## Notes
- All I2C devices share the same SDA/SCL lines via the I2C hub
- Internal pull-up resistors are used where applicable
- Ensure proper power supply decoupling for stable I2C operation
- Test each component individually before final assembly