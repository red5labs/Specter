# Specter WiFi Scanner

Specter is an ESP32-based WiFi scanning and tracking tool with a responsive web interface. It creates its own access point, allowing you to connect with any device and scan for nearby WiFi networks.

## Features

- **Real-time WiFi Scanning**: Continuously scans and displays all nearby WiFi networks
- **Signal Tracking**: Monitor signal strength of specific devices over time
- **Responsive Dashboard**: Works on desktop, tablet, and mobile devices
- **Visual Signal Graphs**: Track signal strength changes with interactive charts
- **Network Analytics**: View security types, signal quality, and channel information
- **Data Export**: Export scan results in CSV, JSON, or text report formats

## Hardware Requirements

- ESP32 development board
- USB cable for power/programming
- Power bank (optional, for portable use)

## Software Requirements

- Arduino IDE
- Required libraries:
  - WiFi
  - WebServer
  - WebSocketsServer
  - ArduinoJson

## Installation

1. Clone this repository:
   ```
   git clone https://github.com/red5labs/Specter_v3.git
   ```

2. Open `Specter_v3.ino` in Arduino IDE

3. Install required libraries through Arduino Library Manager

4. Select your ESP32 board in the Arduino IDE

5. Upload the sketch to your ESP32

## Usage

1. Power on your ESP32 device

2. Connect to the "Specter" WiFi network
   - SSID: `Specter`
   - Password: `specter123`

3. Open a web browser and navigate to `192.168.4.1`

4. Use the dashboard to:
   - Start/stop scanning for networks
   - Track signal strength of specific devices
   - Filter and sort network results
   - Export data in various formats

## Dashboard Features

- **Continuous Scanning**: Real-time updates of all nearby networks
- **Signal Tracking**: Monitor signal strength of a specific device over time
- **Filtering Options**: Filter by network name, security type, signal strength, or channel
- **Signal Quality Charts**: View signal strength history with configurable time ranges
- **Data Export**: Save scan results in multiple formats
- **Network Details**: View detailed information about each discovered network

## Security Note

This tool is designed for educational and network troubleshooting purposes only. Use responsibly and respect privacy laws in your jurisdiction.

## Customization

You can modify the following in the code:

- WiFi access point name and password (lines 5-6)
- Scan interval (line 15)
- Tracking interval (line 16)
- Web interface appearance (HTML/CSS in the `handleRoot()` function)

## License

[MIT License](LICENSE)

## Acknowledgements

- This project uses [ArduinoJson](https://arduinojson.org/) library
- Built with ESP32 Arduino core
