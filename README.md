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


## ESP32 Limitations

While the ESP32 is a powerful microcontroller for WiFi projects, it does have several limitations to be aware of when using Specter:

### WiFi Scanning Limitations

- **Scanning Speed**: WiFi scanning on ESP32 takes time (typically 2-3 seconds per full scan) as it must switch channels and listen on each one.
- **Channel Hopping**: The ESP32 can only listen on one WiFi channel at a time, requiring channel switching to detect all networks.
- **Signal Tracking Latency**: When tracking a specific device, there may be a delay in the initial connection as the ESP32 needs to scan all channels to locate it.
- **Concurrent Operations**: While scanning, other WiFi operations may be temporarily paused or slowed down.

### Hardware Limitations

- **Range**: The built-in antenna has limited range compared to dedicated WiFi adapters.
- **Power Consumption**: Active WiFi scanning draws significant power, reducing battery life in portable setups.
- **Heat Generation**: Extended scanning sessions can cause the ESP32 to heat up.

### Memory Constraints

- **Network Storage**: The ESP32 has limited RAM, which restricts how many networks can be cached in memory.
- **Client Connections**: The soft AP mode supports a maximum of 4-5 simultaneous client connections.
- **JSON Size**: Large scan results might require careful memory management to avoid buffer overflows.

### Accuracy Considerations

- **Signal Strength**: RSSI measurements have limited precision and can fluctuate.
- **Refresh Rates**: Real-time tracking is constrained by scan times, so rapid movements may not be accurately reflected.
- **Hidden Networks**: The ESP32 may have difficulty consistently detecting hidden networks.

### Optimizations Implemented

Despite these limitations, Specter implements several optimizations:
- Channel caching to speed up tracking of known devices
- Asynchronous scanning to improve responsiveness
- Targeted scanning for tracked devices
- Reduced tracking intervals for more frequent updates

### WiFi Frequency Support

- **2.4 GHz Band Only**: The ESP32 currently supports only the 2.4 GHz WiFi band (IEEE 802.11 b/g/n)
  - Channels 1-14 (depending on region)
  - 40 MHz bandwidth support
  - No 5 GHz support available

- **Frequency Range**: 2400 MHz to 2483.5 MHz
  - Channels 1-11: Available worldwide
  - Channels 12-13: Available in most regions outside North America
  - Channel 14: Limited to Japan for 802.11b only

- **Range Characteristics**: 
  - Indoor range: Approximately 30-50 meters (100-165 feet) under optimal conditions
  - Outdoor line-of-sight range: Up to 150 meters (490 feet) with clear line of sight
  - Range is significantly affected by obstacles, interference, and antenna orientation
  
- **Signal Sensitivity**:
  - Receiver sensitivity: Around -97 dBm (802.11b) to -89 dBm (802.11n)
  - Transmit power: +20 dBm maximum (may be limited by regional regulations)

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
