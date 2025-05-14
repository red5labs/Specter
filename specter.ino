#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <esp_wifi.h>

// WiFi credentials for your ESP32 access point
const char* ssid = "Specter_Scanner";
const char* password = "spectrum123";

// DNS server
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Web server
WebServer server(80);

// Scan result array - use compatible structures with WiFi library
#define MAX_AP_RECORDS 20
struct APRecord {
  uint8_t bssid[6];
  uint8_t ssid[33]; // 32 bytes + null terminator
  int8_t rssi;
  int channel;
  wifi_auth_mode_t encryptionType;
};
APRecord ap_records[MAX_AP_RECORDS];
int ap_count = 0;

// Device tracking
#define MAX_DEVICES 100
struct DeviceInfo {
  String mac;
  String ssid;
  int rssi;
  bool isTracking;
  unsigned long lastSeen;
  int channel;
  String encryptionType;
};
DeviceInfo devices[MAX_DEVICES];
int deviceCount = 0;

// Signal history for tracked devices
#define MAX_HISTORY 100
struct SignalHistory {
  String mac;
  int rssiValues[MAX_HISTORY];
  unsigned long timestamps[MAX_HISTORY];
  int count;
  int index;
};
SignalHistory trackedDeviceHistory;

// Function declarations
String getDefaultHTML();
String getEncryptionTypeString(wifi_auth_mode_t encryptionType);

void setup() {
  Serial.begin(115200);
  Serial.println("Specter WiFi Scanner starting...");
  
  // Initialize SPIFFS - but we won't rely on it
  if (!SPIFFS.begin(true)) {
    Serial.println("An error occurred while mounting SPIFFS - continuing with defaults");
  } else {
    Serial.println("SPIFFS mounted successfully - but we'll use defaults anyway");
  }

  // Set up ESP32 as an access point with station mode enabled
  WiFi.mode(WIFI_AP_STA);
  
  // Configure WiFi scanning parameters
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  
  // Create access point
  WiFi.softAP(ssid, password);
  
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Set up DNS server to redirect all domains to the ESP32
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  Serial.println("DNS server started");
  
  // Perform initial scan
  scanWiFiNetworks();
  updateDeviceList();
  
  // Set up web server routes - simplified to only the necessary endpoints
  server.on("/", HTTP_GET, handleRoot);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/track", HTTP_POST, handleTrack);
  server.on("/signal", HTTP_GET, handleSignal);
  server.on("/test", HTTP_GET, []() {
    server.send(200, "text/plain", "Server is running");
  });
  
  // Handle files not found - redirect to home page
  server.onNotFound([]() {
    Serial.println("404 Not Found: " + server.uri());
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });
  
  // Start the server
  server.begin();
  Serial.println("HTTP server started");
  
  // Initialize tracking history
  trackedDeviceHistory.count = 0;
  trackedDeviceHistory.index = 0;
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  
  // Perform periodic WiFi scan
  static unsigned long lastScanTime = 0;
  if (millis() - lastScanTime > 2000) { // Scan every 2 seconds
    scanWiFiNetworks();
    updateDeviceList();
    lastScanTime = millis();
  }
}

void scanWiFiNetworks() {
  Serial.println("Starting WiFi scan...");
  
  // Ensure we're in the right WiFi mode
  if (WiFi.getMode() != WIFI_AP_STA) {
    WiFi.mode(WIFI_AP_STA);
    delay(100);
  }
  
  // Delete any previous scan results
  WiFi.scanDelete();
  
  // Start the scan - scan all channels, include hidden networks
  // Parameters: async, show_hidden, passive, max_ms_per_channel, channel
  int scanResult = WiFi.scanNetworks(false, true, false, 300);
  Serial.printf("WiFi scan completed, found %d networks\n", scanResult);
  
  if (scanResult == 0) {
    Serial.println("No networks found");
    ap_count = 0;
  } else if (scanResult > 0) {
    // Update our count, capped at our array size
    ap_count = (scanResult > MAX_AP_RECORDS) ? MAX_AP_RECORDS : scanResult;
    
    // Copy scan results to our records array
    for (int i = 0; i < ap_count; i++) {
      // Get network info
      memcpy(ap_records[i].bssid, WiFi.BSSID(i), 6);
      
      // Copy SSID safely
      String networkSSID = WiFi.SSID(i);
      if (networkSSID.length() == 0) {
        networkSSID = "<Hidden Network>";
      }
      
      // Ensure SSID buffer is zeroed
      memset(ap_records[i].ssid, 0, sizeof(ap_records[i].ssid));
      strncpy((char*)ap_records[i].ssid, networkSSID.c_str(), 32);
      
      // Set RSSI
      ap_records[i].rssi = WiFi.RSSI(i);
      
      // Set channel
      ap_records[i].channel = WiFi.channel(i);
      
      // Set encryption type
      ap_records[i].encryptionType = WiFi.encryptionType(i);
      
      Serial.printf("Found network: %s, MAC: %02X:%02X:%02X:%02X:%02X:%02X, RSSI: %d, Channel: %d, Encryption: %d\n", 
                   networkSSID.c_str(),
                   ap_records[i].bssid[0], ap_records[i].bssid[1], ap_records[i].bssid[2],
                   ap_records[i].bssid[3], ap_records[i].bssid[4], ap_records[i].bssid[5],
                   ap_records[i].rssi,
                   ap_records[i].channel,
                   ap_records[i].encryptionType);
    }
  } else {
    Serial.printf("WiFi scan error: %d\n", scanResult);
    ap_count = 0;
  }
  
  // Clean up scan results to free memory
  WiFi.scanDelete();
}

