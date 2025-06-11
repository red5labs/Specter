#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "Specter";
const char* password = "specter123";

WebServer server(80);

String deviceTable = "";

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.println("Access Point Started");

  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/track", handleTrack);

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
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
      min-height: 48px; /* Touch-friendly */
    }
    
    .btn:hover {
      transform: translateY(-2px);
      box-shadow: 0 8px 25px rgba(59, 130, 246, 0.4);
      background: linear-gradient(135deg, #2563eb 0%, #1e40af 100%);
    }
    
    .btn:active {
      transform: translateY(0);
    }
    
    .scan-btn {
      width: 100%;
      max-width: 300px;
      margin: 0 auto 2rem auto;
      display: block;
      font-size: 1.1rem;
    }
    
    /* Table responsive design */
    .table-container {
      background: rgba(30, 41, 59, 0.8);
      border-radius: 1rem;
      overflow: hidden;
      margin-top: 2rem;
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
    }
    
    td { 
      border-bottom: 1px solid rgba(71, 85, 105, 0.3);
      padding: 0.875rem 0.75rem; 
      background: rgba(51, 65, 85, 0.3);
    }
    
    tr:hover td {
      background: rgba(71, 85, 105, 0.4);
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
    }
    
         @media (max-width: 480px) {
       body { padding: 0.5rem; }
       
       .container { padding: 0 0.5rem; }
       
       h1 { margin-bottom: 1.5rem; }
       
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
       
       /* Stack action buttons vertically on very small screens */
       td .btn {
         display: block;
         width: 100%;
         margin: 0.25rem 0;
         font-size: 0.75rem;
         padding: 0.4rem 0.5rem;
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
      max-width: 400px;
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
    
    /* Loading animation */
    .loading {
      display: inline-block;
      width: 20px;
      height: 20px;
      border: 3px solid rgba(59, 130, 246, 0.3);
      border-radius: 50%;
      border-top-color: #3b82f6;
      animation: spin 1s ease-in-out infinite;
    }
    
    @keyframes spin {
      to { transform: rotate(360deg); }
    }
    
    /* Status indicators */
    .status-indicator {
      display: inline-block;
      width: 8px;
      height: 8px;
      border-radius: 50%;
      margin-right: 0.5rem;
    }
    
    .status-open { background: #10b981; }
    .status-secure { background: #f59e0b; }
    .status-unknown { background: #6b7280; }
  </style>
</head>
<body>
  <div class="container">
    <h1>Specter Wireless Dashboard</h1>
    <button class="btn scan-btn" onclick="scan()">
      <span id="scanText">Scan for Devices</span>
      <span id="scanLoader" class="loading" style="display:none; margin-left: 10px;"></span>
    </button>
    
    <div class="table-container">
      <table id="results">
        <thead>
          <tr>
            <th>Network</th>
            <th>Signal</th>
            <th>Actions</th>
          </tr>
        </thead>
        <tbody id="tableBody">
          <tr>
            <td colspan="3" style="text-align: center; padding: 2rem; color: #64748b;">
              Click "Scan for Devices" to discover nearby wireless networks
            </td>
          </tr>
        </tbody>
      </table>
    </div>
  </div>

  <div class="modal" id="trackerModal">
    <div class="modal-content">
      <h2>📡 Tracking Device</h2>
      <p id="trackInfo"></p>
      <p>Signal Strength: <span id="signal">-</span> dBm</p>
      <button class="btn" onclick="closeModal()" style="width: 100%; margin-top: 1rem;">Close Tracker</button>
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
         function scan() {
       if (isScanning) {
         stopScanning();
       } else {
         startScanning();
       }
     }

     function startScanning() {
       isScanning = true;
       const scanBtn = document.querySelector('.scan-btn');
       const scanText = document.getElementById('scanText');
       const scanLoader = document.getElementById('scanLoader');
       
       scanText.textContent = 'Stop Scanning';
       scanLoader.style.display = 'inline-block';
       
       // Perform initial scan
       performScan();
       
       // Set up continuous scanning every 3 seconds
       scanInterval = setInterval(performScan, 3000);
     }

     function stopScanning() {
       isScanning = false;
       const scanBtn = document.querySelector('.scan-btn');
       const scanText = document.getElementById('scanText');
       const scanLoader = document.getElementById('scanLoader');
       
       if (scanInterval) {
         clearInterval(scanInterval);
         scanInterval = null;
       }
       
       scanText.textContent = 'Scan for Devices';
       scanLoader.style.display = 'none';
     }

     function performScan() {
       fetch("/scan").then(r => r.json()).then(data => {
         currentNetworks = data; // Store networks for details modal
         const table = document.getElementById("tableBody");
         table.innerHTML = "";
         
         if (data.length === 0) {
           table.innerHTML = `<tr><td colspan="3" style="text-align: center; padding: 2rem; color: #64748b;">No networks found - scanning continues...</td></tr>`;
         } else {
           data.forEach((d, index) => {
             const encClass = d.enc === 'Open' ? 'status-open' : d.enc === 'Unknown' ? 'status-unknown' : 'status-secure';
             table.innerHTML += `<tr>
               <td style="font-weight: 600;">${d.essid || '<Hidden>'}</td>
               <td style="font-weight: 600; color: ${d.rssi > -50 ? '#10b981' : d.rssi > -70 ? '#f59e0b' : '#ef4444'};">${d.rssi} dBm</td>
               <td>
                 <button class="btn" onclick="showDetails(${index})" style="margin-right: 0.25rem; font-size: 0.8rem;">Details</button>
                 <button class="btn" onclick="track('${d.bssid}', '${d.essid}')" style="font-size: 0.8rem;">Track</button>
               </td>
             </tr>`;
           });
         }
       }).catch(err => {
         console.error('Scan failed:', err);
         if (isScanning) {
           document.getElementById("tableBody").innerHTML = `<tr><td colspan="3" style="text-align: center; padding: 2rem; color: #f59e0b;">Scan error - retrying...</td></tr>`;
         }
       });
     }

         let tracking = "";
     let currentNetworks = [];
     let selectedNetwork = null;
     let scanInterval = null;
     let isScanning = false;

     function track(bssid, essid) {
       // Pause main scanning when tracking starts
       if (isScanning) {
         stopScanning();
       }
       
       tracking = bssid;
       document.getElementById("trackerModal").style.display = "flex";
       document.getElementById("trackInfo").innerHTML = `<strong>Network:</strong> ${essid || '<Hidden>'}<br><strong>BSSID:</strong> ${bssid}`;
       pollSignal();
     }

     function closeModal() {
       document.getElementById("trackerModal").style.display = "none";
       tracking = "";
       
       // Resume main scanning when tracking stops
       if (!isScanning) {
         startScanning();
       }
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
           </div>
           
           <div>
             <strong style="color: #3b82f6;">Security:</strong><br>
             <span class="status-indicator ${encClass}"></span><span style="font-weight: 600;">${selectedNetwork.enc}</span>
           </div>
         </div>
       `;
       
       document.getElementById("detailsModal").style.display = "flex";
     }

     function closeDetailsModal() {
       document.getElementById("detailsModal").style.display = "none";
       selectedNetwork = null;
     }

     function trackFromDetails() {
       if (selectedNetwork) {
         // Pause main scanning when tracking starts
         if (isScanning) {
           stopScanning();
         }
         
         // Open tracking modal first
         tracking = selectedNetwork.bssid;
         document.getElementById("trackInfo").innerHTML = `<strong>Network:</strong> ${selectedNetwork.essid || '<Hidden>'}<br><strong>BSSID:</strong> ${selectedNetwork.bssid}`;
         document.getElementById("trackerModal").style.display = "flex";
         
         // Then close details modal
         document.getElementById("detailsModal").style.display = "none";
         selectedNetwork = null;
         
         // Start polling signal
         pollSignal();
       }
     }

    function pollSignal() {
      if (!tracking) return;
      fetch(`/track?bssid=${tracking}`).then(r => r.text()).then(signal => {
        document.getElementById("signal").innerText = signal;
        setTimeout(pollSignal, 1000);
      }).catch(err => {
        console.error('Tracking failed:', err);
        document.getElementById("signal").innerText = 'Error';
      });
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
