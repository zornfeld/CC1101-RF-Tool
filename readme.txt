CC1101 Pro RF Tool v0.8.5 — ESP32 + CC1101
============================================

ESP32 + CC1101 RF-Tool mit vollständigem WebUI. Frequenz-Scan, Brute Force, 
Rolling Code Capture, Spektrum-Analyzer — alles per Browser steuerbar.

FEATURES
--------
• 10 WebUI Tabs: Config, RX/TX, Frames, RAW, Scan, Brute, Rolling, S+B, Angriffe, WiFi
• 300-928 MHz, alle CC1101 Parameter live steuerbar
• Brute Force: 12 Protokolle (EV1527, PT2262, Came, Nice, KeeLoq...)
• Rolling Code: Jam & Capture Angriff  
• Scan+Brute: Automatisch beste Frequenz brute-forcen
• Spektrum Analyzer: Canvas-Grafik mit Peak-Erkennung
• Frame Buffer: 50 Frames speichern/analysieren/replay
• OTA Update: Firmware per Browser flashen
• WiFi AP/Client + Netzwerk-Scan

HARDWARE
--------
CC1101 → ESP32 Pins:
SCK   → GPIO 18
MISO  → GPIO 19  
MOSI  → GPIO 23
SS/CS → GPIO 5
GDO0  → GPIO 2
GDO2  → GPIO 4
OLED: SDA→GPIO 21, SCL→GPIO 22 (optional)

Board: ESP32 Dev Module, 240 MHz, 4MB Flash

INSTALLATION
------------
1. Bibliotheken (Arduino Library Manager):
   - ELECHOUSE_CC1101_SRC_DRV
   - Adafruit GFX Library  
   - Adafruit SSD1306

2. Board Settings:
   - ESP32 Dev Module
   - CPU Frequency: 240 MHz
   - Flash Size: 4MB (Default 4MB with spiffs)

3. Flashen:
   - CC1101_Pro_RF_Tool_Blue_Edition_0_8.1_FIXED.ino öffnen
   - Upload

4. WebUI öffnen:
   - WLAN: "CC1101_PRO" (Passwort: 12345678)
   - IP: http://192.168.4.1

WIFI SETUP
----------
Standard: Access Point Modus (CC1101_PRO)
- WiFi-Tab → Mit eigenem Netzwerk verbinden (Daten werden gespeichert)
- "→ AP Modus" → Zurück zum AP-Modus

API BEISPIELE (GET-Requests)
----------------------------
/setfreq?freq=433.92          Frequenz setzen
/setmod?mod=2                 ASK/OOK (0=2FSK,1=GFSK,2=ASK,3=4FSK,4=MSK)
/setpower?power=10            TX Power dBm
/tx?data=AABBCC               HEX senden
/rx                          RX Toggle (ON/OFF)
/getdata                      Letztes Paket (HEX)
/getframes                    Frames als JSON
/replayframe?idx=0&repeat=5   Frame replay
/brutestart?proto=2&bits=24&start=0&end=0&repeat=3&delay=100
/bruteprogress                Brute Status JSON
/rollingstart?freq=433.92&max=3&jam=10000&rx=20000
/spectrum?start=433&stop=434  Spektrum Daten
/scanstep?start=433&stop=434  Frequenz-Scan Schritt
/jam                          Jammer ON
/stopjam                      Jammer OFF
/tesla?variant=0&repeat=10    Tesla Ladeport
/getstatus                    Vollständiger Status JSON
/wifiscan                     Netzwerke scannen
POST /ota                     Firmware Update (multipart)

UNTERSTÜTZTE PROTOKOLLE (Brute Force)
-------------------------------------
0: Princeton  24bit
1: PT2262     12bit  
2: EV1527     24bit
3: HT12E      12bit
4: Came       12bit
5: Nice       12bit
6: FAAC       12bit
7: Hörmann    44bit
8: KeeLoq     64bit
9: Somfy RTS  56bit
10: Linear    10bit
11: Custom

RECHTLICHES
-----------
Nur für eigene Geräte und autorisierte Penetrationstests!
Jamming von Frequenzen oder Hacking fremder Systeme ist strafbar.

Lizenz: MIT License
Version: 0.8.5 (22.02.2026)
