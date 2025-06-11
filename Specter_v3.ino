#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

const char* ssid = "Specter";
const char* password = "specter123";

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

String deviceTable = "";
bool isScanning = false;
String trackingBSSID = "";
unsigned long lastScanTime = 0;
unsigned long lastTrackTime = 0;
const unsigned long scanInterval = 3000; // 3 seconds
const unsigned long trackInterval = 1000; // 1 second

// Function declaration
String getEncType(wifi_auth_mode_t enc);

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.println("Access Point Started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.begin();
  Serial.println("HTTP server started");

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server started on port 81");
}

void loop() {
  server.handleClient();
  webSocket.loop();
  
  unsigned long currentTime = millis();
  
  // Handle continuous scanning
  if (isScanning && (currentTime - lastScanTime >= scanInterval)) {
    performScan();
    lastScanTime = currentTime;
  }
  
  // Handle device tracking
  if (!trackingBSSID.isEmpty() && (currentTime - lastTrackTime >= trackInterval)) {
    performTracking();
    lastTrackTime = currentTime;
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      // Stop scanning if client disconnects
      isScanning = false;
      trackingBSSID = "";
      break;
      
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] Connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
      
      // Send connection confirmation
      DynamicJsonDocument doc(200);
      doc["type"] = "connected";
      doc["message"] = "WebSocket connected successfully";
      String response;
      serializeJson(doc, response);
      webSocket.sendTXT(num, response);
      break;
    }
    
    case WStype_TEXT: {
      Serial.printf("[%u] Received Text: %s\n", num, payload);
      
      DynamicJsonDocument doc(512);
      deserializeJson(doc, payload);
      
      String command = doc["command"];
      
      if (command == "startScan") {
        isScanning = true;
        lastScanTime = 0; // Trigger immediate scan
        Serial.println("Started continuous scanning");
        
        // Send confirmation
        DynamicJsonDocument response(200);
        response["type"] = "scanStatus";
        response["scanning"] = true;
        String responseStr;
        serializeJson(response, responseStr);
        webSocket.sendTXT(num, responseStr);
        
      } else if (command == "stopScan") {
        isScanning = false;
        Serial.println("Stopped continuous scanning");
        
        // Send confirmation
        DynamicJsonDocument response(200);
        response["type"] = "scanStatus";
        response["scanning"] = false;
        String responseStr;
        serializeJson(response, responseStr);
        webSocket.sendTXT(num, responseStr);
        
      } else if (command == "startTrack") {
        String bssid = doc["bssid"];
        String essid = doc["essid"];
        trackingBSSID = bssid;
        isScanning = false; // Stop main scanning
        lastTrackTime = 0; // Trigger immediate tracking
        Serial.println("Started tracking: " + bssid);
        
        // Send confirmation with network name
        DynamicJsonDocument response(300);
        response["type"] = "trackStatus";
        response["tracking"] = true;
        response["bssid"] = bssid;
        response["essid"] = essid;
        String responseStr;
        serializeJson(response, responseStr);
        webSocket.sendTXT(num, responseStr);
        
      } else if (command == "stopTrack") {
        trackingBSSID = "";
        Serial.println("Stopped tracking");
        
        // Send confirmation
        DynamicJsonDocument response(200);
        response["type"] = "trackStatus";
        response["tracking"] = false;
        String responseStr;
        serializeJson(response, responseStr);
        webSocket.sendTXT(num, responseStr);
      }
      break;
    }
    
    default:
      break;
  }
}

void performScan() {
  int n = WiFi.scanNetworks();
  
  DynamicJsonDocument doc(4096);
  doc["type"] = "scanResults";
  JsonArray networks = doc.createNestedArray("networks");
  
  for (int i = 0; i < n; ++i) {
    JsonObject network = networks.createNestedObject();
    network["essid"] = WiFi.SSID(i);
    network["bssid"] = WiFi.BSSIDstr(i);
    network["channel"] = WiFi.channel(i);
    network["rssi"] = WiFi.RSSI(i);
    network["enc"] = getEncType(WiFi.encryptionType(i));
  }
  
  String response;
  serializeJson(doc, response);
  webSocket.broadcastTXT(response);
  
  WiFi.scanDelete();
  Serial.println("Scan completed, found " + String(n) + " networks");
}

void performTracking() {
  int n = WiFi.scanNetworks();
  
  for (int i = 0; i < n; ++i) {
    if (WiFi.BSSIDstr(i) == trackingBSSID) {
      DynamicJsonDocument doc(300);
      doc["type"] = "trackingUpdate";
      doc["bssid"] = trackingBSSID;
      doc["rssi"] = WiFi.RSSI(i);
      doc["timestamp"] = millis();
      
      String response;
      serializeJson(doc, response);
      webSocket.broadcastTXT(response);
      
      WiFi.scanDelete();
      return;
    }
  }
  
  // Device not found
  DynamicJsonDocument doc(300);
  doc["type"] = "trackingUpdate";
  doc["bssid"] = trackingBSSID;
  doc["rssi"] = "Not Found";
  doc["timestamp"] = millis();
  
  String response;
  serializeJson(doc, response);
  webSocket.broadcastTXT(response);
  
  WiFi.scanDelete();
}

