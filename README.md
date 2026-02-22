<div align="center">

# 3D_Scanner_ESP8266

[![Arduino](https://img.shields.io/badge/Arduino_IDE-00979D?style=for-the-badge&logo=arduino&logoColor=white)](https://docs.arduino.cc/software/ide/)
[![C++](https://img.shields.io/badge/C%2B%2B-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)](https://learncpp.com/)
[![HTML5](https://img.shields.io/badge/HTML5-E34F26?style=for-the-badge&logo=html5&logoColor=white)](https://www.w3schools.com/html/default.asp)
![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)

**A real-time 3D point cloud scanner over WiFi using Arduino Uno, ESP8266 and VL53L0X laser distance sensor.**

</div>

---

## Hardware

| Component | Quantity |
|---|---|
| Arduino Uno | 1 |
| ESP8266-01S WiFi Module | 1 |
| VL53L0X Laser Distance Sensor | 1 |
| MG90S Servo Motor (Pan) | 1 |
| MG90S Servo Motor (Tilt) | 1 |

---

## Wiring

| Component | Arduino Pin | Note |
|---|---|---|
| Servo Pan | D9 | PWM |
| Servo Pan | 5V | Power |
| Servo Pan | GND | Ground |
| Servo Tilt | D10 | PWM |
| Servo Tilt | 5V | Power |
| Servo Tilt |GND | Ground |
| VL53L0X SDA | A4 | I2C Data |
| VL53L0X SCL | A5 | I2C Clock |
| VL53L0X VIN | 3V3 | Power |
| VL53L0X GND | GND | Ground |
| ESP8266 RX | D3 (SoftSerial TX) | Direct connection |
| ESP8266 TX | D2 (SoftSerial RX) | Direct connection |
| ESP8266 CH_PD,VCC | 3V3 | Power |
| ESP8266 GND | GND | Ground |

![Main Wiring Diagram](Wiring%20Diagrams/wiring_diagram_main.jpg)

---

## Setup

### 1. Libraries

Install the following libraries via Arduino IDE Library Manager:

- `VL53L0X` — Pololu
- `Servo` — Arduino Built-in
- `SoftwareSerial` — Arduino Built-in
- `ESP8266WiFi` — ESP8266 Community
- `ESP8266WebServer` — ESP8266 Community
- `LittleFS` — ESP8266 Community

### 2. ESP8266 Board Support

Go to **File > Preferences > Additional Board Manager URLs** and add:

```
https://arduino.esp8266.com/stable/package_esp8266com_index.json
```

Then install the `esp8266` package from **Tools > Board Manager**.

### 3. Flash Size

Before uploading to ESP8266, set:

**Tools > Flash Size > `4MB (FS:2MB OTA:~1MB)`**

LittleFS requires a dedicated flash partition to store scan data.

### 4. Upload Order

Upload `ESP8266_Server.ino` to the ESP8266 first, then upload `Uno_Controller.ino` to the Arduino Uno.

---

## Usage

1. Power on the system — the ESP8266 will create a WiFi access point named **`3D_Scanner_Project`**.
2. Connect your phone or computer to this network. Password: **`12345678`**
3. Open a browser and go to **`192.168.4.1`**.
4. Navigate to **`192.168.4.1/scan`** to start a scan.
5. Once the scan is complete, return to the main page — the point cloud will be rendered in WebGL.

### Endpoints

| Address | Function |
|---|---|
| `/scan` | Starts a new scan |
| `/status` | Returns current status as JSON |
| `/reset` | Clears previous scan data |
| `/format` | Formats the LittleFS filesystem |

---

## Adjusting Pan-Tilt Angles

You can customize the scan area by editing the configuration block at the top of `Uno_Controller.ino`:

```cpp
const int YAW_MIN  = 0;   // Pan start angle  (°)
const int YAW_MAX  = 80;  // Pan end angle    (°)
const int PITCH_MIN = 0;  // Tilt start angle (°)
const int PITCH_MAX = 90; // Tilt end angle   (°)

const int STEP_YAW   = 2; // Step size — smaller = more points, slower scan
const int STEP_PITCH = 2;

const float VALID_MIN = 0.04f; // Valid distance range (meters)
const float VALID_MAX = 0.30f;
```

---

## Flashing the ESP8266-01S

![ESP8266-01S Flashing Setup](Wiring%20Diagrams/esp8266-01s_firmware_flashing.jpg)

### ESP8266-01S Firmware Flashing Procedure
This section details the hardware setup required to flash custom firmware onto the ESP8266-01S using the Arduino IDE. Due to the minimal pin output of the ESP-01S, a special USB-TTL serial converter is required for programming.

#### Critical Power Supply Considerations
Most USB-TTL converters provide a maximum of 50–100mA from the 3.3V pin, but the ESP8266-01S can draw up to 170–300mA during programming. Insufficient current causes:
- Brownout reset (continuous reset due to voltage drop)
- Connection drops during firmware upload
- "Failed to connect to ESP8266" errors
- Risk of permanent damage to the module

#### AMS1117-3.3 Regulator Solution
The AMS1117-3.3 linear voltage regulator solves this by providing a stable 3.3V/800mA from the 5V USB supply.

**Key connections:**

1. **GND** — Connect ESP-01S GND to AMS1117 GND.
2. **VCC & CH_PD** — Connect ESP-01S VCC and CH_PD (Chip Enable) to AMS1117 VOUT+.
   > ⚠️ Do **not** power the ESP-01S directly from the USB-TTL adapter's 3.3V pin — it cannot supply sufficient current.
3. **TX/RX** — Cross-connect the serial lines:
   - ESP-01S `TXD` → USB-TTL `RXD`
   - ESP-01S `RXD` → USB-TTL `TXD`
4. **Flashing Mode** — Connect ESP-01S `GPIO0` directly to GND. This puts the module into bootloader mode, ready to receive new firmware.

---

## 3D Printable Parts

The `3D Printable Parts/` folder contains all parts needed to assemble the scanner mechanism.

The tripod used in this project is sourced from:
**[Folding Tripod — MakerWorld](https://makerworld.com/tr/models/671280-folding-tripod-two-sizes?from=search#profileId-599018)**

---

## Inspiration

This project is inspired by the original ESP8266 3D scanner by **[bitluni](https://bitluni.net/3d-scanner)**.

---

## License

This project is licensed under the [MIT License](LICENSE).
