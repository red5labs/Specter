#include <WiFi.h>
#include <WebServer.h>
#include <esp_wifi.h>

// Access Point credentials
const char* AP_SSID = "Specter";
const char* AP_PASSWORD = "specter1234";

WebServer server(80);

// Function to get encryption type as string
String getEncryptionType(wifi_auth_mode_t encryptionType) {
  switch (encryptionType) {
    case WIFI_AUTH_OPEN:
      return "OPEN";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA-PSK";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2-PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA/WPA2-PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "WPA2-ENTERPRISE";
    default:
      return "UNKNOWN";
  }
}

// Function to get signal quality percentage
int getSignalQuality(int rssi) {
  if (rssi <= -100) {
    return 0;
  } else if (rssi >= -50) {
    return 100;
  } else {
    return 2 * (rssi + 100);
  }
}

// Function to get signal strength description
String getSignalStrength(int rssi) {
  if (rssi >= -50) return "Excellent";
  if (rssi >= -60) return "Good";
  if (rssi >= -70) return "Fair";
  if (rssi >= -80) return "Poor";
  return "Very Poor";
}

// Function to get frequency from channel
float getFrequency(int channel) {
  if (channel >= 1 && channel <= 13) {
    return 2.412 + (channel - 1) * 0.005;
  } else if (channel == 14) {
    return 2.484;
  } else if (channel >= 36 && channel <= 165) {
    return 5.0 + (channel - 36) * 0.005;
  }
  return 0;
}

String getAssociatedClients() {
  String clients = "";
  int numStations = WiFi.softAPgetStationNum();
  
  Serial.println("\n=== Client Information ===");
  Serial.print("Number of connected clients: ");
  Serial.println(numStations);
  
  if (numStations > 0) {
    clients += "<li style='margin-top: 10px;'><strong style='color: #4CAF50;'>Connected Clients (" + String(numStations) + "):</strong>";
    clients += "<ul style='margin: 5px 0; padding-left: 20px;'>";
    
    // Get the AP's MAC address
    uint8_t mac[6];
    if (WiFi.softAPmacAddress(mac)) {
      char macStr[18];
      sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
              mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
      clients += "<li>AP MAC Address: " + String(macStr) + "</li>";
      Serial.print("AP MAC: ");
      Serial.println(macStr);
    }
    
    clients += "<li>Connected Devices: " + String(numStations) + "</li>";
    clients += "</ul></li>";
  } else {
    clients += "<li style='margin-top: 10px;'><strong style='color: #f44336;'>No clients connected</strong></li>";
    Serial.println("No clients connected");
  }
  Serial.println("=======================\n");
  return clients;
}