void handleRoot() {
  String page = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Specter Dashboard</title>
  <style>
    /* Reset and base styles */
    * { box-sizing: border-box; margin: 0; padding: 0; }
    
    body { 
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; 
      background: linear-gradient(135deg, #0f172a 0%, #1e293b 100%); 
      color: #f1f5f9; 
      padding: 1rem; 
      min-height: 100vh;
      line-height: 1.6;
    }
    
    .container {
      max-width: 1200px;
      margin: 0 auto;
      padding: 0 1rem;
    }
    
    h1 { 
      font-size: clamp(1.5rem, 4vw, 2.5rem);
      margin-bottom: 2rem; 
      text-align: center;
      background: linear-gradient(45deg, #3b82f6, #8b5cf6);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
      background-clip: text;
      font-weight: 700;
    }
    
    .status-bar {
      background: rgba(30, 41, 59, 0.8);
      border-radius: 0.75rem;
      padding: 1rem;
      margin-bottom: 1rem;
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      gap: 1rem;
    }
    
    .status-indicator {
      display: flex;
      align-items: center;
      gap: 0.5rem;
    }
    
    .status-dot {
      width: 10px;
      height: 10px;
      border-radius: 50%;
      background: #ef4444;
    }
    
    .status-dot.connected {
      background: #10b981;
    }
    
    .status-dot.scanning {
      background: #f59e0b;
      animation: pulse 2s infinite;
    }
    
    @keyframes pulse {
      0%, 100% { opacity: 1; }
      50% { opacity: 0.5; }
    }
    
    .btn { 
      background: linear-gradient(135deg, #3b82f6 0%, #1d4ed8 100%);
      color: white;
      padding: 0.875rem 1.5rem; 
      border: none;
      border-radius: 0.75rem; 
      margin: 0.5rem 0.25rem; 
      cursor: pointer;
      font-size: 1rem;
      font-weight: 600;
      transition: all 0.3s ease;
      box-shadow: 0 4px 15px rgba(59, 130, 246, 0.3);
      min-height: 48px;
    }
    
    .btn:hover {
      transform: translateY(-2px);
      box-shadow: 0 8px 25px rgba(59, 130, 246, 0.4);
      background: linear-gradient(135deg, #2563eb 0%, #1e40af 100%);
    }
    
    .btn:active {
      transform: translateY(0);
    }
    
    .btn:disabled {
      opacity: 0.5;
      transform: none;
      cursor: not-allowed;
    }
    
    .scan-btn {
      width: 100%;
      max-width: 300px;
      margin: 0 auto 2rem auto;
      display: block;
      font-size: 1.1rem;
    }
    
    .btn-stop {
      background: linear-gradient(135deg, #ef4444 0%, #dc2626 100%);
    }
    
    .btn-stop:hover {
      background: linear-gradient(135deg, #dc2626 0%, #b91c1c 100%);
    }
    
    /* Filter Panel Styles */
    .filter-panel {
      background: rgba(30, 41, 59, 0.8);
      border-radius: 1rem;
      padding: 1.5rem;
      margin-bottom: 1rem;
      backdrop-filter: blur(10px);
      border: 1px solid rgba(71, 85, 105, 0.3);
    }
    
    .filter-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 1rem;
    }
    
    .filter-title {
      color: #3b82f6;
      font-weight: 600;
      font-size: 1.1rem;
      display: flex;
      align-items: center;
      gap: 0.5rem;
    }
    
    .filter-toggle {
      background: none;
      border: 1px solid #3b82f6;
      color: #3b82f6;
      padding: 0.5rem 1rem;
      border-radius: 0.5rem;
      cursor: pointer;
      font-size: 0.9rem;
      transition: all 0.2s;
    }
    
    .filter-toggle:hover {
      background: rgba(59, 130, 246, 0.1);
    }
    
    .filter-toggle.active {
      background: #3b82f6;
      color: white;
    }
    
    .filter-content {
      display: none;
      grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
      gap: 1rem;
      margin-top: 1rem;
    }
    
    .filter-content.active {
      display: grid;
    }
    
    .filter-group {
      display: flex;
      flex-direction: column;
      gap: 0.5rem;
    }
    
    .filter-label {
      color: #cbd5e1;
      font-size: 0.9rem;
      font-weight: 500;
    }
    
    .filter-input {
      background: rgba(51, 65, 85, 0.6);
      border: 1px solid rgba(71, 85, 105, 0.5);
      border-radius: 0.5rem;
      padding: 0.75rem;
      color: #f1f5f9;
      font-size: 0.9rem;
      transition: all 0.2s;
    }
    
    .filter-input:focus {
      outline: none;
      border-color: #3b82f6;
      box-shadow: 0 0 0 3px rgba(59, 130, 246, 0.1);
    }
    
    .filter-input::placeholder {
      color: #64748b;
    }
    
    .filter-select {
      background: rgba(51, 65, 85, 0.6);
      border: 1px solid rgba(71, 85, 105, 0.5);
      border-radius: 0.5rem;
      padding: 0.75rem;
      color: #f1f5f9;
      font-size: 0.9rem;
      cursor: pointer;
    }
    
    .filter-select option {
      background: #1e293b;
      color: #f1f5f9;
    }
    
    .filter-actions {
      display: flex;
      gap: 0.5rem;
      margin-top: 1rem;
    }
    
    .filter-btn {
      background: rgba(59, 130, 246, 0.2);
      color: #3b82f6;
      border: 1px solid #3b82f6;
      padding: 0.5rem 1rem;
      border-radius: 0.5rem;
      font-size: 0.8rem;
      cursor: pointer;
      transition: all 0.2s;
    }
    
    .filter-btn:hover {
      background: rgba(59, 130, 246, 0.3);
    }
    
    .clear-btn {
      background: rgba(239, 68, 68, 0.2);
      color: #ef4444;
      border: 1px solid #ef4444;
    }
    
    .clear-btn:hover {
      background: rgba(239, 68, 68, 0.3);
    }
    
    .results-summary {
      background: rgba(51, 65, 85, 0.4);
      padding: 0.75rem;
      border-radius: 0.5rem;
      margin-bottom: 1rem;
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      gap: 1rem;
    }
    
    .results-info {
      color: #94a3b8;
      font-size: 0.9rem;
    }
    
    .sort-controls {
      display: flex;
      gap: 0.5rem;
      align-items: center;
    }
    
    .sort-label {
      color: #94a3b8;
      font-size: 0.8rem;
    }
    
    .sort-select {
      background: rgba(30, 41, 59, 0.8);
      border: 1px solid rgba(71, 85, 105, 0.5);
      border-radius: 0.25rem;
      padding: 0.25rem 0.5rem;
      color: #f1f5f9;
      font-size: 0.8rem;
    }
    
    /* Table responsive design */
    .table-container {
      background: rgba(30, 41, 59, 0.8);
      border-radius: 1rem;
      overflow: hidden;
      margin-top: 1rem;
      box-shadow: 0 10px 30px rgba(0, 0, 0, 0.3);
      backdrop-filter: blur(10px);
    }
    
    table { 
      width: 100%; 
      border-collapse: collapse;
      font-size: 0.9rem;
    }
    
    th { 
      background: linear-gradient(135deg, #1e40af 0%, #3730a3 100%);
      color: white;
      padding: 1rem 0.75rem; 
      text-align: left;
      font-weight: 600;
      text-transform: uppercase;
      letter-spacing: 0.5px;
      font-size: 0.8rem;
      cursor: pointer;
      position: relative;
      user-select: none;
    }
    
    th:hover {
      background: linear-gradient(135deg, #1d4ed8 0%, #3730a3 100%);
    }
    
    th.sortable::after {
      content: '↕️';
      margin-left: 0.5rem;
      opacity: 0.5;
    }
    
    th.sort-asc::after {
      content: '↑';
      opacity: 1;
    }
    
    th.sort-desc::after {
      content: '↓';
      opacity: 1;
    }
    
    td { 
      border-bottom: 1px solid rgba(71, 85, 105, 0.3);
      padding: 0.875rem 0.75rem; 
      background: rgba(51, 65, 85, 0.3);
    }
    
    tr:hover td {
      background: rgba(71, 85, 105, 0.4);
    }
    
    .highlight {
      background: rgba(59, 130, 246, 0.3) !important;
      border: 1px solid rgba(59, 130, 246, 0.5) !important;
    }
    
    .network-name {
      display: flex;
      align-items: center;
      gap: 0.5rem;
    }
    
    .hidden-network {
      opacity: 0.7;
      font-style: italic;
    }
    
    /* Mobile responsive table */
    @media (max-width: 768px) {
      .table-container {
        overflow-x: auto;
        -webkit-overflow-scrolling: touch;
      }
      
      table {
        min-width: 600px;
      }
      
      th, td {
        padding: 0.75rem 0.5rem;
        font-size: 0.85rem;
      }
      
      .btn {
        padding: 0.625rem 1rem;
        font-size: 0.875rem;
        min-height: 44px;
      }
      
      .filter-content {
        grid-template-columns: 1fr;
      }
      
      .results-summary {
        flex-direction: column;
        align-items: stretch;
      }
    }
    
    @media (max-width: 480px) {
      body { padding: 0.5rem; }
      
      .container { padding: 0 0.5rem; }
      
      h1 { margin-bottom: 1.5rem; }
      
      .filter-panel {
        padding: 1rem;
      }
      
      th, td {
        padding: 0.625rem 0.375rem;
        font-size: 0.8rem;
      }
      
      .btn {
        padding: 0.5rem 0.75rem;
        font-size: 0.8rem;
        margin: 0.25rem 0.125rem;
      }
      
      table {
        min-width: 400px;
      }
      
      td .btn {
        display: block;
        width: 100%;
        margin: 0.25rem 0;
        font-size: 0.75rem;
        padding: 0.4rem 0.5rem;
      }
      
      .status-bar {
        flex-direction: column;
        align-items: flex-start;
      }
    }
    
    /* Modal improvements */
    .modal { 
      display: none; 
      position: fixed; 
      inset: 0; 
      background: rgba(0, 0, 0, 0.8);
      backdrop-filter: blur(5px);
      align-items: center; 
      justify-content: center;
      z-index: 1000;
      padding: 1rem;
    }
    
    .modal-content { 
      background: linear-gradient(135deg, #1e293b 0%, #334155 100%);
      padding: 2rem; 
      border-radius: 1rem; 
      width: 100%; 
      max-width: 600px;
      box-shadow: 0 20px 50px rgba(0, 0, 0, 0.5);
      border: 1px solid rgba(71, 85, 105, 0.3);
    }
    
    .modal h2 {
      color: #3b82f6;
      margin-bottom: 1rem;
      font-size: 1.5rem;
    }
    
    .modal p {
      margin-bottom: 1rem;
      color: #cbd5e1;
    }
    
    #signal {
      color: #10b981;
      font-weight: 700;
      font-size: 1.1rem;
    }
    
    /* Signal strength chart */
    .chart-container {
      background: rgba(15, 23, 42, 0.8);
      border-radius: 0.75rem;
      padding: 1rem;
      margin: 1rem 0;
      border: 1px solid rgba(71, 85, 105, 0.3);
    }
    
    .chart-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 1rem;
      flex-wrap: wrap;
      gap: 1rem;
    }
    
    .chart-title {
      color: #3b82f6;
      font-weight: 600;
      font-size: 1.1rem;
    }
    
    .chart-controls {
      display: flex;
      gap: 0.5rem;
      flex-wrap: wrap;
    }
    
    .time-btn {
      background: rgba(59, 130, 246, 0.2);
      color: #3b82f6;
      border: 1px solid #3b82f6;
      padding: 0.25rem 0.75rem;
      border-radius: 0.5rem;
      font-size: 0.8rem;
      cursor: pointer;
      transition: all 0.2s;
    }
    
    .time-btn.active {
      background: #3b82f6;
      color: white;
    }
    
    .time-btn:hover {
      background: rgba(59, 130, 246, 0.3);
    }
    
    #signalChart {
      border-radius: 0.5rem;
      background: rgba(0, 0, 0, 0.3);
      width: 100%;
      height: 200px;
    }
    
    .chart-stats {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(120px, 1fr));
      gap: 1rem;
      margin-top: 1rem;
    }
    
    .stat-item {
      text-align: center;
      background: rgba(51, 65, 85, 0.4);
      padding: 0.75rem;
      border-radius: 0.5rem;
    }
    
    .stat-label {
      color: #94a3b8;
      font-size: 0.8rem;
      margin-bottom: 0.25rem;
    }
    
    .stat-value {
      font-weight: 700;
      font-size: 1.1rem;
    }
    
    /* Status indicators */
    .security-indicator {
      display: inline-block;
      width: 8px;
      height: 8px;
      border-radius: 50%;
      margin-right: 0.5rem;
    }
    
    .status-open { background: #10b981; }
    .status-secure { background: #f59e0b; }
    .status-unknown { background: #6b7280; }
    
    /* Network count */
    .network-count {
      font-size: 0.9rem;
      color: #94a3b8;
      margin-left: auto;
    }
    
    /* Real-time update indicator */
    .live-indicator {
      display: inline-block;
      width: 6px;
      height: 6px;
      background: #10b981;
      border-radius: 50%;
      margin-right: 0.5rem;
      animation: blink 1.5s infinite;
    }
    
    @keyframes blink {
      0%, 50% { opacity: 1; }
      51%, 100% { opacity: 0.3; }
    }
    
    /* Mobile modal adjustments */
    @media (max-width: 480px) {
      .modal-content {
        max-width: 95vw;
        padding: 1.5rem;
      }
      
      .chart-header {
        flex-direction: column;
        align-items: stretch;
      }
      
      .chart-stats {
        grid-template-columns: repeat(2, 1fr);
      }
      
      #signalChart {
        height: 160px;
      }
    }

    @keyframes slideIn {
      from {
        transform: translateX(100%);
        opacity: 0;
      }
      to {
        transform: translateX(0);
        opacity: 1;
      }
    }

    @keyframes slideOut {
      from {
        transform: translateX(0);
        opacity: 1;
      }
      to {
        transform: translateX(100%);
        opacity: 0;
      }
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Specter Wireless Dashboard</h1>
    
    <div class="status-bar">
      <div class="status-indicator">
        <div class="status-dot" id="wsStatus"></div>
        <span id="wsStatusText">Connecting...</span>
      </div>
      <div class="status-indicator">
        <div class="status-dot" id="scanStatus"></div>
        <span id="scanStatusText">Ready</span>
      </div>
      <div class="network-count" id="networkCount">0 networks</div>
    </div>
    
    <button class="btn scan-btn" onclick="toggleScan()" id="scanBtn" disabled>
      <span id="scanText">Connect WebSocket</span>
    </button>
    
    <!-- Filter Panel -->
    <div class="filter-panel">
      <div class="filter-header">
        <div class="filter-title">
          🔍 Smart Filters
        </div>
        <button class="filter-toggle" onclick="toggleFilters()" id="filterToggle">
          Show Filters
        </button>
      </div>
      
      <div class="filter-content" id="filterContent">
        <div class="filter-group">
          <label class="filter-label">Search Networks</label>
          <input type="text" class="filter-input" placeholder="Network name or BSSID..." 
                 id="searchInput" oninput="applyFilters()">
        </div>
        
        <div class="filter-group">
          <label class="filter-label">Security Type</label>
          <select class="filter-select" id="securityFilter" onchange="applyFilters()">
            <option value="">All Security Types</option>
            <option value="Open">Open</option>
            <option value="WEP">WEP</option>
            <option value="WPA">WPA</option>
            <option value="WPA2">WPA2</option>
            <option value="WPA/WPA2">WPA/WPA2</option>
          </select>
        </div>
        
        <div class="filter-group">
          <label class="filter-label">Signal Strength</label>
          <select class="filter-select" id="signalFilter" onchange="applyFilters()">
            <option value="">All Signal Levels</option>
            <option value="excellent">Excellent (> -50 dBm)</option>
            <option value="good">Good (-50 to -60 dBm)</option>
            <option value="fair">Fair (-60 to -70 dBm)</option>
            <option value="poor">Poor (-70 to -80 dBm)</option>
            <option value="very-poor">Very Poor (< -80 dBm)</option>
          </select>
        </div>
        
        <div class="filter-group">
          <label class="filter-label">Channel</label>
          <select class="filter-select" id="channelFilter" onchange="applyFilters()">
            <option value="">All Channels</option>
            <option value="1">Channel 1</option>
            <option value="6">Channel 6</option>
            <option value="11">Channel 11</option>
            <option value="36">Channel 36</option>
            <option value="40">Channel 40</option>
            <option value="44">Channel 44</option>
            <option value="48">Channel 48</option>
            <option value="other">Other Channels</option>
          </select>
        </div>
        
        <div class="filter-actions">
          <button class="filter-btn" onclick="toggleHiddenNetworks()" id="hiddenToggle">
            Show Hidden
          </button>
          <button class="filter-btn clear-btn" onclick="clearFilters()">
            Clear All
          </button>
        </div>
      </div>
    </div>
    
    <!-- Export Panel -->
    <div class="filter-panel" style="margin-top: 1rem;">
      <div class="filter-header">
        <div class="filter-title">
          📥 Export Data
        </div>
        <button class="filter-toggle" onclick="toggleExports()" id="exportToggle">
          Show Export
        </button>
      </div>
      
      <div class="filter-content" id="exportContent">
        <div class="filter-group">
          <label class="filter-label">Export Networks</label>
          <div style="display: flex; gap: 0.5rem; flex-wrap: wrap;">
            <button class="filter-btn" onclick="exportNetworks('csv')">
              📊 CSV Format
            </button>
            <button class="filter-btn" onclick="exportNetworks('json')">
              📄 JSON Format
            </button>
            <button class="filter-btn" onclick="exportNetworks('txt')">
              📝 Text Report
            </button>
          </div>
        </div>
        
        <div class="filter-group">
          <label class="filter-label">Export Options</label>
          <div style="display: flex; gap: 1rem; flex-wrap: wrap; align-items: center;">
            <label style="display: flex; align-items: center; gap: 0.5rem; color: #cbd5e1; font-size: 0.9rem;">
              <input type="radio" name="exportScope" value="all" checked style="accent-color: #3b82f6;">
              All Networks
            </label>
            <label style="display: flex; align-items: center; gap: 0.5rem; color: #cbd5e1; font-size: 0.9rem;">
              <input type="radio" name="exportScope" value="filtered" style="accent-color: #3b82f6;">
              Filtered Results
            </label>
          </div>
        </div>
        
        <div class="filter-group" id="signalExportGroup" style="display: none;">
          <label class="filter-label">Export Signal History</label>
          <div style="display: flex; gap: 0.5rem; flex-wrap: wrap;">
            <button class="filter-btn" onclick="exportSignalHistory('csv')" id="exportSignalBtn">
              📈 Signal CSV
            </button>
            <button class="filter-btn" onclick="exportSignalHistory('json')" id="exportSignalJsonBtn">
              📊 Signal JSON
            </button>
          </div>
        </div>
      </div>
    </div>
    
    <!-- Results Summary -->
    <div class="results-summary" id="resultsSummary" style="display: none;">
      <div class="results-info" id="resultsInfo">
        Showing 0 of 0 networks
      </div>
      <div class="sort-controls">
        <span class="sort-label">Sort by:</span>
        <select class="sort-select" id="sortSelect" onchange="applySorting()">
          <option value="signal-desc">Signal Strength (High to Low)</option>
          <option value="signal-asc">Signal Strength (Low to High)</option>
          <option value="name-asc">Network Name (A-Z)</option>
          <option value="name-desc">Network Name (Z-A)</option>
          <option value="channel-asc">Channel (Low to High)</option>
          <option value="channel-desc">Channel (High to Low)</option>
          <option value="security-asc">Security Type</option>
        </select>
      </div>
    </div>
    
    <div class="table-container">
      <table id="results">
        <thead>
          <tr>
            <th class="sortable" onclick="sortTable('name')">Network</th>
            <th class="sortable" onclick="sortTable('signal')">Signal</th>
            <th>Actions</th>
          </tr>
        </thead>
        <tbody id="tableBody">
          <tr>
            <td colspan="3" style="text-align: center; padding: 2rem; color: #64748b;">
              Connecting to WebSocket server...
            </td>
          </tr>
        </tbody>
      </table>
    </div>
  </div>

  <div class="modal" id="trackerModal">
    <div class="modal-content">
      <h2>📡 Signal Tracking</h2>
      <p id="trackInfo"></p>
      
      <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 1rem;">
        <p>Current Signal: <span id="signal">-</span> dBm</p>
        <p style="font-size: 0.9rem; color: #94a3b8;">
          <span class="live-indicator"></span>Live updates
        </p>
      </div>
      
      <div class="chart-container">
        <div class="chart-header">
          <span class="chart-title">📊 Signal Strength History</span>
          <div class="chart-controls">
            <button class="time-btn active" onclick="setTimeRange(30)">30s</button>
            <button class="time-btn" onclick="setTimeRange(60)">1m</button>
            <button class="time-btn" onclick="setTimeRange(300)">5m</button>
            <button class="time-btn" onclick="clearChart()">Clear</button>
          </div>
        </div>
        <canvas id="signalChart" width="500" height="200"></canvas>
        <div class="chart-stats">
          <div class="stat-item">
            <div class="stat-label">Current</div>
            <div class="stat-value" id="currentSignal">- dBm</div>
          </div>
          <div class="stat-item">
            <div class="stat-label">Average</div>
            <div class="stat-value" id="avgSignal">- dBm</div>
          </div>
          <div class="stat-item">
            <div class="stat-label">Min</div>
            <div class="stat-value" id="minSignal">- dBm</div>
          </div>
          <div class="stat-item">
            <div class="stat-value" id="maxSignal">- dBm</div>
          </div>
        </div>
      </div>
      
      <p style="font-size: 0.9rem; color: #94a3b8; margin-bottom: 1rem;">
        Last updated: <span id="lastUpdate">-</span>
      </p>
      <button class="btn" onclick="stopTracking()" style="width: 100%;">Stop Tracking</button>
    </div>
  </div>

  <div class="modal" id="detailsModal">
    <div class="modal-content">
      <h2>📋 Network Details</h2>
      <div id="detailsContent"></div>
      <div style="display: flex; gap: 0.5rem; margin-top: 1.5rem;">
        <button class="btn" onclick="closeDetailsModal()" style="flex: 1;">Close</button>
        <button class="btn" onclick="trackFromDetails()" id="trackFromDetailsBtn" style="flex: 1; background: linear-gradient(135deg, #10b981 0%, #059669 100%);">Track Device</button>
      </div>
    </div>
  </div>

  <script>
    let ws = null;
    let currentNetworks = [];
    let filteredNetworks = [];
    let selectedNetwork = null;
    let isScanning = false;
    let isTracking = false;
    let isConnected = false;
    let showHidden = true;
    let currentSort = { field: 'signal', direction: 'desc' };
    
    // Signal tracking variables
    let signalHistory = [];
    let chart = null;
    let chartCtx = null;
    let timeRange = 30;
    let maxDataPoints = 150;
    
    // Filter and search functions
    function toggleFilters() {
      const content = document.getElementById('filterContent');
      const toggle = document.getElementById('filterToggle');
      
      if (content.classList.contains('active')) {
        content.classList.remove('active');
        toggle.textContent = 'Show Filters';
        toggle.classList.remove('active');
      } else {
        content.classList.add('active');
        toggle.textContent = 'Hide Filters';
        toggle.classList.add('active');
      }
    }
    
    function applyFilters() {
      const searchTerm = document.getElementById('searchInput').value.toLowerCase();
      const securityFilter = document.getElementById('securityFilter').value;
      const signalFilter = document.getElementById('signalFilter').value;
      const channelFilter = document.getElementById('channelFilter').value;
      
      filteredNetworks = currentNetworks.filter(network => {
        // Search filter (ESSID or BSSID)
        const matchesSearch = !searchTerm || 
          (network.essid && network.essid.toLowerCase().includes(searchTerm)) ||
          (network.bssid && network.bssid.toLowerCase().includes(searchTerm));
        
        // Security filter
        const matchesSecurity = !securityFilter || network.enc === securityFilter;
        
        // Signal strength filter
        let matchesSignal = true;
        if (signalFilter) {
          const rssi = network.rssi;
          switch (signalFilter) {
            case 'excellent': matchesSignal = rssi > -50; break;
            case 'good': matchesSignal = rssi <= -50 && rssi > -60; break;
            case 'fair': matchesSignal = rssi <= -60 && rssi > -70; break;
            case 'poor': matchesSignal = rssi <= -70 && rssi > -80; break;
            case 'very-poor': matchesSignal = rssi <= -80; break;
          }
        }
        
        // Channel filter
        let matchesChannel = true;
        if (channelFilter) {
          if (channelFilter === 'other') {
            matchesChannel = ![1, 6, 11, 36, 40, 44, 48].includes(network.channel);
          } else {
            matchesChannel = network.channel == channelFilter;
          }
        }
        
        // Hidden networks filter
        const isHidden = !network.essid || network.essid.trim() === '';
        const matchesHidden = showHidden || !isHidden;
        
        return matchesSearch && matchesSecurity && matchesSignal && matchesChannel && matchesHidden;
      });
      
      applySorting();
      updateNetworkTable(filteredNetworks);
      updateResultsSummary();
    }
    
    function applySorting() {
      const sortValue = document.getElementById('sortSelect').value;
      const [field, direction] = sortValue.split('-');
      
      currentSort = { field, direction };
      
      filteredNetworks.sort((a, b) => {
        let aVal, bVal;
        
        switch (field) {
          case 'signal':
            aVal = a.rssi;
            bVal = b.rssi;
            break;
          case 'name':
            aVal = (a.essid || '<Hidden>').toLowerCase();
            bVal = (b.essid || '<Hidden>').toLowerCase();
            break;
          case 'channel':
            aVal = a.channel;
            bVal = b.channel;
            break;
          case 'security':
            aVal = a.enc;
            bVal = b.enc;
            break;
          default:
            return 0;
        }
        
        if (direction === 'asc') {
          return aVal > bVal ? 1 : aVal < bVal ? -1 : 0;
        } else {
          return aVal < bVal ? 1 : aVal > bVal ? -1 : 0;
        }
      });
      
      updateNetworkTable(filteredNetworks);
    }
    
    function sortTable(field) {
      // Toggle direction if same field, otherwise default to desc
      if (currentSort.field === field) {
        currentSort.direction = currentSort.direction === 'asc' ? 'desc' : 'asc';
      } else {
        currentSort.field = field;
        currentSort.direction = 'desc';
      }
      
      // Update sort select to match
      document.getElementById('sortSelect').value = `${field}-${currentSort.direction}`;
      
      applySorting();
      updateSortHeaders();
    }
    
    function updateSortHeaders() {
      // Reset all headers
      document.querySelectorAll('th.sortable').forEach(th => {
        th.classList.remove('sort-asc', 'sort-desc');
      });
      
      // Set current sort
      const headers = document.querySelectorAll('th.sortable');
      headers.forEach(th => {
        const field = th.onclick.toString().match(/sortTable\('(.+?)'\)/)?.[1];
        if (field === currentSort.field) {
          th.classList.add(currentSort.direction === 'asc' ? 'sort-asc' : 'sort-desc');
        }
      });
    }
    
    function toggleHiddenNetworks() {
      showHidden = !showHidden;
      const toggle = document.getElementById('hiddenToggle');
      toggle.textContent = showHidden ? 'Hide Hidden' : 'Show Hidden';
      toggle.style.background = showHidden ? 'rgba(239, 68, 68, 0.2)' : 'rgba(59, 130, 246, 0.2)';
      toggle.style.color = showHidden ? '#ef4444' : '#3b82f6';
      toggle.style.borderColor = showHidden ? '#ef4444' : '#3b82f6';
      applyFilters();
    }
    
    function clearFilters() {
      document.getElementById('searchInput').value = '';
      document.getElementById('securityFilter').value = '';
      document.getElementById('signalFilter').value = '';
      document.getElementById('channelFilter').value = '';
      showHidden = true;
      document.getElementById('hiddenToggle').textContent = 'Hide Hidden';
      document.getElementById('hiddenToggle').style.background = 'rgba(239, 68, 68, 0.2)';
      document.getElementById('hiddenToggle').style.color = '#ef4444';
      document.getElementById('hiddenToggle').style.borderColor = '#ef4444';
      applyFilters();
    }
    
    function updateResultsSummary() {
      const summary = document.getElementById('resultsSummary');
      const info = document.getElementById('resultsInfo');
      
      if (currentNetworks.length > 0) {
        summary.style.display = 'flex';
        const total = currentNetworks.length;
        const filtered = filteredNetworks.length;
        
        if (filtered === total) {
          info.textContent = `Showing all ${total} network${total !== 1 ? 's' : ''}`;
        } else {
          info.textContent = `Showing ${filtered} of ${total} network${total !== 1 ? 's' : ''}`;
        }
      } else {
        summary.style.display = 'none';
      }
    }
    
    function highlightSearchTerm(text, searchTerm) {
      if (!searchTerm || !text) return text;
      
      const regex = new RegExp(`(${searchTerm})`, 'gi');
      return text.replace(regex, '<mark style="background: rgba(59, 130, 246, 0.3); color: inherit;">$1</mark>');
    }
    
    // Chart functions
    function drawChart() {
      if (!chartCtx) return;
      
      const canvas = chartCtx.canvas;
      const width = canvas.width / window.devicePixelRatio;
      const height = canvas.height / window.devicePixelRatio;
      
      chartCtx.clearRect(0, 0, width, height);
      
      const padding = 40;
      const chartWidth = width - padding * 2;
      const chartHeight = height - padding * 2;
      
      const now = Date.now();
      const timeThreshold = now - (timeRange * 1000);
      const filteredData = signalHistory.filter(point => point.timestamp >= timeThreshold);
      
      if (filteredData.length === 0) {
        drawChartFrame(padding, chartWidth, chartHeight);
        drawEmptyMessage(width, height);
        return;
      }
      
      const signalValues = filteredData.map(p => p.signal).filter(s => s !== 'Not Found');
      const minSignal = Math.min(...signalValues, -100);
      const maxSignal = Math.max(...signalValues, -20);
      const signalRange = maxSignal - minSignal || 20;
      
      drawChartFrame(padding, chartWidth, chartHeight);
      drawGrid(padding, chartWidth, chartHeight, minSignal, maxSignal);
      drawAxes(padding, chartWidth, chartHeight, minSignal, maxSignal, timeThreshold);
      drawSignalLine(filteredData, padding, chartWidth, chartHeight, minSignal, signalRange, timeThreshold);
      updateStatistics(signalValues);
    }
    
    function drawChartFrame(padding, chartWidth, chartHeight) {
      chartCtx.strokeStyle = '#475569';
      chartCtx.lineWidth = 1;
      chartCtx.strokeRect(padding, padding, chartWidth, chartHeight);
    }
    
    function drawGrid(padding, chartWidth, chartHeight, minSignal, maxSignal) {
      chartCtx.strokeStyle = '#334155';
      chartCtx.lineWidth = 0.5;
      
      const signalStep = 10;
      for (let signal = Math.ceil(minSignal / signalStep) * signalStep; signal <= maxSignal; signal += signalStep) {
        const y = padding + chartHeight - ((signal - minSignal) / (maxSignal - minSignal)) * chartHeight;
        chartCtx.beginPath();
        chartCtx.moveTo(padding, y);
        chartCtx.lineTo(padding + chartWidth, y);
        chartCtx.stroke();
      }
      
      const timeStep = timeRange / 6;
      for (let i = 1; i < 6; i++) {
        const x = padding + (i * chartWidth / 6);
        chartCtx.beginPath();
        chartCtx.moveTo(x, padding);
        chartCtx.lineTo(x, padding + chartHeight);
        chartCtx.stroke();
      }
    }
    
    function drawAxes(padding, chartWidth, chartHeight, minSignal, maxSignal, timeThreshold) {
      chartCtx.fillStyle = '#94a3b8';
      chartCtx.font = '10px sans-serif';
      chartCtx.textAlign = 'center';
      
      chartCtx.textAlign = 'right';
      const signalStep = 10;
      for (let signal = Math.ceil(minSignal / signalStep) * signalStep; signal <= maxSignal; signal += signalStep) {
        const y = padding + chartHeight - ((signal - minSignal) / (maxSignal - minSignal)) * chartHeight;
        chartCtx.fillText(signal + 'dBm', padding - 5, y + 3);
      }
      
      chartCtx.textAlign = 'center';
      for (let i = 0; i <= 6; i++) {
        const timeAgo = (timeRange * i / 6);
        const x = padding + chartWidth - (i * chartWidth / 6);
        const label = timeAgo === 0 ? 'Now' : `-${timeAgo}s`;
        chartCtx.fillText(label, x, padding + chartHeight + 15);
      }
    }
    
    function drawSignalLine(data, padding, chartWidth, chartHeight, minSignal, signalRange, timeThreshold) {
      if (data.length < 2) return;
      
      chartCtx.lineWidth = 2;
      chartCtx.strokeStyle = '#3b82f6';
      chartCtx.beginPath();
      
      let firstPoint = true;
      data.forEach((point, index) => {
        if (point.signal === 'Not Found') return;
        
        const x = padding + chartWidth - ((Date.now() - point.timestamp) / (timeRange * 1000)) * chartWidth;
        const y = padding + chartHeight - ((point.signal - minSignal) / signalRange) * chartHeight;
        
        if (firstPoint) {
          chartCtx.moveTo(x, y);
          firstPoint = false;
        } else {
          chartCtx.lineTo(x, y);
        }
      });
      
      chartCtx.stroke();
      drawSignalZones(padding, chartWidth, chartHeight, minSignal, signalRange);
      drawDataPoints(data, padding, chartWidth, chartHeight, minSignal, signalRange);
    }
    
    function drawSignalZones(padding, chartWidth, chartHeight, minSignal, signalRange) {
      const zones = [
        { min: -50, color: 'rgba(16, 185, 129, 0.1)', label: 'Excellent' },
        { min: -60, color: 'rgba(34, 197, 94, 0.1)', label: 'Good' },
        { min: -70, color: 'rgba(245, 158, 11, 0.1)', label: 'Fair' },
        { min: -80, color: 'rgba(239, 68, 68, 0.1)', label: 'Poor' }
      ];
      
      zones.forEach(zone => {
        if (zone.min >= minSignal && zone.min <= minSignal + signalRange) {
          const y = padding + chartHeight - ((zone.min - minSignal) / signalRange) * chartHeight;
          chartCtx.fillStyle = zone.color;
          chartCtx.fillRect(padding, padding, chartWidth, y - padding);
        }
      });
    }
    
    function drawDataPoints(data, padding, chartWidth, chartHeight, minSignal, signalRange) {
      data.forEach(point => {
        if (point.signal === 'Not Found') return;
        
        const x = padding + chartWidth - ((Date.now() - point.timestamp) / (timeRange * 1000)) * chartWidth;
        const y = padding + chartHeight - ((point.signal - minSignal) / signalRange) * chartHeight;
        
        const signal = point.signal;
        let color = '#ef4444';
        if (signal > -50) color = '#10b981';
        else if (signal > -60) color = '#22c55e';
        else if (signal > -70) color = '#f59e0b';
        
        chartCtx.fillStyle = color;
        chartCtx.beginPath();
        chartCtx.arc(x, y, 3, 0, 2 * Math.PI);
        chartCtx.fill();
        
        if (point === data[data.length - 1]) {
          chartCtx.strokeStyle = color;
          chartCtx.lineWidth = 2;
          chartCtx.beginPath();
          chartCtx.arc(x, y, 5, 0, 2 * Math.PI);
          chartCtx.stroke();
        }
      });
    }
    
    function drawEmptyMessage(width, height) {
      chartCtx.fillStyle = '#64748b';
      chartCtx.font = '14px sans-serif';
      chartCtx.textAlign = 'center';
      chartCtx.fillText('No data available', width / 2, height / 2);
      chartCtx.fillText('Start tracking to see signal history', width / 2, height / 2 + 20);
    }
    
    function updateStatistics(signalValues) {
      if (signalValues.length === 0) {
        document.getElementById('currentSignal').textContent = '- dBm';
        document.getElementById('avgSignal').textContent = '- dBm';
        document.getElementById('minSignal').textContent = '- dBm';
        document.getElementById('maxSignal').textContent = '- dBm';
        return;
      }
      
      const current = signalHistory[signalHistory.length - 1]?.signal || '-';
      const avg = Math.round(signalValues.reduce((a, b) => a + b, 0) / signalValues.length);
      const min = Math.min(...signalValues);
      const max = Math.max(...signalValues);
      
      document.getElementById('currentSignal').textContent = current + ' dBm';
      document.getElementById('avgSignal').textContent = avg + ' dBm';
      document.getElementById('minSignal').textContent = min + ' dBm';
      document.getElementById('maxSignal').textContent = max + ' dBm';
      
      const currentEl = document.getElementById('currentSignal');
      if (current > -50) currentEl.style.color = '#10b981';
      else if (current > -60) currentEl.style.color = '#22c55e';
      else if (current > -70) currentEl.style.color = '#f59e0b';
      else currentEl.style.color = '#ef4444';
    }
    
    function addSignalDataPoint(signal) {
      const timestamp = Date.now();
      signalHistory.push({ signal, timestamp });
      
      if (signalHistory.length > maxDataPoints) {
        signalHistory = signalHistory.slice(-maxDataPoints);
      }
      
      drawChart();
    }
    
    function setTimeRange(seconds) {
      timeRange = seconds;
      document.querySelectorAll('.time-btn').forEach(btn => btn.classList.remove('active'));
      event.target.classList.add('active');
      drawChart();
    }
    
    function clearChart() {
      signalHistory = [];
      drawChart();
      
      document.getElementById('currentSignal').textContent = '- dBm';
      document.getElementById('avgSignal').textContent = '- dBm';
      document.getElementById('minSignal').textContent = '- dBm';
      document.getElementById('maxSignal').textContent = '- dBm';
    }
    
    // WebSocket functions
    function initWebSocket() {
      const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
      const wsUrl = `${protocol}//${window.location.hostname}:81`;
      
      console.log('Connecting to WebSocket:', wsUrl);
      ws = new WebSocket(wsUrl);
      
      ws.onopen = function(event) {
        console.log('WebSocket connected');
        isConnected = true;
        updateConnectionStatus(true);
        document.getElementById('scanBtn').disabled = false;
        document.getElementById('scanText').textContent = 'Start Scanning';
        document.getElementById('tableBody').innerHTML = `
          <tr><td colspan="3" style="text-align: center; padding: 2rem; color: #64748b;">
            Ready to scan for networks
          </td></tr>
        `;
      };
      
      ws.onmessage = function(event) {
        const data = JSON.parse(event.data);
        handleWebSocketMessage(data);
      };
      
      ws.onclose = function(event) {
        console.log('WebSocket disconnected');
        isConnected = false;
        isScanning = false;
        updateConnectionStatus(false);
        updateScanStatus(false);
        document.getElementById('scanBtn').disabled = true;
        document.getElementById('scanText').textContent = 'Reconnecting...';
        
        setTimeout(initWebSocket, 3000);
      };
      
      ws.onerror = function(error) {
        console.error('WebSocket error:', error);
      };
    }
    
    function handleWebSocketMessage(data) {
      switch(data.type) {
        case 'connected':
          console.log('WebSocket connection confirmed');
          break;
          
        case 'scanStatus':
          isScanning = data.scanning;
          updateScanStatus(data.scanning);
          break;
          
        case 'scanResults':
          currentNetworks = data.networks;
          applyFilters(); // Apply current filters to new data
          break;
          
        case 'trackStatus':
          isTracking = data.tracking;
          updateTrackingUI(data.tracking);
          if (data.tracking) {
            document.getElementById('trackInfo').innerHTML = 
              `<strong>Network:</strong> ${data.essid || '<Hidden>'}<br><strong>BSSID:</strong> ${data.bssid}`;
            document.getElementById('trackerModal').style.display = 'flex';
            setTimeout(initChart, 100);
          } else {
            document.getElementById('trackerModal').style.display = 'none';
            clearChart();
          }
          break;
          
        case 'trackingUpdate':
          const signal = data.rssi === 'Not Found' ? 'Not Found' : parseInt(data.rssi);
          document.getElementById('signal').textContent = data.rssi;
          document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();
          addSignalDataPoint(signal);
          break;
      }
    }
    
    function updateConnectionStatus(connected) {
      const statusDot = document.getElementById('wsStatus');
      const statusText = document.getElementById('wsStatusText');
      
      if (connected) {
        statusDot.classList.add('connected');
        statusText.textContent = 'Connected';
      } else {
        statusDot.classList.remove('connected');
        statusText.textContent = 'Disconnected';
      }
    }
    
    function updateScanStatus(scanning) {
      const statusDot = document.getElementById('scanStatus');
      const statusText = document.getElementById('scanStatusText');
      const scanBtn = document.getElementById('scanBtn');
      const scanText = document.getElementById('scanText');
      
      if (scanning) {
        statusDot.classList.add('scanning');
        statusText.textContent = 'Scanning...';
        scanBtn.classList.add('btn-stop');
        scanText.textContent = 'Stop Scanning';
      } else {
        statusDot.classList.remove('scanning');
        statusText.textContent = 'Ready';
        scanBtn.classList.remove('btn-stop');
        scanText.textContent = 'Start Scanning';
      }
    }
    
    function updateNetworkTable(networks) {
      const table = document.getElementById("tableBody");
      const networkCount = document.getElementById("networkCount");
      const searchTerm = document.getElementById('searchInput').value;
      
      networkCount.textContent = `${networks.length} network${networks.length !== 1 ? 's' : ''}`;
      
      if (networks.length === 0) {
        const message = currentNetworks.length === 0 ? 
          'No networks found - scanning continues...' : 
          'No networks match current filters';
        table.innerHTML = `<tr><td colspan="3" style="text-align: center; padding: 2rem; color: #64748b;">
          <span class="live-indicator"></span>${message}
        </td></tr>`;
      } else {
        table.innerHTML = "";
        networks.forEach((d, index) => {
          const encClass = d.enc === 'Open' ? 'status-open' : d.enc === 'Unknown' ? 'status-unknown' : 'status-secure';
          const signalColor = d.rssi > -50 ? '#10b981' : d.rssi > -70 ? '#f59e0b' : '#ef4444';
          const signalIcon = d.rssi > -50 ? '📶' : d.rssi > -70 ? '📶' : '📶';
          const isHidden = !d.essid || d.essid.trim() === '';
          const displayName = d.essid || '<Hidden>';
          const highlightedName = highlightSearchTerm(displayName, searchTerm);
          const highlightedBSSID = highlightSearchTerm(d.bssid, searchTerm);
          
          // Find original index for actions
          const originalIndex = currentNetworks.findIndex(n => n.bssid === d.bssid);
          
          table.innerHTML += `<tr${searchTerm && (displayName.toLowerCase().includes(searchTerm.toLowerCase()) || d.bssid.toLowerCase().includes(searchTerm.toLowerCase())) ? ' class="highlight"' : ''}>
            <td>
              <div class="network-name">
                <span class="security-indicator ${encClass}"></span>
                <div>
                  <div style="font-weight: 600; ${isHidden ? 'opacity: 0.7; font-style: italic;' : ''}">${highlightedName}</div>
                  <div style="font-size: 0.8rem; color: #94a3b8; font-family: monospace;">${highlightedBSSID}</div>
                </div>
              </div>
            </td>
            <td style="font-weight: 600; color: ${signalColor};">
              ${signalIcon} ${d.rssi} dBm
              <div style="font-size: 0.8rem; color: #94a3b8;">Ch ${d.channel}</div>
            </td>
            <td>
              <button class="btn" onclick="showDetails(${originalIndex})" style="margin-right: 0.25rem; font-size: 0.8rem;">Details</button>
              <button class="btn" onclick="startTracking('${d.bssid}', '${d.essid}')" style="font-size: 0.8rem;">Track</button>
            </td>
          </tr>`;
        });
      }
      
      updateSortHeaders();
    }
    
    // Keep all other existing functions (toggleScan, startTracking, etc.)
    function toggleScan() {
      if (!isConnected) {
        console.log('WebSocket not connected');
        return;
      }
      
      if (isScanning) {
        console.log('Stopping scan');
        ws.send(JSON.stringify({command: 'stopScan'}));
      } else {
        console.log('Starting scan');
        ws.send(JSON.stringify({command: 'startScan'}));
      }
    }
    
    function startTracking(bssid, essid) {
      if (!isConnected) {
        console.log('WebSocket not connected');
        return;
      }
      
      console.log('Starting tracking for:', bssid);
      ws.send(JSON.stringify({
        command: 'startTrack',
        bssid: bssid,
        essid: essid
      }));
      
      // Update the modal immediately with network info
      document.getElementById('trackInfo').innerHTML = 
        `<strong>Network:</strong> ${essid || '<Hidden>'}<br><strong>BSSID:</strong> ${bssid}`;
    }
    
    function stopTracking() {
      if (!isConnected) return;
      
      console.log('Stopping tracking');
      ws.send(JSON.stringify({command: 'stopTrack'}));
      document.getElementById('trackerModal').style.display = 'none';
    }
    
    function showDetails(index) {
      selectedNetwork = currentNetworks[index];
      const encClass = selectedNetwork.enc === 'Open' ? 'status-open' : selectedNetwork.enc === 'Unknown' ? 'status-unknown' : 'status-secure';
      
      document.getElementById("detailsContent").innerHTML = `
        <div style="background: rgba(51, 65, 85, 0.3); padding: 1.5rem; border-radius: 0.75rem; margin-bottom: 1rem;">
          <div style="margin-bottom: 1rem;">
            <strong style="color: #3b82f6;">Network Name (ESSID):</strong><br>
            <span style="font-size: 1.1rem; font-weight: 600;">${selectedNetwork.essid || '<Hidden Network>'}</span>
          </div>
          
          <div style="margin-bottom: 1rem;">
            <strong style="color: #3b82f6;">MAC Address (BSSID):</strong><br>
            <span style="font-family: monospace; background: rgba(0,0,0,0.3); padding: 0.25rem 0.5rem; border-radius: 0.25rem;">${selectedNetwork.bssid}</span>
          </div>
          
          <div style="margin-bottom: 1rem;">
            <strong style="color: #3b82f6;">Channel:</strong><br>
            <span style="font-size: 1.1rem; font-weight: 600;">${selectedNetwork.channel}</span>
          </div>
          
          <div style="margin-bottom: 1rem;">
            <strong style="color: #3b82f6;">Signal Strength:</strong><br>
            <span style="font-size: 1.2rem; font-weight: 700; color: ${selectedNetwork.rssi > -50 ? '#10b981' : selectedNetwork.rssi > -70 ? '#f59e0b' : '#ef4444'};">${selectedNetwork.rssi} dBm</span>
            <br><small style="color: #94a3b8;">${getSignalQuality(selectedNetwork.rssi)}</small>
          </div>
          
          <div>
            <strong style="color: #3b82f6;">Security:</strong><br>
            <span class="security-indicator ${encClass}"></span><span style="font-weight: 600;">${selectedNetwork.enc}</span>
          </div>
        </div>
      `;
      
      document.getElementById("detailsModal").style.display = "flex";
    }
    
    function getSignalQuality(rssi) {
      if (rssi > -50) return "Excellent signal quality";
      if (rssi > -60) return "Good signal quality";
      if (rssi > -70) return "Fair signal quality";
      if (rssi > -80) return "Poor signal quality";
      return "Very poor signal quality";
    }
    
    function closeDetailsModal() {
      document.getElementById("detailsModal").style.display = "none";
      selectedNetwork = null;
    }
    
    function trackFromDetails() {
      if (selectedNetwork && isConnected) {
        closeDetailsModal();
        startTracking(selectedNetwork.bssid, selectedNetwork.essid);
      }
    }
    
    // Initialize WebSocket connection when page loads
    document.addEventListener('DOMContentLoaded', function() {
      console.log('Page loaded, initializing WebSocket...');
      initWebSocket();
      
      // Initialize filters to default state
      filteredNetworks = currentNetworks;
      updateResultsSummary();
    });
    
    // Handle page visibility changes
    document.addEventListener('visibilitychange', function() {
      if (!document.hidden) {
        console.log('Page visible');
        if (!isConnected) {
          initWebSocket();
        }
        if (isTracking && chartCtx) {
          drawChart();
        }
      }
    });
    
    // Handle window resize
    window.addEventListener('resize', function() {
      if (chartCtx) {
        setTimeout(() => {
          initChart();
        }, 100);
      }
    });
    
    // Keyboard shortcuts
    document.addEventListener('keydown', function(event) {
      // Ctrl/Cmd + F to focus search
      if ((event.ctrlKey || event.metaKey) && event.key === 'f') {
        event.preventDefault();
        document.getElementById('searchInput').focus();
      }
      
      // Escape to clear search
      if (event.key === 'Escape') {
        if (document.getElementById('searchInput') === document.activeElement) {
          clearFilters();
        }
      }
    });

    function initChart() {
      const canvas = document.getElementById('signalChart');
      if (!canvas) return;
      
      // Set up high DPI support
      const dpr = window.devicePixelRatio || 1;
      const rect = canvas.getBoundingClientRect();
      canvas.width = rect.width * dpr;
      canvas.height = rect.height * dpr;
      
      chartCtx = canvas.getContext('2d');
      chartCtx.scale(dpr, dpr);
      
      // Clear any existing data
      signalHistory = [];
      drawChart();
    }

    // Export Functions
    function toggleExports() {
      const content = document.getElementById('exportContent');
      const toggle = document.getElementById('exportToggle');
      
      if (content.classList.contains('active')) {
        content.classList.remove('active');
        toggle.textContent = 'Show Export';
        toggle.classList.remove('active');
      } else {
        content.classList.add('active');
        toggle.textContent = 'Hide Export';
        toggle.classList.add('active');
      }
    }

    function exportNetworks(format) {
      const exportScope = document.querySelector('input[name="exportScope"]:checked').value;
      const networksToExport = exportScope === 'all' ? currentNetworks : filteredNetworks;
      
      if (networksToExport.length === 0) {
        alert('No networks to export. Please scan for networks first.');
        return;
      }
      
      let content, filename, mimeType;
      const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, -5);
      
      switch (format) {
        case 'csv':
          content = generateCSV(networksToExport);
          filename = `wifi-scan-${timestamp}.csv`;
          mimeType = 'text/csv';
          break;
        case 'json':
          content = generateJSON(networksToExport);
          filename = `wifi-scan-${timestamp}.json`;
          mimeType = 'application/json';
          break;
        case 'txt':
          content = generateTextReport(networksToExport);
          filename = `wifi-report-${timestamp}.txt`;
          mimeType = 'text/plain';
          break;
      }
      
      downloadFile(content, filename, mimeType);
    }

    function generateCSV(networks) {
      const headers = ['Network Name', 'BSSID', 'Channel', 'Signal (dBm)', 'Security', 'Signal Quality'];
      const rows = [headers.join(',')];
      
      networks.forEach(network => {
        const row = [
          `"${(network.essid || 'Hidden').replace(/"/g, '""')}"`,
          `"${network.bssid}"`,
          network.channel,
          network.rssi,
          `"${network.enc}"`,
          `"${getSignalQuality(network.rssi)}"`
        ];
        rows.push(row.join(','));
      });
      
      return rows.join('\n');
    }

    function generateJSON(networks) {
      const exportData = {
        timestamp: new Date().toISOString(),
        scanInfo: {
          totalNetworks: networks.length,
          exportType: document.querySelector('input[name="exportScope"]:checked').value,
          filters: getActiveFilters()
        },
        networks: networks.map(network => ({
          essid: network.essid || null,
          bssid: network.bssid,
          channel: network.channel,
          rssi: network.rssi,
          security: network.enc,
          signalQuality: getSignalQuality(network.rssi),
          isHidden: !network.essid || network.essid.trim() === ''
        }))
      };
      
      return JSON.stringify(exportData, null, 2);
    }

    function generateTextReport(networks) {
      const timestamp = new Date().toLocaleString();
      const exportType = document.querySelector('input[name="exportScope"]:checked').value;
      
      let report = `WiFi Network Scan Report\n`;
      report += `Generated: ${timestamp}\n`;
      report += `Total Networks: ${networks.length}\n`;
      report += `Export Type: ${exportType === 'all' ? 'All Networks' : 'Filtered Results'}\n`;
      
      const filters = getActiveFilters();
      if (Object.keys(filters).length > 0) {
        report += `\nActive Filters:\n`;
        Object.entries(filters).forEach(([key, value]) => {
          report += `  ${key}: ${value}\n`;
        });
      }
      
      report += `\n${'='.repeat(80)}\n\n`;
      
      // Group by security type
      const securityGroups = {};
      networks.forEach(network => {
        const security = network.enc;
        if (!securityGroups[security]) securityGroups[security] = [];
        securityGroups[security].push(network);
      });
      
      Object.entries(securityGroups).forEach(([security, nets]) => {
        report += `${security} Networks (${nets.length}):\n`;
        report += `-`.repeat(40) + '\n';
        
        nets.sort((a, b) => b.rssi - a.rssi).forEach(network => {
          const name = network.essid || '<Hidden>';
          const quality = getSignalQuality(network.rssi);
          report += `  ${name.padEnd(25)} | ${network.bssid} | Ch ${network.channel.toString().padStart(2)} | ${network.rssi.toString().padStart(4)} dBm | ${quality}\n`;
        });
        report += '\n';
      });
      
      // Summary statistics
      const signals = networks.map(n => n.rssi);
      const avgSignal = Math.round(signals.reduce((a, b) => a + b, 0) / signals.length);
      const maxSignal = Math.max(...signals);
      const minSignal = Math.min(...signals);
      
      report += `Summary Statistics:\n`;
      report += `-`.repeat(20) + '\n';
      report += `Average Signal: ${avgSignal} dBm\n`;
      report += `Strongest Signal: ${maxSignal} dBm\n`;
      report += `Weakest Signal: ${minSignal} dBm\n`;
      report += `Open Networks: ${networks.filter(n => n.enc === 'Open').length}\n`;
      report += `Secured Networks: ${networks.filter(n => n.enc !== 'Open').length}\n`;
      report += `Hidden Networks: ${networks.filter(n => !n.essid || n.essid.trim() === '').length}\n`;
      
      // Channel distribution
      const channels = {};
      networks.forEach(network => {
        channels[network.channel] = (channels[network.channel] || 0) + 1;
      });
      
      report += `\nChannel Distribution:\n`;
      report += `-`.repeat(20) + '\n';
      Object.entries(channels).sort(([a], [b]) => parseInt(a) - parseInt(b)).forEach(([channel, count]) => {
        report += `Channel ${channel}: ${count} network${count !== 1 ? 's' : ''}\n`;
      });
      
      return report;
    }

    function getActiveFilters() {
      const filters = {};
      
      const search = document.getElementById('searchInput').value;
      if (search) filters['Search'] = search;
      
      const security = document.getElementById('securityFilter').value;
      if (security) filters['Security'] = security;
      
      const signal = document.getElementById('signalFilter').value;
      if (signal) filters['Signal Strength'] = signal;
      
      const channel = document.getElementById('channelFilter').value;
      if (channel) filters['Channel'] = channel;
      
      if (!showHidden) filters['Hidden Networks'] = 'Excluded';
      
      return filters;
    }

    function exportSignalHistory(format) {
      if (signalHistory.length === 0) {
        alert('No signal history to export. Start tracking a device first.');
        return;
      }
      
      let content, filename, mimeType;
      const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, -5);
      const trackingBSSID = document.getElementById('trackInfo').textContent.match(/BSSID:\s*([^\s]+)/)?.[1] || 'unknown';
      
      switch (format) {
        case 'csv':
          content = generateSignalCSV();
          filename = `signal-history-${trackingBSSID}-${timestamp}.csv`;
          mimeType = 'text/csv';
          break;
        case 'json':
          content = generateSignalJSON();
          filename = `signal-history-${trackingBSSID}-${timestamp}.json`;
          mimeType = 'application/json';
          break;
      }
      
      downloadFile(content, filename, mimeType);
    }

    function generateSignalCSV() {
      const headers = ['Timestamp', 'Unix Timestamp', 'Signal (dBm)', 'Signal Quality'];
      const rows = [headers.join(',')];
      
      signalHistory.forEach(point => {
        const date = new Date(point.timestamp).toISOString();
        const quality = point.signal === 'Not Found' ? 'Not Found' : getSignalQuality(point.signal);
        const row = [
          `"${date}"`,
          point.timestamp,
          point.signal,
          `"${quality}"`
        ];
        rows.push(row.join(','));
      });
      
      return rows.join('\n');
    }

    function generateSignalJSON() {
      const trackInfo = document.getElementById('trackInfo').innerHTML;
      const networkName = trackInfo.match(/Network:\s*<\/strong>\s*([^<]+)/)?.[1] || 'Unknown';
      const bssid = trackInfo.match(/BSSID:\s*<\/strong>\s*([^<]+)/)?.[1] || 'Unknown';
      
      const signals = signalHistory.map(p => p.signal).filter(s => s !== 'Not Found');
      const stats = signals.length > 0 ? {
        average: Math.round(signals.reduce((a, b) => a + b, 0) / signals.length),
        min: Math.min(...signals),
        max: Math.max(...signals),
        dataPoints: signals.length
      } : null;
      
      const exportData = {
        timestamp: new Date().toISOString(),
        trackingInfo: {
          networkName: networkName,
          bssid: bssid,
          duration: signalHistory.length > 0 ? signalHistory[signalHistory.length - 1].timestamp - signalHistory[0].timestamp : 0,
          totalDataPoints: signalHistory.length
        },
        statistics: stats,
        signalHistory: signalHistory.map(point => ({
          timestamp: point.timestamp,
          datetime: new Date(point.timestamp).toISOString(),
          signal: point.signal,
          signalQuality: point.signal === 'Not Found' ? 'Not Found' : getSignalQuality(point.signal)
        }))
      };
      
      return JSON.stringify(exportData, null, 2);
    }

    function downloadFile(content, filename, mimeType) {
      const blob = new Blob([content], { type: mimeType });
      const url = URL.createObjectURL(blob);
      const link = document.createElement('a');
      link.href = url;
      link.download = filename;
      document.body.appendChild(link);
      link.click();
      document.body.removeChild(link);
      URL.revokeObjectURL(url);
      
      // Show success message
      showExportSuccess(filename);
    }

    function showExportSuccess(filename) {
      const message = document.createElement('div');
      message.style.cssText = `
        position: fixed;
        top: 20px;
        right: 20px;
        background: linear-gradient(135deg, #10b981 0%, #059669 100%);
        color: white;
        padding: 1rem 1.5rem;
        border-radius: 0.75rem;
        box-shadow: 0 10px 30px rgba(0, 0, 0, 0.3);
        z-index: 1001;
        font-weight: 600;
        animation: slideIn 0.3s ease;
      `;
      message.innerHTML = `✅ Exported: ${filename}`;
      
      document.body.appendChild(message);
      setTimeout(() => {
        message.style.animation = 'slideOut 0.3s ease';
        setTimeout(() => document.body.removeChild(message), 300);
      }, 3000);
    }

    // Update tracking status to show/hide signal export options
    function updateTrackingUI(isTracking) {
      const signalExportGroup = document.getElementById('signalExportGroup');
      if (signalExportGroup) {
        signalExportGroup.style.display = isTracking ? 'block' : 'none';
      }
    }
  </script>
</body>
</html>
  )rawliteral";

  server.send(200, "text/html", page);
}

void handleScan() {
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; ++i) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"essid\":\"" + WiFi.SSID(i) + "\",";
    json += "\"bssid\":\"" + WiFi.BSSIDstr(i) + "\",";
    json += "\"channel\":" + String(WiFi.channel(i)) + ",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";

    json += "\"enc\":\"" + getEncType(WiFi.encryptionType(i)) + "\"";
    json += "}";
  }
  json += "]";
  WiFi.scanDelete();
  server.send(200, "application/json", json);
}

void handleTrack() {
  String targetBSSID = server.arg("bssid");
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; ++i) {
    if (WiFi.BSSIDstr(i) == targetBSSID) {
      server.send(200, "text/plain", String(WiFi.RSSI(i)));
      return;
    }
  }
  server.send(404, "text/plain", "Not found");
}

String getEncType(wifi_auth_mode_t enc) {
  switch (enc) {
    case WIFI_AUTH_OPEN: return "Open";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-Ent";
    default: return "Unknown";
  }
}