void updateDeviceList() {
  Serial.printf("Updating device list with %d access points\n", ap_count);
  
  for (int i = 0; i < ap_count; i++) {
    String mac = formatMac(ap_records[i].bssid);
    String ssid = String(reinterpret_cast<char*>(ap_records[i].ssid));
    int rssi = ap_records[i].rssi;
    int channel = ap_records[i].channel;
    String encType = getEncryptionTypeString(ap_records[i].encryptionType);
    
    Serial.printf("Processing AP %d: SSID='%s', MAC=%s, RSSI=%d, Channel=%d, Encryption=%s\n", 
                  i, ssid.c_str(), mac.c_str(), rssi, channel, encType.c_str());
    
    // Check if device is already in list
    bool found = false;
    for (int j = 0; j < deviceCount; j++) {
      if (devices[j].mac == mac) {
        devices[j].rssi = rssi;
        devices[j].channel = channel;
        devices[j].encryptionType = encType;
        devices[j].lastSeen = millis();
        
        // If this device is being tracked, update its history
        if (devices[j].isTracking) {
          updateSignalHistory(mac, rssi);
        }
        
        found = true;
        Serial.printf("Updated existing device at index %d\n", j);
        break;
      }
    }
    
    // Add new device to list
    if (!found && deviceCount < MAX_DEVICES) {
      devices[deviceCount].mac = mac;
      devices[deviceCount].ssid = ssid;
      devices[deviceCount].rssi = rssi;
      devices[deviceCount].channel = channel;
      devices[deviceCount].encryptionType = encType;
      devices[deviceCount].isTracking = false;
      devices[deviceCount].lastSeen = millis();
      Serial.printf("Added new device at index %d\n", deviceCount);
      deviceCount++;
    }
  }
  
  // Remove stale devices (not seen for 60 seconds)
  int removedCount = 0;
  for (int i = 0; i < deviceCount; i++) {
    if (millis() - devices[i].lastSeen > 60000) {
      // If this was a tracked device, stop tracking
      if (devices[i].isTracking) {
        trackedDeviceHistory.count = 0;
        trackedDeviceHistory.index = 0;
        Serial.printf("Stopped tracking device %s (stale)\n", devices[i].mac.c_str());
      }
      
      // Remove device by shifting array
      Serial.printf("Removing stale device: %s (%s)\n", 
                    devices[i].ssid.c_str(), devices[i].mac.c_str());
      for (int j = i; j < deviceCount - 1; j++) {
        devices[j] = devices[j + 1];
      }
      deviceCount--;
      removedCount++;
      i--; // Recheck the current index
    }
  }
  
  Serial.printf("Device list updated. Total: %d, Added/Updated: %d, Removed: %d\n", 
                deviceCount, ap_count, removedCount);
}

void updateSignalHistory(String mac, int rssi) {
  trackedDeviceHistory.mac = mac;
  trackedDeviceHistory.rssiValues[trackedDeviceHistory.index] = rssi;
  trackedDeviceHistory.timestamps[trackedDeviceHistory.index] = millis();
  
  if (trackedDeviceHistory.count < MAX_HISTORY) {
    trackedDeviceHistory.count++;
  }
  
  trackedDeviceHistory.index = (trackedDeviceHistory.index + 1) % MAX_HISTORY;
}

