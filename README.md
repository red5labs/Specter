# Specter ESP32 Wi-Fi Scanner - Proof of Concept
ESP32 Wireless network scanner

This project is a **proof-of-concept** that uses an **ESP32 microcontroller** to passively scan for nearby wireless networks and display basic information about them. It demonstrates the capability of low-cost IoT hardware to perform network discovery, a technique useful in wireless diagnostics, penetration testing, or educational research.

## Features

* Scans for 2.4GHz Wi-Fi networks using the ESP32’s built-in capabilities
* Displays SSID, RSSI (signal strength), MAC address (BSSID), channel, and encryption type
* Updates results in real-time on a serial monitor or web interface on a laptop, tablet, or mobile device
* Lightweight codebase suitable for experimentation and learning

## Hardware Required

* [ESP32 Dev Board (e.g., WROOM-32 or WROVER)](https://www.espressif.com/en/products/socs/esp32)
* USB cable for programming
* (Optional) OLED display or web interface for live data display

## Getting Started

1. **Install the ESP32 board support** in the Arduino IDE or use PlatformIO.
2. **Clone this repository**:

   ```bash
   git clone https://github.com/red5labs/Specter.git
   cd esp32-wifi-scanner
   ```
3. **Upload the code** to your ESP32.
4. Connect to the ESP32 using it's built-in access point (SSID: Specter / PW: specter1234)
5. Open a browser and navigate to 192.168.4.1
6. (Optional) Open the **Serial Monitor** at 115200 baud to view scanned networks.

## Notes

* This tool is for educational and research purposes only. Always ensure you have authorization before scanning wireless networks.
* Future enhancements may include logging, filtering by signal strength, or serving results via a lightweight web server on the ESP32.

## License

MIT License — feel free to fork, modify, and contribute.
