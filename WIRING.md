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
| CLK        | D10         | Clock signal |
| DIO        | D11         | Data I/O signal |
| VCC        | 5V          | Power supply |
| GND        | GND         | Ground |

### 2. PCF8574 I2C Expander (Address: 0x25)
Controls the 7-segment switches and button for code entry.

| PCF8574 Pin | Function | Description |
|-------------|----------|-------------|
| P0-P6       | Segment Control | Toggle switches for 7-segment display (segments a-g) |
| P7          | Button Input | Push button (connect to GND, internal pull-up used) |
| SDA         | I2C Hub SDA | I2C data line |
| SCL         | I2C Hub SCL | I2C clock line |
| VCC         | 5V | Power supply |
| GND         | GND | Ground |

**Address Configuration**: A0 connected to VCC, A1 and A2 connected to GND for address 0x25.

### 3. MCP23017 I2C Expander (Address: 0x20)
Controls puzzle status LEDs and Simon Says puzzle.

| MCP23017 Pin | Function | Description |
|--------------|----------|-------------|
| A3           | Puzzle 0 LED | Status LED for SevenSegCodePuzzle (Active LOW) |
| A4           | Puzzle 1 LED | Status LED for TiltButtonPuzzle (Active LOW) |
| A5           | Puzzle 2 LED | Status LED for SimonSaysPuzzle (Active LOW) |
| A6           | Puzzle 3 LED | Status LED for NFCAmiiboPuzzle (Active LOW) |
| A7           | Future LED | Reserved for additional puzzle status LED |
| B0           | Button 0 Input | Simon Says MX switch for note G (with pullup) |
| B1           | Button 1 Input | Simon Says MX switch for note C/A/F (with pullup) |
| B2           | Button 2 Input | Simon Says MX switch for note E/G (with pullup) |
| B3           | Button 3 Input | Simon Says MX switch for note D/C (with pullup) |
| B4           | Button 0 LED | Simon Says LED for button 0 (Active LOW) |
| B5           | Button 1 LED | Simon Says LED for button 1 (Active LOW) |
| B6           | Button 2 LED | Simon Says LED for button 2 (Active LOW) |
| B7           | Button 3 LED | Simon Says LED for button 3 (Active LOW) |
| SDA          | I2C Hub SDA | I2C data line |
| SCL          | I2C Hub SCL | I2C clock line |
| VCC          | 5V | Power supply |
| GND          | GND | Ground |

**Address Configuration**: A0, A1, A2 all connected to GND for address 0x20.

| SDA          | I2C Hub SDA | I2C data line |
| SCL          | I2C Hub SCL | I2C clock line |
| VCC          | 5V | Power supply |
| GND          | GND | Ground |

**Address Configuration**: A0, A1, A2 all connected to GND for address 0x20.

**LED Connection**: Connect LEDs with appropriate current-limiting resistors. LEDs are active LOW (LOW = LED ON, HIGH = LED OFF).

**Simon Says Button Wiring**: Connect one side of each MX switch to the respective B0-B3 pin, other side to GND. Internal pullups are enabled.

**Simon Says LED Wiring**: Connect LED cathode to B4-B7 pins, LED anode to +5V through current-limiting resistor (220Ω recommended).

### 4. Passive Buzzer
Provides audio feedback for Simon Says puzzle.

| Buzzer Pin | Arduino Pin | Description |
|------------|-------------|-------------|
| Positive   | D5          | PWM signal for tone generation |
| Negative   | GND         | Ground connection |

### 5. Servo Motor
Controls the lock mechanism.

| Servo Wire | Arduino Pin | Description |
|------------|-------------|-------------|
| Signal     | D9          | PWM control signal |
| VCC        | 5V          | Power supply |
| GND        | GND         | Ground |

**Servo Angles**:
- **Locked**: 0°
- **Unlocked**: 140°

### 6. Tilt Sensor
Detects orientation for the tilt puzzle.

