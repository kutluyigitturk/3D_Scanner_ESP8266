/*
 * ============================================================
 *  3D SCANNER — ESP8266 WiFi Server
 *
 *  [1] SPIFFS WRITE ERROR FIX:
 *      - SPIFFS removed entirely, replaced with LittleFS.
 *        (SPIFFS is deprecated in ESP8266 Arduino Core 3.x)
 *      - If LittleFS.begin() fails, filesystem is auto-formatted.
 *  [2] LIVE STREAMING:
 *      - New points are buffered in RAM.
 *      - /newpoints endpoint returns buffered points and clears the buffer.
 *      - Renderer.h polls /newpoints every 2 seconds and dynamically
 *        appends incoming points to the WebGL buffer.
 *  [3] File kept open (inherited from v2.0): opened on START, closed on END.
 *
 *  SETUP NOTE:
 *  Arduino IDE → Tools → Flash Size → "4MB (FS:2MB OTA:~1MB)"
 *  No separate "ESP8266 LittleFS Data Upload" tool needed;
 *  files are created entirely by the firmware.
 * ============================================================
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include "Renderer.h"

ESP8266WebServer server(80);

// ─── State ───────────────────────────────────────────────────
enum ScanState { IDLE, SCANNING, DONE, FS_ERROR };
ScanState scanState  = IDLE;
int       pointCount = 0;

// ─── RAM buffer for live streaming ───────────────────────────
// Each point is "x,y,z" ~20 chars. 100 points = ~2KB → safe.
const int  LIVE_BUF_SIZE = 100;
String     liveBuf[LIVE_BUF_SIZE];
int        liveBufHead  = 0;  // Write index
int        liveBufTail  = 0;  // Read index
int        liveBufCount = 0;

void livePush(const String& point) {
  if (liveBufCount < LIVE_BUF_SIZE) {
    liveBuf[liveBufHead] = point;
    liveBufHead = (liveBufHead + 1) % LIVE_BUF_SIZE;
    liveBufCount++;
  }
  // If buffer is full, oldest data is dropped (graceful degradation)
}

// ─── File handle ─────────────────────────────────────────────
File dataFile;
bool fileOpen = false;

void safeCloseFile() {
  if (fileOpen && dataFile) {
    dataFile.close();
    fileOpen = false;
  }
}

// ─── /  Main page ─────────────────────────────────────────────
void handleRoot() {
  server.send(200, "text/html", renderer);
}

// ─── /vertices.js  Completed point cloud ──────────────────────
void handleVertices() {
  if (scanState == SCANNING || scanState == IDLE) {
    // Scan not finished or not yet started — return empty array
    server.send(200, "application/javascript",
      "var vertices = new Float32Array([]);");
    return;
  }
  if (!LittleFS.exists("/distance.js")) {
    server.send(200, "application/javascript",
      "var vertices = new Float32Array([]);");
    return;
  }
  File f = LittleFS.open("/distance.js", "r");
  if (!f) {
    server.send(200, "application/javascript",
      "var vertices = new Float32Array([]);");
    return;
  }
  server.streamFile(f, "application/javascript");
  f.close();
}

// ─── /newpoints  Polling endpoint for live streaming ─────────
// Returns buffered new points as a JSON array and clears the buffer.
// Renderer.h polls this every 2 seconds.
void handleNewPoints() {
  String json = "[";
  int count = 0;
  while (liveBufCount > 0) {
    if (count > 0) json += ",";
    json += "\"" + liveBuf[liveBufTail] + "\"";
    liveBufTail  = (liveBufTail + 1) % LIVE_BUF_SIZE;
    liveBufCount--;
    count++;
  }
  json += "]";

  // Cache-Control header to prevent browser caching
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", json);
}

// ─── /scan  Start scan ────────────────────────────────────────
void handleScan() {
  if (scanState == SCANNING) {
    server.send(409, "text/plain", "Scan already in progress.");
    return;
  }
  if (scanState == FS_ERROR) {
    server.send(500, "text/plain",
      "Filesystem error. Please go to /format to reformat.");
    return;
  }
  Serial.println("SCAN");  // Send command to Arduino
  server.send(200, "text/plain", "OK");
}

// ─── /status  JSON status ─────────────────────────────────────
void handleStatus() {
  const char* states[] = {"idle", "scanning", "done", "fs_error"};
  String json = "{\"state\":\"";
  json += states[(int)scanState];
  json += "\",\"points\":";
  json += pointCount;
  json += ",\"buffered\":";
  json += liveBufCount;
  json += "}";
  server.send(200, "application/json", json);
}

// ─── /reset  Clear previous scan data ────────────────────────
void handleReset() {
  if (scanState == SCANNING) {
    server.send(409, "text/plain", "Scan in progress. Cannot reset.");
    return;
  }
  safeCloseFile();
  LittleFS.remove("/distance.js");
  scanState    = IDLE;
  pointCount   = 0;
  liveBufHead  = liveBufTail = liveBufCount = 0;
  server.send(200, "text/plain", "Reset complete. Use /scan to start a new scan.");
}

// ─── /format  Format LittleFS (emergency use) ─────────────────
void handleFormat() {
  safeCloseFile();
  bool ok = LittleFS.format();
  if (ok && LittleFS.begin()) {
    scanState = IDLE;
    server.send(200, "text/plain", "Format successful.");
  } else {
    server.send(500, "text/plain", "Format failed. Possible hardware issue.");
  }
}

// ─── Setup ───────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);

  // Initialize LittleFS — auto-format if it fails
  if (!LittleFS.begin()) {
    // Format on first use or corrupted filesystem
    LittleFS.format();
    if (!LittleFS.begin()) {
      scanState = FS_ERROR;
      // Server will still run, /format endpoint remains active
    }
  }

  WiFi.softAP("3D_Scanner_Project", "12345678");

  server.on("/",            handleRoot);
  server.on("/vertices.js", handleVertices);
  server.on("/newpoints",   handleNewPoints);
  server.on("/scan",        handleScan);
  server.on("/status",      handleStatus);
  server.on("/reset",       handleReset);
  server.on("/format",      handleFormat);

  server.begin();
}

// ─── Loop ────────────────────────────────────────────────────
void loop() {
  server.handleClient();

  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    data.trim();
    if (data.length() == 0) return;

    if (data == "START") {
      safeCloseFile();
      if (scanState != FS_ERROR) {
        dataFile = LittleFS.open("/distance.js", "w");
        if (dataFile) {
          dataFile.print("var vertices = new Float32Array([");
          fileOpen = true;
        } else {
          scanState = FS_ERROR;
          return;
        }
      }
      scanState    = SCANNING;
      pointCount   = 0;
      liveBufHead  = liveBufTail = liveBufCount = 0;
    }

    else if (data == "END") {
      if (fileOpen) {
        dataFile.print("]);");
        safeCloseFile();
      }
      scanState = DONE;
    }

    else if (data.indexOf(',') != -1 && scanState == SCANNING) {
      // Write to LittleFS
      if (fileOpen) {
        dataFile.print(data);
        dataFile.print(",");
        if (pointCount % 50 == 0) dataFile.flush();
      }
      // Push to live buffer
      livePush(data);
      pointCount++;
    }
  }
}