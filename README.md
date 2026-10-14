# 🏭 Industrial Safety Monitoring System

An ESP32-based industrial safety monitoring prototype designed to improve workplace safety through sensor-based hazard detection, RFID authentication, automated responses, and real-time alerts.

## 📌 Project Overview

Industrial environments can face hazards such as gas leaks, excessive temperatures, and fire-related emergencies. This project integrates sensors and control modules with an ESP32 to monitor operating conditions and support timely safety responses.

The system combines hardware monitoring, access control, alerts, and machine control in a single prototype.

## 🎯 Objectives

- Monitor potentially hazardous environmental conditions.
- Implement RFID-based worker access control.
- Detect gas concentration changes using an MQ-2 sensor.
- Monitor temperature and humidity using a DHT22 sensor.
- Provide audible and visual alerts when configured thresholds are exceeded.
- Demonstrate machine control using a relay module.
- Control an emergency exit mechanism using a servo motor.
- Display system information and record relevant events.

## ⚙️ Hardware Components

| Component | Purpose |
|---|---|
| ESP32 Development Board | Main controller |
| MFRC522 RFID Reader | RFID-based access control |
| MQ-2 Gas Sensor | Gas and smoke detection |
| DHT22 Sensor | Temperature and humidity monitoring |
| Relay Module | Machine ON/OFF control |
| Servo Motor | Emergency exit mechanism |
| Buzzer | Audible warning |
| LEDs | Visual status indicators |
| Push Buttons | Manual controls and emergency stop |
| LCD | System status display |
| Connecting Wires | Hardware connections |

## ✨ Key Features

- **RFID Authentication:** Demonstrates access control using RFID cards.
- **Gas Monitoring:** Reads MQ-2 sensor values and compares them with a configured threshold.
- **Temperature Monitoring:** Tracks environmental temperature and supports high-temperature alerts.
- **Machine Control:** Uses a relay module to demonstrate machine switching.
- **Emergency Stop:** Provides a manual emergency-control input.
- **Servo-Controlled Exit:** Demonstrates opening an emergency exit mechanism.
- **Visual and Audible Alerts:** Uses LEDs, an LCD, and a buzzer to communicate system status.
- **Event Logging:** Supports recording system events with timestamps.
- **Remote Notifications:** Supports Telegram alerts for configured safety events.

Feature availability depends on the current firmware configuration and hardware setup.

## 🔌 Hardware Circuit

The circuit diagram is available in [`hardware/Circuit.png`](hardware/Circuit.png).

The Wokwi circuit configuration is available in [`diagram.json`](diagram.json).

## 🧑‍💻 Software and Technologies

- C/C++ for embedded programming
- ESP32 development board
- PlatformIO
- Arduino framework
- Wokwi for circuit simulation
- Sensor and peripheral libraries
- Telegram Bot API for remote notifications, where configured

## 🚀 Getting Started

### Prerequisites

- Visual Studio Code
- PlatformIO IDE extension
- An ESP32 development board or compatible simulation environment
- Required sensors and modules
- USB cable and suitable power supply for physical hardware

### Setup Instructions

1. Clone or download this repository.
2. Open the project folder in Visual Studio Code.
3. Install the PlatformIO IDE extension if it is not already installed.
4. Allow PlatformIO to install the dependencies specified in `platformio.ini`.
5. Connect the ESP32 board or configure the supported simulation environment.
6. Verify sensor connections, GPIO assignments, and configured thresholds.
7. Build the firmware using PlatformIO.
8. Upload the firmware to the ESP32 and open the serial monitor to inspect system output.

### Simulation

The project includes `diagram.json` and `wokwi.toml` for its Wokwi setup. Open the project in a compatible Wokwi environment and verify that the component definitions and pin assignments match the firmware.

## 🔐 Configuration and Security

- Do not commit Wi-Fi passwords, Telegram bot tokens, or other private credentials.
- Keep private configuration values outside tracked source files.
- Replace placeholder credentials with your own local configuration before testing.
- Verify sensor thresholds and emergency-control behavior before connecting real equipment.

## 🧪 Testing

The prototype can be evaluated using scenarios such as:

1. Normal operating conditions.
2. Authorized and unauthorized RFID cards.
3. Gas sensor readings above the configured threshold.
4. Temperature readings above the configured threshold.
5. Relay switching and emergency-stop activation.
6. Servo-controlled exit operation.
7. Buzzer, LED, LCD, and Telegram alert behavior.
8. Event logging and timestamp verification, where implemented.

## ⚠️ Limitations

This project is an educational prototype and is not a certified industrial safety system. Sensor readings, threshold settings, actuator behavior, and alert delivery must be validated before any real-world application.

It must not replace certified industrial safety equipment or emergency procedures.

## 👥 Project Information

- **Project:** Industrial Safety Monitoring System
- **Controller:** ESP32
- **Development Environment:** PlatformIO
- **Simulation:** Wokwi
- **Project Type:** Embedded Systems / Industrial Safety

## 📄 License

No license has been assigned to this repository yet. Reuse and redistribution permissions should be specified by adding an appropriate license file.

---

*Developed as an academic project exploring embedded systems, environmental monitoring, access control, and industrial safety automation.*
