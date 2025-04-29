
#include <SD.h>
#include <M5GFX.h>
#include <M5Unified.h>
#include <WiFiManager.h>
#include <TimeLib.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "WindSpeed.h"
#include "WindSpeedDisplay.h"
#include "BatteryDisplay.h"
#include "StartupDisplay.h"
#include <Preferences.h>

// constants
#define WINDSPEED_PIN 19
#define EVALUATION_RANGE 300
#define SAMPLE_RATE 1000            // ms
#define WINDSPEED_THRESHOLD 8       // m/s
#define WINDSPEED_DURATION_RANGE 20 // samples
#define NTP_SYNC_INTERVAL 600
#define VOLUME 100            // %
#define DISPLAY_BRIGHTNESS 50 // %
#define CHARGE_CURRENT 800    // mA
#define PREFERENCE_NAMESPACE "f3xwind"
#define MDNSNAME "f3xwind"
#define AP_SSID "f3xwind Accesspoint"

// structs, enums
struct Settings
{
  int Volume;
  int CalibrationFactor;
  int Threshold;
  int DurationRange;
  int DisplayBrightness;
  int MaximumChargeCurrent;
};

// global variables
Preferences preferences;
WiFiManager wifiManager;
Settings settings = {VOLUME, 1, WINDSPEED_THRESHOLD, WINDSPEED_DURATION_RANGE, DISPLAY_BRIGHTNESS, CHARGE_CURRENT};
WindSpeed windSpeed(WINDSPEED_PIN, EVALUATION_RANGE, settings.Threshold, settings.DurationRange, settings.CalibrationFactor);
WindSpeedDisplay windSpeedDisplay(EVALUATION_RANGE, settings.Threshold, settings.DurationRange, &windSpeed);
BatteryDisplay batteryDisplay;
StartupDisplay startupDisplay;

WiFiUDP Udp;
AsyncWebServer server(80);
unsigned long lastMillis;
static const char *hostname = "f3xwind";
static const char ntpServerName[] = "de.pool.ntp.org";
unsigned int localPort = 8888;
const int timeZone = 0;
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];
int menuX = 0;
bool isAlarmActive = false;
bool isStartupActive = true;
bool isAPModeActive = true;

static constexpr const char *menu_x_items[4] = {"Combined", "Plot", "Number", "Stats"};

void saveSettings();

