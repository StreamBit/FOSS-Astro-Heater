# Overview
This is a dew heater running open source firmware. 

Personally, I'm sick of seeing companies charge $300+ (USD, yes really) for basic dew heaters and then have the audacity to lock the user into their proprietary software — so I made my own.

This dew heater takes user input from a dial and keeps the heater band at a contant temperature (typically withing +/-3 degrees f). There are future plans to expand this functionality, see **Potential Features** below.


# Parts List
| Component                     | Details |
|--------------------------------|---------|
| **ESP32 WROOM**               | I used a 38-pin dev board from DORHEA |
| **DS18B20 Temp Probe**        | or similar three-lead temp probe |
| **IRF3708PBF MOSFET**         | - |
| **128x64 I2C OLED Display (x2)** | I used two SSD1306's<br>Make sure one can be running on IIC Address 0x3C and the other is on 0x3D |
| **Heater Band**               | Just find the cheapest 10W heater band available to you; no controller required |
| **10kΩ Potentiometer**        | - |
| **Momentary Button**          | - |
| **10kΩ Resistor**             | - |
| **220Ω Resistor**             | - |
| **Male/Female USB plugs (optional)** | If you don't want to use plugs, you can hardwire the heater and power USBs directly to the ESP32 |



# Assembly
Assemble the pieces on a breadboard or blank PCB using the below wiring:

| Component          | Wiring  |
|-------------------|------------|
| OLED Display 0x3C  | SDA -> GPIO 21, SCL -> GPIO 22 |
| OLED Display 0c3D  | SDA -> GPIO 21, SCL -> GPIO 22 |
| DS18B20 Sensor   | Data -> GPIO 4, VCC -> 3.3V, GND -> GND |
| Potentiometer    | Output -> GPIO 34 |
| Heater Power     | MOSFET Drain -> Heater(-), Heater(+) to 5V Source |
| Button Switch    | GPIO 13 |
| 220Ω Resistor | MOSFET Gate -> Resistor -> GPIO26 |
| 10kΩ Resistor | MOSFET Source -> Resistor -> MOSFET Drain |
| ESP32 | 5V source -> 5V pin, GND source -> GND pin |

Note: *All GPIO pins refer to the ESP32*


# Installation
1. Clone repo or download `firmware.ino` to your computer
2. Open file in `Arduino IDE` and connect ESP32
    - **Note:** The screens will not function properly unless you use **esp32 board manager `version 2.0.14`** Later versions of the board manager include I2C breaking code.
3. Install all included libraries to your IDE (find libraries at top of `firmware.ino`)
4. Compile and Upload firmware to your ESP32


# Usage

## Basic Use
Wrap the heater band around your scope, placing the temperature probe inside the band touching the scope body.

Power on your dew heater by plugging it into a 5V 2A USB source — the screens should illuminate. After the splash screen, you may select a target temperature with the pot from 0-100°f. 

The heater will being to warm until the temp probe reaches the target temperature. Once the temp falls back below the target, the heater will kick on again.

Screen 2 (0x3D) will output a graph to show temp probe readings and heater state for the last 30min.

## Locking Target Temp
Depending on your specific setup, the potentiometer reading may fluctuate as the heater is drawing power and the temp probe is reading within 0-4° of the target. If this is the case, use the button to lockout the pot once you have selected your target temp.

To change the target temp, press the lockout button again to toggle the lock.


# FAQ

<details>
  <summary><strong>Really? $300 for a Proprietary Dew Heater??</strong></summary>
  <p>Ok, maybe I fudged a bit there — but at the time of writing the <em>Pegasus DewMaster 2</em> is $279 below VAT and Shipping costs, and that's before buying the actual heating bands as well.
  
  Now, this current setup doesn't do <em>quite</em> as much as the <em>DewMaster</em>, but it is a <strong>whole</strong> lot cheaper. I built my personal setup for ~$50, and you could do it cheaper if you got better deals or already had some of the parts.
  
  See the Potential Features section below to see some things which might be added soon to truly make this a replacement for the <em>Pegasus</em>.</p>
</details>

<details>
  <summary><strong>How much does this all cost?</strong></summary>
  <p>About $50 if you have to buy everything, but you can save some money if you get better deals than I did or have some of these parts already laying around.
  
  I tried to use common components so you might be able to find somebody willing to part with a resistor or two.</p>
</details>

<details>
  <summary><strong>I'm having problems with my dew heater</strong></summary>
  <p>Ok, that's not really a question, but check out the Troubleshooting section below!</p>
</details>

<details>
  <summary><strong>This seems pretty basic, are you going to add more features?</strong></summary>
  <p>Short Answer: Probably
  
  Long Answer: I have a job and a life, <em>but</em> part of my life is Astronomy, so I'll add new features as I think of them and have the time to implement them. I won't be keeping any sort of update schedule, but you can follow this repo to be notified of the changes.</p>
</details>

<details>
  <summary><strong>I have a great idea for this project which needs to be added!</strong></summary>
  <p>That's great! That's the beauty of FOSS, feel free to make a fork of this project and get working on it!  :)
  
  If you want to collab on the main branch, make a pull request with your changes and I'll take a look.
  
  If you just have a feature you'd like to request, make an issue report and tag it as `feature request`.</p>
</details>

<details>
  <summary><strong>I found a bug</strong></summary>
  <p>Ooh, not good. Create an issue report and I'll take a look.
  
  Try to include as much info as you can to help me recreate the bug: ESP32 model#, part#'s, Serial Monitor outputs etc.
  
  Make sure to tag your post with `bug`.</p>
</details>

<details>
  <summary><strong>Why the GPLv3 License?</strong></summary>
  <p>Pretty simply put, I don't want anyone to try and charge money for this code (that includes <em>me</em>). I'm putting this out into the world so anyone can put together their own dew heater that just works. I want to make sure you have the right to use this code, for free, in perpetuity — and GNU's GPL license allows me to do that. 
  
  Check out https://www.gnu.org/licenses/gpl-3.0.en.html to learn more.</p>
</details>
<br>

# Potential Features and Fixes

## WiFi Integration
Add WiFi support to view board stats remotely.

## Support for Ambient Temperature and Humidity Sensors
Add support for additional sensors to display ambient temp and humidity. Show calculated dew point so user doesn't have to do the math themself.

## Automatic Mode
Use the additional sensors to automatically adjust the target temp based on dew point.

## Settings Menu
Add a settings menu to change defaults, view IP address, toggle wifi, etc.

## Error Handling
Current code returns an error when the temp probe is not connected or probe is misread. In an error state, heater is automatically on. Add toggle to adjust default heater state.

# Troubleshooting
Troubleshooting steps to be added as discovered