String getNetworkList() {
  String webpage = "<!DOCTYPE html><html><head>";
  webpage += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  webpage += "<title>Specter WiFi Scanner</title>";
  webpage += "<style>";
  webpage += "body { font-family: Arial, sans-serif; margin: 20px; }";
  webpage += "table { border-collapse: collapse; width: 100%; max-width: 1000px; margin: 20px auto 0 auto; padding-left: 16px; padding-right: 16px; box-sizing: border-box; }";
  webpage += "th, td { border: 1px solid #ddd; padding: 8px 16px; text-align: left; }";
  webpage += "th { background-color: #4CAF50; color: white; }";
  webpage += "tr:nth-child(even) { background-color: #f2f2f2; }";
  webpage += ".refresh-btn { background-color: #4CAF50; color: white; padding: 10px 20px; ";
  webpage += "border: none; border-radius: 4px; cursor: pointer; margin-right: 10px; }";
  webpage += ".scan-btn { background-color: #2196F3; color: white; padding: 10px 20px; ";
  webpage += "border: none; border-radius: 4px; cursor: pointer; }";
  webpage += ".signal-bar { width: 100px; height: 20px; background-color: #ddd; border-radius: 3px; }";
  webpage += ".signal-level { height: 100%; background-color: #4CAF50; border-radius: 3px; }";
  webpage += ".hidden { display: none; }";
  webpage += ".toggle-btn { background-color: #666; color: white; padding: 5px 10px; border: none; border-radius: 3px; cursor: pointer; }";
  webpage += ".client-info { background: #f4f8fb; padding: 20px 24px 18px 24px; border-radius: 6px; margin: 24px 0 24px 0; box-shadow: 0 2px 8px #0001; max-width: 600px; }";
  webpage += ".client-info table { margin-top: 8px; border-collapse: collapse; width: 100%; }";
  webpage += ".client-info td { padding: 6px 10px; border: none; color: #222; }";
  webpage += ".ap-card { background: #f4f8fb; border-radius: 6px; margin: 24px auto 24px auto; box-shadow: 0 2px 8px #0001; max-width: 1000px; padding: 0; }";
  webpage += ".ap-toggle { width: 100%; background: #4CAF50; color: #fff; border: none; border-radius: 6px 6px 0 0; padding: 14px 0 14px 12px; font-size: 1.1em; font-weight: bold; cursor: pointer; text-align: left; outline: none; transition: background 0.2s; }";
  webpage += ".ap-toggle:hover { background: #388e3c; }";
  webpage += ".ap-content { padding: 0 18px 18px 18px; display: none; background: #fff; border-radius: 0 0 6px 6px; }";
  webpage += ".ap-content table { width: 100%; border-collapse: collapse; margin-top: 10px; }";
  webpage += ".ap-content td { padding: 6px 10px; border: none; color: #222; }";
  webpage += "th span[id^='sort-indicator-'] { font-size: 0.9em; margin-left: 4px; }";
  webpage += "</style>";
  webpage += "<script>";
  webpage += "var isScanning = localStorage.getItem('isScanning') === 'true';";
  webpage += "var refreshInterval;";
  webpage += "var lastSortedCol = -1;";
  webpage += "var lastSortDir = 'asc';";
  webpage += "function toggleScan() {";
  webpage += "  var scanBtn = document.getElementById('scanBtn');";
  webpage += "  if (isScanning) {";
  webpage += "    clearInterval(refreshInterval);";
  webpage += "    scanBtn.textContent = 'Start Scan';";
  webpage += "    scanBtn.style.backgroundColor = '#2196F3';";
  webpage += "    document.getElementById('scanStatus').textContent = 'Scanning stopped';";
  webpage += "    localStorage.setItem('isScanning', 'false');";
  webpage += "  } else {";
  webpage += "    refreshInterval = setInterval(function() { window.location.reload(); }, 10000);";
  webpage += "    scanBtn.textContent = 'Stop Scan';";
  webpage += "    scanBtn.style.backgroundColor = '#f44336';";
  webpage += "    document.getElementById('scanStatus').textContent = 'Auto-refreshing every 10 seconds';";
  webpage += "    localStorage.setItem('isScanning', 'true');";
  webpage += "  }";
  webpage += "  isScanning = !isScanning;";
  webpage += "}";
  webpage += "function toggleDetails(rowId) {";
  webpage += "  var btn = document.getElementById('btn_' + rowId);";
  webpage += "  var row = document.getElementById(rowId);";
  webpage += "  if (row.style.display === 'none' || row.style.display === '') {";
  webpage += "    row.style.display = 'table-row';";
  webpage += "    btn.textContent = 'Hide Details';";
  webpage += "  } else {";
  webpage += "    row.style.display = 'none';";
  webpage += "    btn.textContent = 'Show Details';";
  webpage += "  }";
  webpage += "}";
  webpage += "function toggleApInfo() {";
  webpage += "  var content = document.getElementById('apInfo');";
  webpage += "  if (content.style.display === 'none' || content.style.display === '') {";
  webpage += "    content.style.display = 'block';";
  webpage += "  } else {";
  webpage += "    content.style.display = 'none';";
  webpage += "  }";
  webpage += "}";
  webpage += "function sortTable(n) {";
  webpage += "  var table = document.getElementById('wifiTable');";
  webpage += "  var tbody = table.tBodies[0];";
  webpage += "  var rows = Array.from(tbody.rows);";
  webpage += "  var pairs = [];";
  // Group main and details rows as pairs
  webpage += "  for (var i = 0; i < rows.length; i += 2) {";
  webpage += "    pairs.push([rows[i], rows[i + 1]]);";
  webpage += "  }";
  // Determine sort direction: toggle if same column, else default to asc
  webpage += "  var dir = 'asc';";
  webpage += "  if (lastSortedCol === n) {";
  webpage += "    dir = (lastSortDir === 'asc') ? 'desc' : 'asc';";
  webpage += "  }";
  webpage += "  lastSortedCol = n;";
  webpage += "  lastSortDir = dir;";
  // Clear all indicators
  webpage += "  for (var j = 0; j < 5; j++) {";
  webpage += "    document.getElementById('sort-indicator-' + j).innerHTML = '';";
  webpage += "  }";
  // Sort the pairs
  webpage += "  pairs.sort(function(a, b) {";
  webpage += "    var x = a[0].getElementsByTagName('TD')[n].textContent.trim();";
  webpage += "    var y = b[0].getElementsByTagName('TD')[n].textContent.trim();";
  webpage += "    var xNum = parseFloat(x);";
  webpage += "    var yNum = parseFloat(y);";
  webpage += "    if (!isNaN(xNum) && !isNaN(yNum)) {";
  webpage += "      return dir === 'asc' ? xNum - yNum : yNum - xNum;";
  webpage += "    } else {";
  webpage += "      return dir === 'asc'";
  webpage += "        ? x.localeCompare(y, undefined, {numeric: true, sensitivity: 'base'})";
  webpage += "        : y.localeCompare(x, undefined, {numeric: true, sensitivity: 'base'});";
  webpage += "    }";
  webpage += "  });";
  // Re-append the sorted pairs to tbody
  webpage += "  for (var i = 0; i < pairs.length; i++) {";
  webpage += "    tbody.appendChild(pairs[i][0]);";
  webpage += "    tbody.appendChild(pairs[i][1]);";
  webpage += "  }";
  // Set the indicator for the sorted column
  webpage += "  var indicator = document.getElementById('sort-indicator-' + n);";
  webpage += "  indicator.innerHTML = dir === 'asc' ? '&uarr;' : '&darr;';";
  webpage += "}";
  webpage += "window.onload = function() {";
  webpage += "  if (isScanning) {";
  webpage += "    var scanBtn = document.getElementById('scanBtn');";
  webpage += "    scanBtn.textContent = 'Stop Scan';";
  webpage += "    scanBtn.style.backgroundColor = '#f44336';";
  webpage += "    document.getElementById('scanStatus').textContent = 'Auto-refreshing every 10 seconds';";
  webpage += "    refreshInterval = setInterval(function() { window.location.reload(); }, 10000);";
  webpage += "  }";
  webpage += "};";
  webpage += "</script>";
  webpage += "</head><body>";
  webpage += "<h1>Specter WiFi Scanner</h1>";

  // --- AP/Client Info Collapsible Card ---
  webpage += "<div class='ap-card'>";
  webpage += "<button class='ap-toggle' onclick=\"toggleApInfo()\">Access Point Info</button>";
  webpage += "<div id='apInfo' class='ap-content' style='display:none;'>";
  webpage += "<table>";
  webpage += "<tr><td style='font-weight:bold;'>AP SSID:</td><td>" + String(AP_SSID) + "</td></tr>";
  webpage += "<tr><td style='font-weight:bold;'>AP Password:</td><td>" + String(AP_PASSWORD) + "</td></tr>";
  uint8_t apmac[6];
  if (WiFi.softAPmacAddress(apmac)) {
    char apmacStr[18];
    sprintf(apmacStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            apmac[0], apmac[1], apmac[2], apmac[3], apmac[4], apmac[5]);
    webpage += "<tr><td style='font-weight:bold;'>AP MAC:</td><td>" + String(apmacStr) + "</td></tr>";
  }
  webpage += "<tr><td style='font-weight:bold;'>AP IP:</td><td>" + WiFi.softAPIP().toString() + "</td></tr>";
  int numStations = WiFi.softAPgetStationNum();
  webpage += "<tr><td style='font-weight:bold;'>Connected Clients:</td><td>" + String(numStations) + "</td></tr>";
  if (numStations > 0) {
    // List connected client MAC addresses
    wifi_sta_list_t stationList;
    if (esp_wifi_ap_get_sta_list(&stationList) == ESP_OK) {
      for (int i = 0; i < stationList.num; i++) {
        char clientMac[18];
        sprintf(clientMac, "%02X:%02X:%02X:%02X:%02X:%02X",
                stationList.sta[i].mac[0], stationList.sta[i].mac[1],
                stationList.sta[i].mac[2], stationList.sta[i].mac[3],
                stationList.sta[i].mac[4], stationList.sta[i].mac[5]);
        webpage += "<tr><td style='font-weight:bold;'>Client " + String(i+1) + " MAC:</td><td>" + String(clientMac) + "</td></tr>";
      }
    }
  }
  webpage += "</table>";
  webpage += "</div></div>";

  webpage += "<div style='margin-bottom: 20px;'>";
  webpage += "<button class='refresh-btn' onclick='window.location.reload()'>Refresh Now</button>";
  webpage += "<button id='scanBtn' class='scan-btn' onclick='toggleScan()'>Start Scan</button>";
  webpage += "<span id='scanStatus' style='margin-left: 10px; color: #666;'>Scanning stopped</span>";
  webpage += "</div>";

  // Scan for networks
  int n = WiFi.scanNetworks();

  if (n == 0) {
    webpage += "<p>No networks found!</p>";
  } else {
    webpage += "<table id='wifiTable'>";
    webpage += "<thead><tr>";
    webpage += "<th style='cursor:pointer;' onclick='sortTable(0)'>SSID <span id='sort-indicator-0'></span></th>";
    webpage += "<th style='cursor:pointer;' onclick='sortTable(1)'>Signal <span id='sort-indicator-1'></span></th>";
    webpage += "<th style='cursor:pointer;' onclick='sortTable(2)'>Channel <span id='sort-indicator-2'></span></th>";
    webpage += "<th style='cursor:pointer;' onclick='sortTable(3)'>MAC Address <span id='sort-indicator-3'></span></th>";
    webpage += "<th style='cursor:pointer;' onclick='sortTable(4)'>Encryption <span id='sort-indicator-4'></span></th>";
    webpage += "<th>Details</th>";
    webpage += "</tr></thead><tbody>";

    for (int i = 0; i < n; ++i) {
      String rowId = "details-" + String(i);
      int signalQuality = getSignalQuality(WiFi.RSSI(i));

      webpage += "<tr>";

      // SSID
      webpage += "<td>" + String(WiFi.SSID(i)) + "</td>";

      // Signal Strength with visual bar
      webpage += "<td>";
      webpage += "<div class='signal-bar'><div class='signal-level' style='width: " + String(signalQuality) + "%;'></div></div>";
      webpage += String(WiFi.RSSI(i)) + " dBm (" + getSignalStrength(WiFi.RSSI(i)) + ")";
      webpage += "</td>";

      // Channel
      webpage += "<td>" + String(WiFi.channel(i)) + "</td>";

      // MAC Address
      uint8_t* bssid = WiFi.BSSID(i);
      char macStr[18];
      sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
              bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
      webpage += "<td>" + String(macStr) + "</td>";

      // Encryption
      webpage += "<td>" + getEncryptionType(WiFi.encryptionType(i)) + "</td>";

      // Details button
      webpage += "<td><button id='btn_" + rowId + "' class='toggle-btn' onclick='toggleDetails(\"" + rowId + "\")'>Show Details</button></td>";

      webpage += "</tr>";

      // Hidden details row
      webpage += "<tr id='" + rowId + "' style='display: none;'>";
      webpage += "<td colspan='6'>";
      webpage += "<strong>Additional Information:</strong><br>";
      webpage += "<ul style='margin: 5px 0; padding-left: 20px;'>";
      webpage += "<li>Frequency: " + String(getFrequency(WiFi.channel(i)), 3) + " GHz</li>";
      webpage += "<li>Signal Quality: " + String(signalQuality) + "%</li>";
      webpage += "<li>Band: " + String(WiFi.channel(i) <= 14 ? "2.4GHz" : "5GHz") + "</li>";
      webpage += "<li>Channel Width: " + String(WiFi.channel(i) <= 14 ? "20 MHz" : "20/40/80 MHz") + "</li>";
      
      // Check if this is our AP and add client information
      if (String(WiFi.SSID(i)) == AP_SSID) {
        Serial.println("Found our AP in the scan list, adding client information");
        webpage += getAssociatedClients();
      }
      
      webpage += "</ul>";
      webpage += "</td>";
      webpage += "</tr>";
    }
    webpage += "</tbody></table>";
  }

  webpage += "</body></html>";
  return webpage;
}