void interruptCallback(void)
{
  windSpeed.interruptCallback();
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0;          // Stratum, or type of clock
  packetBuffer[2] = 6;          // Polling Interval
  packetBuffer[3] = 0xEC;       // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); // NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

time_t getNtpTime()
{
  IPAddress ntpServerIP;

  while (Udp.parsePacket() > 0)
    ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500)
  {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE)
    {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE); // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

String getDownloadFilesJson()
{

  JsonDocument jsonDocument;

  File logDirectory = SD.open("/logs");

  if (!logDirectory)
  {
    return String();
  }
  if (!logDirectory.isDirectory())
  {
    return String();
  }

  File file = logDirectory.openNextFile();

  while (file)
  {
    if (!file.isDirectory())
    {
      int bytes = file.size();
      String fsize = "";
      if (bytes < 1024)
        fsize = String(bytes) + " B";
      else if (bytes < (1024 * 1024))
        fsize = String(bytes / 1024.0, 3) + " KB";
      else if (bytes < (1024 * 1024 * 1024))
        fsize = String(bytes / 1024.0 / 1024.0, 3) + " MB";
      else
        fsize = String(bytes / 1024.0 / 1024.0 / 1024.0, 3) + " GB";

      JsonObject arrayDocument = jsonDocument.add<JsonObject>();
      arrayDocument["Date"] = String(file.name()).substring(0, 10);
      arrayDocument["Filename"] = String(file.name());
      arrayDocument["Filesize"] = fsize;
      arrayDocument["DownloadUrl"] = "/downloads?filename=" + String(file.name());
    }
    file.close();
    file = logDirectory.openNextFile();
  }

  file.close();
  String jsonString;
  jsonDocument.shrinkToFit();
  serializeJson(jsonDocument, jsonString);

  return jsonString;
}

String printDirectory()
{

  String webpage;
  File root = SD.open("/logs");

  if (!root)
  {
    return String("");
  }
  if (!root.isDirectory())
  {
    return String("");
  }
  File file = root.openNextFile();

  int i = 0;
  while (file)
  {
    if (!file.isDirectory())
    {
      webpage += "<tr><td>" + String(file.name()) + "</td>";
      // webpage += "<td>" + String(file.isDirectory() ? "Dir" : "File") + "</td>";
      int bytes = file.size();
      String fsize = "";
      if (bytes < 1024)
        fsize = String(bytes) + " B";
      else if (bytes < (1024 * 1024))
        fsize = String(bytes / 1024.0, 3) + " KB";
      else if (bytes < (1024 * 1024 * 1024))
        fsize = String(bytes / 1024.0 / 1024.0, 3) + " MB";
      else
        fsize = String(bytes / 1024.0 / 1024.0 / 1024.0, 3) + " GB";
      webpage += "<td>" + fsize + "</td>";
      webpage += "<td>";
      webpage += F("<FORM action='./downloads' method='get'>");
      webpage += F("<button type='submit' name='filename'");
      webpage += F("' value='");
      webpage += String(file.name());
      webpage += F("'>Download</button>");
      webpage += "</td>";
      webpage += "</tr>";
    }
    file = root.openNextFile();
    i++;
  }
  file.close();
  return webpage;
}

void updateVolume()
{
  M5.Speaker.setVolume((int)(settings.Volume / 100.00f * 255.0f));
}

String getSettingsJson()
{
  JsonDocument jsonDocument;

  jsonDocument["Volume"] = settings.Volume;
  jsonDocument["Threshold"] = settings.Threshold;
  jsonDocument["CalibrationFactor"] = settings.CalibrationFactor;
  jsonDocument["DurationRange"] = settings.DurationRange;
  jsonDocument["DisplayBrightness"] = settings.DisplayBrightness;
  jsonDocument["MaximumChargeCurrent"] = settings.MaximumChargeCurrent;

  String jsonString;
  jsonDocument.shrinkToFit();
  serializeJson(jsonDocument, jsonString);
  return jsonString;
}

String getStatusJson()
{
  JsonDocument jsonDocument;

  jsonDocument["BatteryLevel"] = M5.Power.getBatteryLevel();
  jsonDocument["Current"] = M5.Power.getBatteryCurrent();
  jsonDocument["IsPowerConnected"] = M5.Power.Axp192.isACIN();
  jsonDocument["IsCharging"] = M5.Power.isCharging();
  jsonDocument["WifiIpAddress"] = WiFi.localIP();
  jsonDocument["WifiRSSI"] = WiFi.RSSI();

  String jsonString;
  jsonDocument.shrinkToFit();
  serializeJson(jsonDocument, jsonString);
  return jsonString;
}

void updateSettings()
{
  windSpeed.updateSettings(settings.Threshold, settings.DurationRange, settings.CalibrationFactor);
  windSpeedDisplay.updateSettings(settings.Threshold, settings.DurationRange, settings.DisplayBrightness);
  updateVolume();
  M5.Power.Axp192.setChargeCurrent(settings.MaximumChargeCurrent);
  saveSettings();
}

void handleDownloadRequest(AsyncWebServerRequest *request)
{

  Serial.print("Method: ");
  Serial.println(request->method());

  int headers = request->headers();
  int i;
  for (i = 0; i < headers; i++)
  {
    AsyncWebHeader *h = request->getHeader(i);
    Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
  }

  Serial.println("Params: ");
  for (size_t i = 0; i < request->params(); i++)
  {
    Serial.println(request->getParam(i)->name());
  }

  Serial.println("Args: ");
  for (size_t i = 0; i < request->args(); i++)
  {
    Serial.println(request->argName(i));
  }
  if (request->hasParam("filename", false))
  { // Check for files: <input name="filename" />

    String filename2 = request->arg("filename");
    Serial.println(filename2);
    Serial.println("getParam");
    AsyncWebParameter *parameter = request->getParam(0);
    Serial.println(parameter->name());
    Serial.println(parameter->value());
    String filename = request->getParam("filename")->value();
    Serial.println("Download Filename: " + filename);
    request->send(SD, "/logs/" + filename, String(), true);
    return;
  }
  else
  {
    request->send_P(200, "application/json", getDownloadFilesJson().c_str());
  }
}

void playSound(uint32_t duration = 4294967295U)
{
  M5.Speaker.tone(440, duration);
}

void stopSound()
{
  M5.Speaker.stop();
}

void setupSoundModule()
{
  auto config = M5.config();
  config.external_speaker.atomic_spk = true;
  config.external_speaker.module_display = true;
  updateVolume();
  M5.Speaker.begin();
}

void setupDns()
{
  if (!MDNS.begin(MDNSNAME))
  {
    Serial.println("Error setting up MDNS responder!");
  }
  {
    Serial.println("Error setting up MDNS responder!");
  }
  Serial.println("mDNS responder started");
}

void setupWifi()
{

  if (isAPModeActive)
  {
    const char *ssid = AP_SSID;

    WiFi.softAP(ssid);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
  }
  else
  {
    WiFi.mode(WIFI_MODE_STA);
    wifiManager.setHostname(hostname);
    bool res;

    res = wifiManager.autoConnect(AP_SSID);

    if (!res)
    {
      Serial.println("Failed to connect");
    }
    else
    {
      Serial.println("Connected successfull");
    }

    Serial.print("IP number assigned by DHCP is ");
    Serial.println(WiFi.localIP());
  }
  setupDns();
}

void setupNtpTimeSyncProvider()
{
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(NTP_SYNC_INTERVAL);
  Serial.println("NTP Sync done");
}

void evaluationCallback()
{
  playSound();
  isAlarmActive = true;
}

void startButtonCallback(bool isAPEnabled)
{
  if (isAPEnabled)
  {
    isAPModeActive = true;
    Serial.println("Start Button pressed, AP enabled");
  }
  else
  {
    isAPModeActive = false;
    Serial.println("Start Button pressed, AP disabled");
  }
  isStartupActive = false;
  playSound(1000);
}

void setupWindspeedIO()
{
  windSpeed.setupInterruptCallback(interruptCallback);
  windSpeed.setupEvaluationCallback(&evaluationCallback);
}

// callback definition
void parseMyPageBody(AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total)
{
  DynamicJsonDocument bodyJSON(1024);
  deserializeJson(bodyJSON, data, len);
  int volume = bodyJSON["Volume"];
  int threshold = bodyJSON["Threshold"];
  int calibrationValue = bodyJSON["CalibrationValue"];
  int displayBrightness = bodyJSON["DisplayBrightness"];
  int maximumChargeCurrent = bodyJSON["MaximumChargeCurrent"];
  Serial.printf("Volume: %d, Threshold: %d, CalibrationValue: %d, MaxChargeCurrent: %d, DisplayBrightness: %d\n", volume, threshold, calibrationValue, maximumChargeCurrent, displayBrightness);
  settings.Volume = volume;
  settings.Threshold = threshold;
  settings.CalibrationFactor = calibrationValue;
  settings.DisplayBrightness = displayBrightness;
  settings.MaximumChargeCurrent = maximumChargeCurrent;
  updateSettings();
}

void handleSettings(AsyncWebServerRequest *request)
{
  Serial.println("handleSettings");
  request->send_P(200, "application/json", getSettingsJson().c_str());
}

void setupServer()
{

  server.on("/chart.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/chart.js"); });
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/favicon.ico"); });
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/index.html"); });
  server.on("/windspeed", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "application/json", windSpeed.getWindspeedJson().c_str()); });
  server.on("/evaluation", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "application/json", windSpeed.getWindspeedEvaluationJson().c_str()); });
  server.on("/downloads", HTTP_GET, [](AsyncWebServerRequest *request)
            { handleDownloadRequest(request); });
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "application/json", getSettingsJson().c_str()); });
  server.on("/settings", HTTP_POST, handleSettings, nullptr, parseMyPageBody);
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "application/json", getStatusJson().c_str()); });

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  Serial.println("Server begin");
  server.begin();
}