| Tilt Sensor Pin | Arduino Pin | Description |
|-----------------|-------------|-------------|
| Pin 1           | D4          | Digital input (INPUT_PULLUP) |
| Pin 2           | GND         | Ground connection |

**Operation**: HIGH = active (right-side up), requires 10-second hold to solve.

### 7. PN532 NFC Module (I2C Mode, Address: 0x24)
Used for Goomba amiibo recognition puzzle.

| PN532 Pin | Arduino Pin | Description |
|-----------|-------------|-------------|
| VCC       | 3.3V        | Power supply (**3.3V only!**) |
| GND       | GND         | Ground connection |
| SDA       | A4 (SDA)    | I2C data line (via I2C hub) |
| SCL       | A5 (SCL)    | I2C clock line (via I2C hub) |

**Important Notes**: 
- The PN532 module **MUST** be powered with 3.3V, not 5V!
- Set module DIP switches for I2C mode
- IRQ and Reset pins are not used (I2C-only mode)
- Module uses I2C address 0x24

**Operation**: Present the Goomba amiibo (UID: 04:A6:89:72:3C:4D:80) to solve the puzzle.

### 8. ADXL345 Accelerometer (I2C Mode, Address: 0x53)
Used for knock detection puzzle.

| ADXL345 Pin | Arduino Pin | Description |
|-------------|-------------|-------------|
| VCC         | 3.3V        | Power supply (**3.3V recommended**) |
| GND         | GND         | Ground connection |
| SDA         | A4 (SDA)    | I2C data line (via I2C hub) |
| SCL         | A5 (SCL)    | I2C clock line (via I2C hub) |
| INT1        | D2          | Interrupt pin (optional, reserved for future use) |

**Important Notes**: 
- ADXL345 can work with 3.3V or 5V, but 3.3V is recommended
- Module uses I2C address 0x53 (default with SDO grounded)
- INT1 pin reserved on D2 for future interrupt-based detection

**Operation**: Detects 4 knocks in quick succession (2-second window) with deviation from gravity threshold of 5.0 m/s².

## Power Requirements
- **Arduino**: 5V via USB or DC jack
- **Most components**: Powered from Arduino 5V rail
- **PN532 NFC Module**: **3.3V only** (connect to Arduino 3.3V output)
- **Current draw**: Ensure adequate power supply capacity for servo motor operation

## I2C Address Summary
- **MCP23017**: 0x20 (puzzle status LEDs A3-A7, Simon Says buttons B0-B3 and LEDs B4-B7)
- **PN532 NFC**: 0x24 (Goomba amiibo recognition)
- **PCF8574**: 0x25 (7-segment switches and button)
- **ADXL345**: 0x53 (knock detection accelerometer)

## Pin Usage Summary
| Arduino Pin | Function | Component |
|-------------|----------|-----------|
| D2          | Interrupt Input | ADXL345 Accelerometer (INT pin) |
| D4          | Digital Input | Tilt Sensor |
| D5          | PWM Output | Passive Buzzer (Simon Says) |
| D9          | PWM Output | Servo Motor |
| D10         | TM1637 CLK | 7-Segment Display |
| D11         | TM1637 DIO | 7-Segment Display |
| A4 (SDA)    | I2C Data | I2C Hub |
| A5 (SCL)    | I2C Clock | I2C Hub |
| 5V          | Power Rail | All Components |
| GND         | Ground Rail | All Components |

## Available Pins for Expansion
- Digital: D3, D6, D7, D8, D12, D13
- Analog: A0, A1, A2, A3, A6, A7
- MCP23017 expansion pins: A0-A2, A7 (B0-B7 used by Simon Says)

## Notes
- All I2C devices share the same SDA/SCL lines via the I2C hub
- **PN532 NFC module requires 3.3V power, not 5V**
- Internal pull-up resistors are used where applicable
- Ensure proper power supply decoupling for stable I2C operation
- Test each component individually before final assembly