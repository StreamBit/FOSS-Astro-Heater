# FOSS-Astro-Heater

## Overview
The **FOSS-Astro-Heater** is an **open-source ESP32-based dew heater controller** designed for astrophotography and similar applications. This project was created as a response to the absurd prices Astronomy and Optics companies charge for similar products.

At time of writing, the _Celestron Dew Heater_ and _PegasusAstro Pocket Powerbox Micro_ are each ~$250. To be clear, I don't believe these are bad companies but I want to provide an alternative for those of us who only need a single dew heater coil with basic functionality.

## Features
- **Temperature Control:** Reads current temperature from a DS18B20 sensor and compares it to the user-set target.
- **MOSFET-Based Heater Control:**
- **Dual OLED Display:**
  - **Display 1 (Primary Screen)**:
    - Shows target temperature (left side)
    - Displays current temperature (right side)
    - Indicates heater state (ON/OFF) and target temperature lock status
  - **Display 2 (Graphical Screen)**:
    - Graphs the last 30 minutes of temperature data
    - Shows heater activity as a bar at the bottom
- **Button Control:** Allows the user to **lock** the target temperature to prevent accidental changes.
- **Graphing History:** Logs temperature data every **20 seconds** to display **30 minutes of temperature trends**.

## Hardware Requirements
- **ESP32 Development Board**
- **Two SSD1306 OLED Displays (0.96" I2C, 128x64 pixels)**
- **DS18B20 Digital Temperature Sensor**
- **10kΩ Potentiometer** (for target temperature selection)
- **IRF3708PBF MOSFET** (for heater control)
- **USB-Powered Resistive Heater**
- **Push Button** (for target lock control)
- **Pull-down Resistors & Wiring**

## Wiring Guide
| Component          | ESP32 Pin  |
|-------------------|------------|
| OLED Display #1  | SDA -> GPIO 21, SCL -> GPIO 22 |
| OLED Display #2  | SDA -> GPIO 21, SCL -> GPIO 22 |
| DS18B20 Sensor   | Data -> GPIO 4, VCC -> 3.3V, GND -> GND |
| Potentiometer    | Output -> GPIO 34 |
| MOSFET Gate      | GPIO 26 |
| Heater Power     | MOSFET Drain -> Heater +, Heater - to GND |
| Button Switch    | GPIO 13 (with pull-down resistor) |

## Installation & Setup
1. **Install Required Libraries**
   - Install the following libraries in the Arduino IDE or PlatformIO:
     - `Adafruit GFX Library`
     - `Adafruit SSD1306`
     - `DallasTemperature`
     - `OneWire`
2. **Flash the Code**
   - Upload the **ESP32 Example Code** to your ESP32 board.
3. **Power On & Monitor**
   - The device will boot up, display a startup animation, and begin temperature monitoring.
   - Adjust the **potentiometer** to set the target temperature.
   - Press the **button** to **lock/unlock** the target temperature.

## License
This project is licensed under the **GNU General Public License v3.0**. See [LICENSE](LICENSE) for details.

## Contributing
Contributions are welcome! Feel free to submit **pull requests** or report **issues** on GitHub.

## Future Improvements
- Support for **Ambient Temperature** and **Humidity** sensors
- **Automatic Target Temp** setting based on dew point
- Adaptive **PID control** instead of simple ON/OFF switching
- Debug Mode