void setupLittleFS()
{
  if (!LittleFS.begin())
  {
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }
}
void saveSettings()
{
  preferences.begin(PREFERENCE_NAMESPACE, false);
  preferences.putInt("Volume", settings.Volume);
  preferences.putInt("Threshold", settings.Threshold);
  preferences.putInt("Calibration", settings.CalibrationFactor);
  preferences.putInt("Brightness", settings.DisplayBrightness);
  preferences.putInt("MaxCurrent", settings.MaximumChargeCurrent);
  preferences.putInt("DurationRange", settings.DurationRange);
  preferences.end();
  Serial.println("Preferences saved");
}

void setupPreferences()
{
  preferences.begin(PREFERENCE_NAMESPACE, false);
  settings.Volume = preferences.getInt("Volume", VOLUME);
  settings.Threshold = preferences.getInt("Threshold", WINDSPEED_THRESHOLD);
  settings.CalibrationFactor = preferences.getInt("Calibration", 1);
  settings.DisplayBrightness = preferences.getInt("Brightness", DISPLAY_BRIGHTNESS);
  settings.MaximumChargeCurrent = preferences.getInt("MaxCurrent", CHARGE_CURRENT);
  settings.DurationRange = preferences.getInt("DurationRange", WINDSPEED_DURATION_RANGE);
  Serial.printf("Volume: %d, Threshold: %d, CalibrationValue: %d, MaxChargeCurrent: %d, DisplayBrightness: %d\n", settings.Volume, settings.Threshold, settings.CalibrationFactor, settings.MaximumChargeCurrent, settings.DisplayBrightness);
  preferences.end();
  saveSettings();
  updateSettings();
}