String formatMac(uint8_t* mac) {
  char macStr[18] = { 0 };
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}

void handleRoot() {
  Serial.println("Handling root request - using all-in-one HTML");
  server.send(200, "text/html", getDefaultHTML());
}

void handleScan() {
  // Force an immediate scan when requested
  Serial.println("Scan endpoint called - performing immediate scan");
  scanWiFiNetworks();
  updateDeviceList();
  
  DynamicJsonDocument doc(4096);
  
  // Add metadata about the scan
  doc["success"] = true;
  doc["scanCount"] = ap_count;
  doc["deviceCount"] = deviceCount;
  doc["timestamp"] = millis();
  
  // Create the devices array
  JsonArray array = doc.createNestedArray("devices");
  
  Serial.printf("Sending %d devices as JSON response\n", deviceCount);
  for (int i = 0; i < deviceCount; i++) {
    JsonObject obj = array.createNestedObject();
    obj["mac"] = devices[i].mac;
    obj["ssid"] = devices[i].ssid;
    obj["rssi"] = devices[i].rssi;
    obj["isTracking"] = devices[i].isTracking;
    obj["channel"] = devices[i].channel;
    obj["encryption"] = devices[i].encryptionType;
    
    // Add signal strength classification
    String signalStrength = "unknown";
    if (devices[i].rssi >= -25) {
      signalStrength = "excellent";
    } else if (devices[i].rssi >= -45) {
      signalStrength = "good";
    } else if (devices[i].rssi >= -65) {
      signalStrength = "fair";
    } else if (devices[i].rssi >= -85) {
      signalStrength = "poor";
    } else {
      signalStrength = "poor";
    }
    obj["signalStrength"] = signalStrength;
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleTrack() {
  String mac = server.arg("mac");
  
  // Stop tracking any previously tracked device
  for (int i = 0; i < deviceCount; i++) {
    devices[i].isTracking = false;
  }
  
  // Start tracking the new device
  for (int i = 0; i < deviceCount; i++) {
    if (devices[i].mac == mac) {
      devices[i].isTracking = true;
      trackedDeviceHistory.count = 0;
      trackedDeviceHistory.index = 0;
      trackedDeviceHistory.mac = mac;
      updateSignalHistory(mac, devices[i].rssi);
      break;
    }
  }
  
  server.send(200, "text/plain", "OK");
}

void handleSignal() {
  String mac = server.arg("mac");
  
  DynamicJsonDocument doc(4096);
  doc["mac"] = trackedDeviceHistory.mac;
  
  JsonArray rssiArray = doc.createNestedArray("rssi");
  JsonArray timeArray = doc.createNestedArray("time");
  
  // Get the current index (most recent entry)
  int startIdx = (trackedDeviceHistory.index - 1 + MAX_HISTORY) % MAX_HISTORY;
  
  // Add entries in chronological order
  for (int i = 0; i < trackedDeviceHistory.count; i++) {
    int idx = (startIdx - trackedDeviceHistory.count + i + 1 + MAX_HISTORY) % MAX_HISTORY;
    rssiArray.add(trackedDeviceHistory.rssiValues[idx]);
    timeArray.add(trackedDeviceHistory.timestamps[idx]);
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

String getDefaultHTML() {
  return R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Specter - Wireless Device Scanner</title>
    <style>
        /* Critical CSS for functionality */
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
            font-family: Arial, sans-serif;
        }
        body {
            background-color: #f4f4f4;
            color: #333;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
            padding: 20px;
        }
        header {
            text-align: center;
            margin-bottom: 20px;
            padding: 20px 0;
            background-color: #2c3e50;
            color: white;
            border-radius: 8px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
        }
        h1 { font-size: 2.5rem; }
        h2 { font-size: 1.2rem; margin-top: 5px; font-weight: normal; }
        h3 { margin-bottom: 15px; color: #2c3e50; }
        .controls {
            display: flex;
            justify-content: center;
            margin-bottom: 20px;
        }
        button {
            padding: 12px 24px;
            background-color: #3498db;
            color: white;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 1rem;
            transition: background-color 0.3s;
        }
        button:hover {
            background-color: #2980b9;
        }
        .main-content {
            display: flex;
            flex-direction: column;
            gap: 20px;
        }
        @media (min-width: 768px) {
            .main-content {
                flex-direction: row;
            }
            .device-list-container, .signal-graph-container {
                flex: 1;
            }
        }
        .device-list-container, .signal-graph-container {
            background-color: #f5f5f5;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
        }
        .device-list {
            max-height: 500px;
            overflow-y: auto;
        }
        .device-item {
            padding: 15px;
            margin-bottom: 10px;
            background-color: white;
            border-radius: 4px;
            box-shadow: 0 1px 3px rgba(0,0,0,0.1);
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .device-info {
            flex: 1;
        }
        .device-name {
            font-weight: bold;
            margin-bottom: 5px;
        }
        .device-mac {
            color: #7f8c8d;
            font-size: 0.9rem;
        }
        .device-details {
            color: #7f8c8d;
            font-size: 0.9rem;
            margin-top: 3px;
        }
        .device-signal {
            background-color: #34495e;
            color: white;
            padding: 5px 10px;
            border-radius: 4px;
            margin-left: 10px;
        }
        .track-btn {
            margin-left: 10px;
            background-color: #2ecc71;
            padding: 5px 10px;
        }
        .track-btn:hover {
            background-color: #27ae60;
        }
        .tracked-device-info {
            margin-bottom: 20px;
            padding: 15px;
            background-color: white;
            border-radius: 4px;
            box-shadow: 0 1px 3px rgba(0,0,0,0.1);
        }
        .signal-strength-indicator {
            margin-top: 20px;
            display: flex;
            align-items: center;
        }
        .signal-meter {
            flex: 1;
            height: 30px;
            background-color: #ecf0f1;
            border-radius: 15px;
            overflow: hidden;
            margin-right: 15px;
        }
        .signal-bar {
            height: 100%;
            width: 0%;
            background: linear-gradient(to right, #FF0000, #FFCC00, #8AFF26, #1AFF1A);
            transition: width 0.5s ease;
        }
        .signal-value {
            font-size: 1.2rem;
            font-weight: bold;
            width: 50px;
            text-align: right;
        }
        #statusMessage {
            text-align: center;
            margin-bottom: 20px;
            padding: 10px;
            border-radius: 4px;
            background-color: #f8f9fa;
            transition: all 0.3s ease;
        }

        /* Modal styles */
        .modal {
            display: none;
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background-color: rgba(0, 0, 0, 0.7);
            z-index: 1000;
            justify-content: center;
            align-items: center;
        }
        
        .modal.active {
            display: flex;
        }
        
        .modal-content {
            background-color: white;
            border-radius: 8px;
            width: 80%;
            max-width: 650px;
            max-height: 85vh;
            overflow-y: auto;
            padding: 15px;
            box-shadow: 0 5px 15px rgba(0, 0, 0, 0.3);
            position: relative;
        }
        
        .close-modal {
            position: absolute;
            top: 10px;
            right: 15px;
            font-size: 24px;
            cursor: pointer;
            color: #777;
        }
        
        .close-modal:hover {
            color: #333;
        }
        
        .modal-header {
            border-bottom: 1px solid #eee;
            margin-bottom: 15px;
            padding-bottom: 10px;
        }
        
        .modal-title {
            font-size: 1.5rem;
            margin-right: 30px;
        }
        
        .tracking-details {
            display: flex;
            flex-wrap: wrap;
            gap: 20px;
            margin-bottom: 20px;
        }
        
        .tracking-info {
            flex: 1;
            min-width: 250px;
        }
        
        .tracking-controls {
            display: flex;
            justify-content: space-between;
            margin-top: 20px;
            padding-top: 15px;
            border-top: 1px solid #eee;
        }
        
        .signal-scale {
            display: flex;
            justify-content: space-between;
            margin-top: 5px;
            font-size: 0.8rem;
            color: #777;
        }
        
        .signal-quality {
            font-weight: bold;
            text-transform: uppercase;
            letter-spacing: 1px;
            font-size: 1.1rem;
            padding: 5px 10px;
            border-radius: 4px;
            margin-bottom: 10px;
            display: inline-block;
        }
        
        /* Signal strength color scale based on intensity */
        .excellent { background-color: #1AFF1A; color: black; } /* Bright green */
        .good { background-color: #8AFF26; color: black; }      /* Green-yellow */
        .fair { background-color: #FFCC00; color: black; }      /* Yellow-orange */
        .poor { background-color: #FF3300; color: white; }      /* Red-orange */
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>Specter</h1>
            <h2>Wireless Device Scanner</h2>
        </header>
        
        <div class="controls">
            <button id="scanBtn">Scan for Devices</button>
        </div>
        
        <div class="main-content">
            <div id="statusMessage">
                Initializing...
            </div>
            
            <div class="device-list-container">
                <h3>Detected Devices</h3>
                <div id="deviceList" class="device-list"></div>
            </div>
        </div>
    </div>
    
    <!-- Tracking Modal -->
    <div id="trackingModal" class="modal">
        <div class="modal-content">
            <span class="close-modal" id="closeModal">&times;</span>
            <div class="modal-header">
                <h3 class="modal-title">Signal Strength Tracker</h3>
            </div>
            
            <div class="tracking-details">
                <div class="tracking-info">
                    <h4 id="trackingDeviceName">Device</h4>
                    <p id="trackingDeviceMAC">MAC: --</p>
                    <p>Current Signal: <span id="trackingSignalValue">-- dBm</span></p>
                    <div id="trackingSignalQuality" class="signal-quality">--</div>
                    <p class="tracking-tip">Walk around with your device to locate the signal source.</p>
                </div>
                
                <div class="tracking-graph" style="flex: 2; min-width: 300px;">
                    <canvas id="trackingChart" style="width: 100%; height: 180px;"></canvas>
                    <div class="signal-meter" style="margin: 10px 0;">
                        <div id="trackingSignalBar" class="signal-bar"></div>
                    </div>
                    <div class="signal-scale">
                        <span>-95 dBm (Weak)</span>
                        <span>-65 dBm</span>
                        <span>-25 dBm (Strong)</span>
                    </div>
                </div>
            </div>
            
            <div class="tracking-controls">
                <button id="stopTrackingBtn">Stop Tracking</button>
                <div>
                    <span id="trackingDuration">00:00</span>
                    <span id="trackingSamples">(0 samples)</span>
                </div>
            </div>
        </div>
    </div>
    
    <script>
        // Simple Chart Implementation
        class SimpleChart {
            constructor(ctx, config) {
                this.canvas = ctx.canvas;
                this.ctx = ctx;
                this.data = config.data;
                this.options = config.options;
                this.render();
            }
            
            update() {
                this.render();
            }
            
            render() {
                const ctx = this.ctx;
                const canvas = this.canvas;
                const data = this.data;
                const options = this.options;
                
                // Clear canvas
                ctx.clearRect(0, 0, canvas.width, canvas.height);
                
                if (!data.datasets[0].data.length) return;
                
                const width = canvas.width;
                const height = canvas.height;
                const padding = 40;
                
                // Get min/max values
                const maxY = options.scales.y.max || -25;
                const minY = options.scales.y.min || -95;
                
                // Draw axes
                ctx.beginPath();
                ctx.strokeStyle = '#888';
                ctx.lineWidth = 1;
                ctx.moveTo(padding, padding);
                ctx.lineTo(padding, height - padding);
                ctx.lineTo(width - padding, height - padding);
                ctx.stroke();
                
                // Plot data points as bars
                const points = data.datasets[0].data;
                const datasetColor = data.datasets[0].borderColor;
                const barCount = points.length;
                const availableWidth = width - 2 * padding;
                const barWidth = Math.max(4, availableWidth / (barCount * 2)); // Ensure minimum width
                const barSpacing = (availableWidth - barWidth * barCount) / (barCount + 1);
                
                // Function to get color based on RSSI value
                const getBarColor = (rssi) => {
                    if (rssi >= -25) return '#1AFF1A'; // Excellent
                    if (rssi >= -45) return '#8AFF26'; // Good
                    if (rssi >= -65) return '#FFCC00'; // Fair
                    return '#FF3300';                  // Poor
                };
                
                const availableHeight = height - 2 * padding;
                
                // Draw bars
                for (let i = 0; i < points.length; i++) {
                    // Calculate bar position and height
                    const x = padding + barSpacing * (i + 1) + barWidth * i;
                    
                    // Convert RSSI to bar height - stronger signals (less negative) should have taller bars
                    // Map from RSSI range (-95 to -25) to height (0 to availableHeight)
                    const barHeight = ((points[i] - minY) / (maxY - minY)) * availableHeight;
                    const y = height - padding - barHeight;
                    
                    // Draw the bar
                    ctx.fillStyle = getBarColor(points[i]);
                    ctx.fillRect(x, y, barWidth, barHeight);
                    
                    // Draw value above bar (optional)
                    if (points.length < 15) { // Only show values if not too crowded
                        ctx.fillStyle = '#333';
                        ctx.font = '10px Arial';
                        ctx.textAlign = 'center';
                        ctx.fillText(points[i], x + barWidth / 2, y - 5);
                    }
                }
                
                // Draw axis labels
                ctx.fillStyle = '#333';
                ctx.font = '12px Arial';
                ctx.textAlign = 'center';
                ctx.fillText('Time', width / 2, height - 10);
                
                ctx.save();
                ctx.translate(15, height / 2);
                ctx.rotate(-Math.PI / 2);
                ctx.textAlign = 'center';
                ctx.fillText('Signal Strength (dBm)', 0, 0);
                ctx.restore();
            }
        }
        
        // DOM Elements
        const statusMessage = document.getElementById('statusMessage');
        const scanBtn = document.getElementById('scanBtn');
        const deviceList = document.getElementById('deviceList');
        
        // Modal elements
        const trackingModal = document.getElementById('trackingModal');
        const closeModal = document.getElementById('closeModal');
        const trackingDeviceName = document.getElementById('trackingDeviceName');
        const trackingDeviceMAC = document.getElementById('trackingDeviceMAC');
        const trackingSignalValue = document.getElementById('trackingSignalValue');
        const trackingSignalQuality = document.getElementById('trackingSignalQuality');
        const trackingSignalBar = document.getElementById('trackingSignalBar');
        const trackingDuration = document.getElementById('trackingDuration');
        const trackingSamples = document.getElementById('trackingSamples');
        const stopTrackingBtn = document.getElementById('stopTrackingBtn');
        
        // App State
        let trackingChart;
        let trackingChartInitialized = false;
        let currentTrackedMAC = null;
        let updateInterval;
        let trackingStartTime = 0;
        let sampleCount = 0;
        
        // Initialize the application when DOM is loaded
        document.addEventListener('DOMContentLoaded', function() {
            statusMessage.innerHTML = 'Welcome to Specter! Testing connection...';
            
            // Test server connection
            fetch('/test')
                .then(response => {
                    if (response.ok) {
                        statusMessage.innerHTML = 'Server is connected!';
                        statusMessage.style.color = 'green';
                        
                        // Auto-scan after successful connection
                        setTimeout(scanDevices, 500);
                        return response.text();
                    }
                    throw new Error('Server connection failed');
                })
                .catch(error => {
                    statusMessage.innerHTML = 'Error connecting to server. Please check your connection.';
                    statusMessage.style.color = 'red';
                    console.error('Error:', error);
                });
            
            // Set up event listeners
            scanBtn.addEventListener('click', scanDevices);
            closeModal.addEventListener('click', stopTracking);
            stopTrackingBtn.addEventListener('click', stopTracking);
        });
        
        // Scan for wireless devices
        function scanDevices() {
            statusMessage.innerHTML = 'Scanning for devices...';
            statusMessage.style.color = 'blue';
            
            fetch('/scan')
                .then(response => {
                    if (response.ok) return response.json();
                    throw new Error('Scan failed');
                })
                .then(data => {
                    // Check if we have the devices in the new format
                    const devices = data.devices || data;
                    
                    // Display meta information if available
                    if (data.success) {
                        statusMessage.innerHTML = `Found ${data.deviceCount} devices (scanned ${data.scanCount} networks)`;
                    } else {
                        statusMessage.innerHTML = `Found ${devices.length} devices`;
                    }
                    
                    statusMessage.style.color = 'green';
                    displayDevices(devices);
                })
                .catch(error => {
                    statusMessage.innerHTML = 'Error scanning for devices';
                    statusMessage.style.color = 'red';
                    console.error('Scan error:', error);
                });
        }
        
        // Display the list of detected devices
        function displayDevices(devices) {
            deviceList.innerHTML = '';
            
            if (devices.length === 0) {
                deviceList.innerHTML = '<p>No devices found</p>';
                return;
            }
            
            // Sort devices by signal strength (strongest first)
            devices.sort((a, b) => b.rssi - a.rssi);
            
            devices.forEach(device => {
                const deviceElement = document.createElement('div');
                deviceElement.className = 'device-item';
                
                const rssiClass = device.signalStrength || getRssiClass(device.rssi);
                
                deviceElement.innerHTML = `
                    <div class="device-info">
                        <div class="device-name">${device.ssid || 'Unknown Device'}</div>
                        <div class="device-mac">${device.mac}</div>
                        <div class="device-details">Ch: ${device.channel || 'N/A'} | ${device.encryption || 'Unknown'}</div>
                    </div>
                    <div class="device-signal ${rssiClass}">${device.rssi} dBm</div>
                    <button class="track-btn" data-mac="${device.mac}" data-ssid="${device.ssid || 'Unknown Device'}">Track</button>
                `;
                
                deviceList.appendChild(deviceElement);
                
                // Add event listener to track button
                const trackBtn = deviceElement.querySelector('.track-btn');
                trackBtn.addEventListener('click', function() {
                    const mac = this.getAttribute('data-mac');
                    const ssid = this.getAttribute('data-ssid');
                    startTracking(mac, ssid);
                });
                
                // Highlight currently tracked device
                if (device.isTracking) {
                    deviceElement.classList.add('tracked');
                }
            });
        }
        
        // Start tracking a device
        function startTracking(mac, ssid) {
            // Show modal
            trackingModal.classList.add('active');
            trackingDeviceName.textContent = ssid;
            trackingDeviceMAC.textContent = `MAC: ${mac}`;
            
            // Find device details in the device list
            let deviceDetails = "";
            for (let i = 0; i < deviceList.children.length; i++) {
                const item = deviceList.children[i];
                const itemMac = item.querySelector('.track-btn').getAttribute('data-mac');
                if (itemMac === mac) {
                    const details = item.querySelector('.device-details').textContent;
                    deviceDetails = details;
                    break;
                }
            }
            
            // If we didn't find the details in the DOM, check when the scan results come in
            if (!deviceDetails) {
                fetch('/scan')
                    .then(response => response.json())
                    .then(data => {
                        const devices = data.devices || data;
                        for (const device of devices) {
                            if (device.mac === mac) {
                                deviceDetails = `Ch: ${device.channel || 'N/A'} | ${device.encryption || 'Unknown'}`;
                                const detailsElem = document.createElement('p');
                                detailsElem.textContent = deviceDetails;
                                detailsElem.id = 'trackingDeviceDetails';
                                trackingDeviceMAC.insertAdjacentElement('afterend', detailsElem);
                                break;
                            }
                        }
                    });
            } else {
                // Add details to modal
                const detailsElem = document.createElement('p');
                detailsElem.textContent = deviceDetails;
                detailsElem.id = 'trackingDeviceDetails';
                trackingDeviceMAC.insertAdjacentElement('afterend', detailsElem);
            }
            
            trackingSignalValue.textContent = '-- dBm';
            trackingSignalQuality.textContent = 'WAITING';
            trackingSignalQuality.className = 'signal-quality';
            trackingSignalBar.style.width = '0%';
            
            // Reset tracking status
            trackingStartTime = Date.now();
            sampleCount = 0;
            
            // Stop any existing tracking
            if (updateInterval) {
                clearInterval(updateInterval);
            }
            
            // Initialize tracking chart if needed
            if (!trackingChartInitialized) {
                initializeTrackingChart();
            } else {
                // Reset chart data
                trackingChart.data.datasets[0].data = [];
                trackingChart.data.labels = [];
                trackingChart.update();
            }
            
            statusMessage.innerHTML = 'Starting to track device...';
            
            // Set this device as tracked
            fetch('/track', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded',
                },
                body: `mac=${mac}`
            })
            .then(response => {
                if (response.ok) {
                    currentTrackedMAC = mac;
                    statusMessage.innerHTML = 'Tracking device: ' + mac;
                    statusMessage.style.color = 'green';
                    
                    // Start updating the signal chart
                    updateSignalChart();
                    updateInterval = setInterval(updateSignalChart, 500);
                } else {
                    throw new Error('Failed to track device');
                }
            })
            .catch(error => {
                statusMessage.innerHTML = 'Error tracking device';
                statusMessage.style.color = 'red';
                console.error('Tracking error:', error);
                trackingModal.classList.remove('active');
            });
        }
        
        // Stop tracking the current device
        function stopTracking() {
            trackingModal.classList.remove('active');
            
            // Remove the details element if it exists
            const detailsElem = document.getElementById('trackingDeviceDetails');
            if (detailsElem) {
                detailsElem.remove();
            }
            
            if (updateInterval) {
                clearInterval(updateInterval);
                updateInterval = null;
            }
            
            currentTrackedMAC = null;
            
            // Refresh device list to update tracking status
            scanDevices();
        }
        
        // Update the signal chart for the tracked device
        function updateSignalChart() {
            if (!currentTrackedMAC) return;
            
            fetch(`/signal?mac=${currentTrackedMAC}`)
                .then(response => response.json())
                .then(data => {
                    if (data.rssi && data.rssi.length > 0) {
                        // Get the most recent RSSI value
                        const currentRssi = data.rssi[data.rssi.length - 1];
                        
                        // Update sample count
                        sampleCount = data.rssi.length;
                        trackingSamples.textContent = `(${sampleCount} samples)`;
                        
                        // Update duration
                        const elapsedSeconds = Math.floor((Date.now() - trackingStartTime) / 1000);
                        const minutes = Math.floor(elapsedSeconds / 60);
                        const seconds = elapsedSeconds % 60;
                        trackingDuration.textContent = `${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
                        
                        // Update modal display
                        trackingSignalValue.textContent = `${currentRssi} dBm`;
                        const qualityClass = getRssiClass(currentRssi);
                        trackingSignalQuality.textContent = qualityClass.toUpperCase();
                        trackingSignalQuality.className = `signal-quality ${qualityClass}`;
                        trackingSignalBar.style.width = `${Math.max(0, Math.min(100, ((currentRssi + 95) / 70) * 100))}%`;
                        
                        // Update tracking chart
                        if (trackingChartInitialized) {
                            // Create timestamps for x-axis labels
                            const timeLabels = data.rssi.map((_, idx) => {
                                // To keep the display clean, only show every other time label on smaller screens
                                const sampleTime = new Date(trackingStartTime + (idx * 500)); // Now 500ms per sample
                                const timeStr = sampleTime.toLocaleTimeString().split(':').slice(1).join(':');
                                return idx % 2 === 0 ? timeStr : '';
                            });
                            
                            trackingChart.data.labels = timeLabels;
                            trackingChart.data.datasets[0].data = data.rssi;
                            trackingChart.update();
                        }
                        
                        // Update the main chart as well
                        if (!trackingChartInitialized) {
                            initializeTrackingChart();
                            trackingChartInitialized = true;
                        }
                        
                        // Update main chart data
                        trackingChart.data.labels = data.rssi.map((_, i) => i);
                        trackingChart.data.datasets[0].data = data.rssi;
                        trackingChart.update();
                    }
                })
                .catch(error => {
                    console.error('Error updating signal chart:', error);
                });
        }
        
        // Initialize the tracking chart in the modal
        function initializeTrackingChart() {
            const ctx = document.getElementById('trackingChart').getContext('2d');
            
            trackingChart = new SimpleChart(ctx, {
                type: 'bar',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'Signal Strength (dBm)',
                        data: [],
                        borderColor: '#2ecc71',
                        backgroundColor: '#2ecc71',
                        barThickness: 'flex',
                        maxBarThickness: 20
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    scales: {
                        y: {
                            beginAtZero: false,
                            max: -25,
                            min: -95,
                            title: {
                                display: true,
                                text: 'Signal Strength (dBm)'
                            }
                        },
                        x: {
                            title: {
                                display: true,
                                text: 'Time'
                            }
                        }
                    },
                    animation: {
                        duration: 200
                    }
                }
            });
            
            trackingChartInitialized = true;
        }
        
        // Helper function to get signal strength class
        function getRssiClass(rssi) {
            if (rssi >= -25) return 'excellent';
            if (rssi >= -45) return 'good';
            if (rssi >= -65) return 'fair';
            if (rssi >= -85) return 'poor';
            return 'poor';
        }
        
        // Error handling fallback
        window.addEventListener('error', function(event) {
            console.error('JavaScript error:', event.message);
            statusMessage.innerHTML = 'JavaScript error occurred. Try refreshing the page.';
            statusMessage.style.color = 'red';
        });
        
        // Make trackDevice globally accessible
        window.trackDevice = startTracking;
        
        // Notify that script loaded successfully
        console.log('Specter script loaded successfully');
    </script>
</body>
</html>
)rawliteral";
}

String getEncryptionTypeString(wifi_auth_mode_t encryptionType) {
  switch (encryptionType) {
    case WIFI_AUTH_OPEN:
      return "Open";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA-PSK";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2-PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA/WPA2-PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "WPA2-Enterprise";
    case WIFI_AUTH_WPA3_PSK:
      return "WPA3-PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "WPA2/WPA3-PSK";
    default:
      return "Unknown";
  }
}