void handleRoot() {
  server.send(200, "text/html", getNetworkList());
}

void setup() {
  Serial.begin(115200);
  delay(1000);  // Give serial connection time to start
  
  Serial.println("\nInitializing WiFi Scanner...");
  
  // Disconnect from any existing WiFi connections
  WiFi.disconnect(true);
  delay(1000);
  
  // Clear any existing WiFi settings
  WiFi.mode(WIFI_OFF);
  delay(1000);
  
  // Set up Access Point
  WiFi.mode(WIFI_AP);
  if (WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Serial.println("Access Point started successfully");
  } else {
    Serial.println("Failed to start Access Point!");
  }
  
  // Configure AP settings
  IPAddress IP = IPAddress(192, 168, 4, 1);
  IPAddress gateway = IPAddress(192, 168, 4, 1);
  IPAddress subnet = IPAddress(255, 255, 255, 0);
  
  WiFi.softAPConfig(IP, gateway, subnet);
  
  Serial.println("\nSpecter WiFi Scanner with Web Interface");
  Serial.println("------------------------------------");
  Serial.print("Access Point SSID: ");
  Serial.println(AP_SSID);
  Serial.print("Access Point Password: ");
  Serial.println(AP_PASSWORD);
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());
  
  // Set up web server
  server.on("/", handleRoot);
  server.begin();
  Serial.println("Web server started");
  
  // Print initial client count
  wifi_sta_list_t stationList;
  if (esp_wifi_ap_get_sta_list(&stationList) == ESP_OK) {
    Serial.print("Initial connected clients: ");
    Serial.println(stationList.num);
  } else {
    Serial.println("Error getting initial client list");
  }
}

void loop() {
  server.handleClient();
  delay(10);
} 