void setup(void)
{
  M5.begin();
  windSpeed.setup();
  setupWindspeedIO();
  setupSoundModule();

  startupDisplay.setup(255);
  startupDisplay.setupStartButtonCallback(&startButtonCallback);
  startupDisplay.draw();
  while (isStartupActive)
  {
    /* code */
    delay(1);
    startupDisplay.evaluateTouches();
  }
  setupWifi();
  setupNtpTimeSyncProvider();
  setupLittleFS();
  setupServer();
  windSpeedDisplay.setup();
  setupPreferences();
}

void startDeepSleep()
{
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_39, 0); // gpio39 == touch INT
  delay(100);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.sleep();
  M5.Display.waitDisplay();
  esp_deep_sleep_start();
}

void evaluateTouches()
{
  auto count = M5.Touch.getCount();
  if (count != 0)
  {
    static m5::touch_state_t prev_state;
    auto touchDetail = M5.Touch.getDetail();

    if (touchDetail.wasFlicked())
    {

      if (abs(touchDetail.distanceX()) > 30 && (abs(touchDetail.distanceX()) > abs(touchDetail.distanceY())))
      {
        if (touchDetail.distanceX() > 0)
        {
          // Serial.println("Swipe RIGHT");
          if (menuX == 3)
          {
            menuX = 0;
          }
          else
          {
            menuX++;
          }
        }
        else
        {
          // Serial.println("Swipe LEFT");
          if (menuX == 0)
          {
            menuX = 3;
          }
          else
          {
            menuX--;
          }
        }
        windSpeedDisplay.draw((DrawType)menuX);
      }
    }
    if (touchDetail.wasHold())
    {
      stopSound();
    }

    if (touchDetail.wasDragged())
    {
      startDeepSleep();
    }

    if (isAlarmActive && touchDetail.getClickCount() == 2)
    {
      stopSound();
      isAlarmActive = false;
    }

    if (touchDetail.getClickCount() == 3)
    {
      wifiManager.resetSettings();
      startDeepSleep();
    }
  }
}

void loop(void)
{
  M5.delay(1);
  M5.update();

  evaluateTouches();

  long currentMillis = millis();
  if (currentMillis - lastMillis >= SAMPLE_RATE)
  {
    windSpeed.calculateWindspeed(true, true);
    lastMillis = currentMillis;
    windSpeedDisplay.draw((DrawType)menuX);
    // batteryDisplay.draw();
    Serial.printf("Level: %d, Voltage: %d, Current: %d, IsCharging:%d, ChargeCurrent: %.2f, isACin: %d \n", M5.Power.getBatteryLevel(), M5.Power.getBatteryVoltage(), M5.Power.getBatteryCurrent(), M5.Power.isCharging(), M5.Power.Axp192.getBatteryChargeCurrent(), M5.Power.Axp192.isACIN());
  }
}
