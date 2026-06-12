/*
  DS18B20ConfigScript.h - Static assets for the DS18B20 sensor assignment
  config portal page.

  Two constants are defined:

  DS18B20_HEAD_ELEMENTS  (static RAM, ~200 bytes)
      Combined <script src> and <style> tags injected into the portal <head>
      via ESPAsync_WiFiManager::setCustomHeadElement(). Must be static RAM —
      setCustomHeadElement() stores a raw pointer and wraps it with String(ptr),
      which on ESP8266 cannot read PROGMEM addresses transparently.

  DS18B20_CONFIG_JS  (PROGMEM flash, ~2 KB)
      Full JavaScript served as /ds18b20.js via AsyncWebServer::send_P().
      send_P() streams directly from flash — zero RAM copy.
      The functions defined here read slotNames[] and savedSlots[] from the
      global scope; those variables are injected by the small inline <script>
      block built at runtime in ThermostatDevice::onAddConfigParameters().
*/

#pragma once
#include <pgmspace.h>

// ---------------------------------------------------------------------------
// DS18B20_HEAD_ELEMENTS
// Injected into <head> via wm.setCustomHeadElement(DS18B20_HEAD_ELEMENTS).
//
// Contains:
//   <script src='/ds18b20.js'>  — loads the external JS functions
//   <style>.ds18btn{...}</style> — compact up/down button style that matches
//                                  the portal color palette (blue, .3rem radius)
//
// NOT PROGMEM: setCustomHeadElement stores the pointer and later does
// String(_customHeadElement), which on ESP8266 requires a regular RAM address.
// ---------------------------------------------------------------------------
static const char DS18B20_HEAD_ELEMENTS[] =
    "<script src='/ds18b20.js'></script>"
    "<style>"
    ".ds18btn{"
        "padding:2px 8px;margin:1px;cursor:pointer;"
        "border-radius:.3rem;"
        "background:#e8f4fd;"
        "border:1px solid rgba(25,118,210,.3);"
        "color:#1976d2;font-size:.95rem;"
    "}"
    ".ds18btn:hover{background:#d0eaf8;}"
    ".ds18btn:active{transform:translateY(1px);}"
    "</style>";

// ---------------------------------------------------------------------------
// DS18B20_CONFIG_JS
// Served as GET /ds18b20.js via req->send_P(200,"application/javascript",...).
//
// Defines these global functions (no auto-execution on load):
//   ds18Render()       — rebuild the table DOM from sensorData[]
//   ds18Up(i)          — swap sensorData[i] with sensorData[i-1], re-render
//   ds18Dn(i)          — swap sensorData[i] with sensorData[i+1], re-render
//   ds18UpdateTemps()  — fetch /api/ds18b20/temps, update temps by address match
//   ds18Save()         — GET /api/ds18b20/assign with current slot order
//   ds18Init(data)     — called once with initial fetch result; orders rows by
//                         savedSlots[] if available, then appends unmatched sensors
//
// The inline <script> in the portal body (built by onAddConfigParameters) sets:
//   var slotNames = [...];   // role names, one per slot
//   var savedSlots = [...];  // stored ROM addresses in slot order (null if unset)
// and then calls:
//   fetch('/api/ds18b20/temps').then(...).then(ds18Init);
//   setInterval(ds18UpdateTemps, 2000);
//
// Table uses portal's existing .sttbl class (no custom table CSS needed).
// Up/down buttons use .ds18btn class (defined in DS18B20_HEAD_ELEMENTS above).
// "Save sensor assignment" button uses .formbtn.secbtn (portal's outline style).
// ---------------------------------------------------------------------------
const char DS18B20_CONFIG_JS[] PROGMEM = R"js(
var sensorData = [];

function ds18Render() {
  var n = sensorData.length;
  var h = '<tr><th>Role</th><th>Address</th><th>Temperature</th><th>Move</th></tr>';
  for (var i = 0; i < n; i++) {
    var nm = (typeof slotNames !== 'undefined' && slotNames[i])
             ? slotNames[i] : ('Slot ' + i);
    var up = (i > 0)
             ? '<button type="button" class="ds18btn" onclick="ds18Up(' + i + ')">&#8679;</button>'
             : '';
    var dn = (i < n - 1)
             ? '<button type="button" class="ds18btn" onclick="ds18Dn(' + i + ')">&#8681;</button>'
             : '';
    h += '<tr>'
       + '<td>' + nm + '</td>'
       + '<td>' + sensorData[i].addr + '</td>'
       + '<td id="dt' + i + '">' + sensorData[i].temp + ' &deg;C</td>'
       + '<td>' + up + dn + '</td>'
       + '</tr>';
  }
  document.getElementById('ds18tbl').innerHTML = h;
}

function ds18Up(i) {
  var t = sensorData[i]; sensorData[i] = sensorData[i - 1]; sensorData[i - 1] = t;
  ds18Render();
}

function ds18Dn(i) {
  var t = sensorData[i]; sensorData[i] = sensorData[i + 1]; sensorData[i + 1] = t;
  ds18Render();
}

function ds18UpdateTemps() {
  fetch('/api/ds18b20/temps')
    .then(function(r) { return r.json(); })
    .then(function(data) {
      var m = {};
      data.forEach(function(s) { m[s.addr] = s.temp; });
      for (var i = 0; i < sensorData.length; i++) {
        var t = m[sensorData[i].addr];
        if (t !== undefined) {
          sensorData[i].temp = t;
          var el = document.getElementById('dt' + i);
          if (el) el.textContent = t + ' °C';
        }
      }
    });
}

function ds18Save() {
  var url = '/api/ds18b20/assign?';
  for (var i = 0; i < sensorData.length; i++) {
    if (i > 0) url += '&';
    url += 's' + i + '=' + sensorData[i].addr;
  }
  fetch(url)
    .then(function(r) { return r.json(); })
    .then(function(d) {
      document.getElementById('ds18msg').textContent = d.ok ? ' Saved!' : ' Error!';
    });
}

function ds18Init(data) {
  var byAddr = {};
  data.forEach(function(s) { byAddr[s.addr] = s; });
  var used = {};
  sensorData = [];
  var saved = (typeof savedSlots !== 'undefined') ? savedSlots : [];
  for (var i = 0; i < saved.length; i++) {
    if (saved[i] && byAddr[saved[i]]) {
      sensorData.push({ addr: saved[i], temp: byAddr[saved[i]].temp });
      used[saved[i]] = true;
    }
  }
  // Append any sensor not covered by the saved assignment (new sensor or first run).
  data.forEach(function(s) {
    if (!used[s.addr]) sensorData.push({ addr: s.addr, temp: s.temp });
  });
  ds18Render();
}
)js";
