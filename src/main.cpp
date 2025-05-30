
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
#include "StartupDisplay.h"
#include <Preferences.h>
#include "WifiConfigDisplay.h"
#include <ESPAsyncHTTPUpdateServer.h>

// constants
#define WINDSPEED_PIN 19
#define WINDSPEED_EVALUATION_RANGE 300
#define SAMPLE_RATE 1000            // ms
#define WINDSPEED_LOWER_THRESHOLD 0 // m/s
#define WINDSPEED_UPPER_THRESHOLD 8       // m/s
#define WINDSPEED_DURATION_RANGE 20 // samples
#define WINDSPEED_NUMBER_OF_WINDOWS 3
#define TIME_SYNC_INTERVAL 600
#define VOLUME 100            // %
#define DISPLAY_BRIGHTNESS 50 // %
#define CHARGE_CURRENT 800    // mA
#define PREFERENCE_NAMESPACE "fxwind"
#define MDNSNAME "fxwind"
#define AP_SSID "fxwind Accesspoint"

// structs, enums
struct Settings
{
  int Volume;
  int CalibrationFactor;
  int LowerWindspeedThreshold;
  int UpperWindspeedThreshold;
  int WindspeedEvaluationRange;
  int WindspeedDurationRange;
  int WindspeedNumberOfWindows;
  int DisplayBrightness;
  int MaximumChargeCurrent;
};

// global variables
Preferences preferences;
WiFiManager wifiManager;
M5GFX display;
Settings settings = {VOLUME, 1, WINDSPEED_LOWER_THRESHOLD, WINDSPEED_UPPER_THRESHOLD, WINDSPEED_EVALUATION_RANGE, WINDSPEED_DURATION_RANGE, WINDSPEED_NUMBER_OF_WINDOWS, DISPLAY_BRIGHTNESS, CHARGE_CURRENT};
WindSpeed windSpeed(WINDSPEED_PIN, settings.LowerWindspeedThreshold, settings.UpperWindspeedThreshold, settings.WindspeedDurationRange, settings.WindspeedEvaluationRange, settings.WindspeedNumberOfWindows, settings.CalibrationFactor);
WindSpeedDisplay windSpeedDisplay(settings.LowerWindspeedThreshold, settings.UpperWindspeedThreshold, settings.WindspeedEvaluationRange, settings.WindspeedDurationRange, &windSpeed);
StartupDisplay startupDisplay;
WifiConfigDisplay wifiConfigDisplay;

WiFiUDP Udp;
ESPAsyncHTTPUpdateServer updateServer;
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
bool isWifiOn = false;
int touchDuration = 0;
bool isSwitchoffSoundActive = false;

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

time_t getRtcTime()
{
  // Get the current RTC datetime
  auto dateTime = M5.Rtc.getDateTime();

  // Populate the tm structure
  struct tm t;
  t.tm_year = dateTime.date.year - 1900; // tm_year is years since 1900
  t.tm_mon = dateTime.date.month - 1;    // tm_mon is 0-based (0 = January)
  t.tm_mday = dateTime.date.date;
  t.tm_hour = dateTime.time.hours;
  t.tm_min = dateTime.time.minutes;
  t.tm_sec = dateTime.time.seconds;
  t.tm_isdst = -1; // Daylight saving time (let the system determine)

  // Convert to time_t
  Serial.println("time synched from RTC");
  return mktime(&t);
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
      unsigned long timeValue = secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
      Serial.println("time synched from NTP server");
      return timeValue;
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
  jsonDocument["LowerWindspeedThreshold"] = settings.LowerWindspeedThreshold;
  jsonDocument["UpperWindspeedThreshold"] = settings.UpperWindspeedThreshold;
  jsonDocument["WindspeedDurationRange"] = settings.WindspeedDurationRange;
  jsonDocument["WindspeedEvaluationRange"] = settings.WindspeedEvaluationRange;
  jsonDocument["WindspeedNumberOfWindows"] = settings.WindspeedNumberOfWindows;
  jsonDocument["CalibrationFactor"] = settings.CalibrationFactor;
  jsonDocument["DisplayBrightness"] = settings.DisplayBrightness;
  jsonDocument["MaximumChargeCurrent"] = settings.MaximumChargeCurrent;

  String jsonString;
  jsonDocument.shrinkToFit();
  serializeJson(jsonDocument, jsonString);
  return jsonString;
}

String getTimestampString()
{
  time_t t = now();
  char stringbuffer[100];
  sprintf(stringbuffer, "%4u-%02u-%02u %02u:%02u:%02u", year(t), month(t), day(t), hour(t), minute(t), second(t));
  return String(stringbuffer);
}

String getStatusJson()
{

  JsonDocument jsonDocument;

  jsonDocument["BatteryLevel"] = M5.Power.getBatteryLevel();
  jsonDocument["Current"] = M5.Power.getBatteryCurrent();
  jsonDocument["IsPowerConnected"] = M5.Power.Axp192.isACIN();
  jsonDocument["IsCharging"] = M5.Power.isCharging();
  jsonDocument["WifiIpAddress"] = isAPModeActive ? WiFi.softAPIP() : WiFi.localIP();
  jsonDocument["WifiRSSI"] = WiFi.RSSI();
  jsonDocument["WifiMode"] = isAPModeActive ? "Accesspoint" : "WiFi";
  jsonDocument["WifiSSID"] = isAPModeActive ? AP_SSID : "NONE";
  jsonDocument["WifiHostname"] = String("http://") + MDNSNAME + String(".local");
  jsonDocument["DateTime"] = getTimestampString();
  jsonDocument["FirmwareVersion"] = String(FWVERSION);

  String jsonString;
  jsonDocument.shrinkToFit();
  serializeJson(jsonDocument, jsonString);
  return jsonString;
}

void updateSettings()
{
  windSpeed.updateSettings(settings.LowerWindspeedThreshold, settings.UpperWindspeedThreshold, settings.WindspeedDurationRange, settings.WindspeedEvaluationRange, settings.WindspeedNumberOfWindows, settings.CalibrationFactor);
  windSpeedDisplay.updateSettings(settings.LowerWindspeedThreshold, settings.UpperWindspeedThreshold, settings.WindspeedEvaluationRange, settings.WindspeedDurationRange, settings.DisplayBrightness);
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
    const AsyncWebHeader *h = request->getHeader(i);
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
    const AsyncWebParameter *parameter = request->getParam(0);
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

void playSound(uint32_t duration = 4294967295U, float frequency = 440.0f)
{
  M5.Speaker.tone(frequency, duration);
}

void playSwitchOffSound()
{
  if (!isSwitchoffSoundActive)
  {
    isSwitchoffSoundActive = true;
    int delayTime = 200;
    playSound(delayTime, 1320.0f);
    M5.delay(delayTime);
    playSound(delayTime, 880.0f);
    M5.delay(delayTime);
    playSound(delayTime, 440.0f);
    M5.delay(delayTime);
  }
}

void playSwitchOnSound()
{
  int delayTime = 200;
  playSound(delayTime, 440.0f);
  M5.delay(delayTime);
  playSound(delayTime, 880.0f);
  M5.delay(delayTime);
  playSound(delayTime, 1320.0f);
  M5.delay(delayTime);
}

void playAlarmSound() {
    playSound();
}

void stopSound()
{
  M5.Speaker.stop();
}

void setupSoundModule()
{
  updateVolume();
  M5.Speaker.begin();
}

void setupDns()
{
  if (!MDNS.begin(MDNSNAME))
  {
    Serial.println("Error setting up MDNS responder!");
  }
  else
  {
    Serial.println("mDNS responder started");
  }
}

void wifiManagerConfigModeCallback(WiFiManager *wifiManager)
{
  Serial.println("Entered wifi manager config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(wifiManager->getConfigPortalSSID());
  wifiConfigDisplay.setup();
  wifiConfigDisplay.draw(wifiManager->getConfigPortalSSID(), WiFi.softAPIP().toString());
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
    wifiManager.setAPCallback(wifiManagerConfigModeCallback);
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

void setupRtcTimeSyncProvider()
{
  setSyncProvider(getRtcTime);
  setSyncInterval(TIME_SYNC_INTERVAL);
}

void setupNtpTimeSyncProvider()
{
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(TIME_SYNC_INTERVAL);
  Serial.println("NTP Sync done");
}

void setupRtc()
{

  if (!M5.Rtc.isEnabled())
  {
    Serial.println("RTC not found.");
    return;
  }

  Serial.println("RTC found.");
  auto dt = M5.Rtc.getDateTime();
  Serial.printf("RTC before set   UTC  :%04d/%02d/%02d  %02d:%02d:%02d\r\n", dt.date.year, dt.date.month, dt.date.date, dt.time.hours, dt.time.minutes, dt.time.seconds);

  if (!isWifiOn || isAPModeActive)
  {
    setupRtcTimeSyncProvider();
  }
  else
  {
    setupNtpTimeSyncProvider();
  }

  // update RTC with current system time
  time_t t = now();
  M5.Rtc.setDateTime(gmtime(&t));
}

void evaluationCallback()
{
  isAlarmActive = true;
  playAlarmSound();
}

void startButtonCallback(bool isWifiEnabled, bool isAPEnabled)
{
  isAPModeActive = isAPEnabled;
  isWifiOn = isWifiEnabled;
  isStartupActive = false;
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
  int lowerWindspeedThreshold = bodyJSON["LowerWindspeedThreshold"];
  int upperWindspeedThreshold = bodyJSON["UpperWindspeedThreshold"];
  settings.WindspeedDurationRange = bodyJSON["WindspeedDurationRange"];
  settings.WindspeedNumberOfWindows = bodyJSON["WindspeedNumberOfWindows"];
  // int calibrationValue = bodyJSON["CalibrationValue"];
  int displayBrightness = bodyJSON["DisplayBrightness"];
  int maximumChargeCurrent = bodyJSON["MaximumChargeCurrent"];
  Serial.printf("Volume: %d, LowerWindspeedTreshold: %d, UpperWindspeedTreshold: %d, MaxChargeCurrent: %d, DisplayBrightness: %d\n", volume, lowerWindspeedThreshold, upperWindspeedThreshold, maximumChargeCurrent, displayBrightness);
  settings.Volume = volume;
  settings.LowerWindspeedThreshold = lowerWindspeedThreshold;
  settings.UpperWindspeedThreshold = upperWindspeedThreshold;
  // settings.CalibrationFactor = calibrationValue;
  settings.DisplayBrightness = displayBrightness;
  settings.MaximumChargeCurrent = maximumChargeCurrent;
  updateSettings();
}

void handleSettings(AsyncWebServerRequest *request)
{
  Serial.println("handleSettings");
  request->send_P(200, "application/json", getSettingsJson().c_str());
}

void handleResetWifi(AsyncWebServerRequest *request)
{
  Serial.println("handleResetWifi");
  wifiManager.resetSettings();
  request->send(200, "text/plain", "Resetting WiFi settings");
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
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "application/json", getStatusJson().c_str()); });
  server.on("/settings", HTTP_POST, handleSettings, nullptr, parseMyPageBody);
  server.on("/resetwifi", HTTP_POST, handleResetWifi);

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  updateServer.setup(&server);

  updateServer.onUpdateBegin = [](const UpdateType type, int &result)
  {
    Serial.println("Update started : " + String(type));
  };
  updateServer.onUpdateEnd = [](const UpdateType type, int &result)
  {
    Serial.println("Update finished : " + String(type) + " result: " + String(result));
  };

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
  preferences.putInt("LowerThreshold", settings.LowerWindspeedThreshold);
  preferences.putInt("UpperThreshold", settings.UpperWindspeedThreshold);
  preferences.putInt("DurationRange", settings.WindspeedDurationRange);
  preferences.putInt("EvaluationRange", settings.WindspeedEvaluationRange);
  preferences.putInt("NumberOfWindows", settings.WindspeedNumberOfWindows);
  preferences.putInt("Calibration", settings.CalibrationFactor);
  preferences.putInt("Brightness", settings.DisplayBrightness);
  preferences.putInt("MaxCurrent", settings.MaximumChargeCurrent);
  preferences.end();
  Serial.println("Preferences saved");
}

void setupPreferences()
{
  preferences.begin(PREFERENCE_NAMESPACE, false);
  settings.Volume = preferences.getInt("Volume", VOLUME);
  settings.LowerWindspeedThreshold = preferences.getInt("LowerThreshold", WINDSPEED_LOWER_THRESHOLD);
  settings.UpperWindspeedThreshold = preferences.getInt("UpperThreshold", WINDSPEED_UPPER_THRESHOLD);
  settings.WindspeedDurationRange = preferences.getInt("DurationRange", WINDSPEED_DURATION_RANGE);
  settings.WindspeedEvaluationRange = preferences.getInt("EvaluationRange", WINDSPEED_EVALUATION_RANGE);
  settings.WindspeedNumberOfWindows = preferences.getInt("NumberOfWindows", WINDSPEED_NUMBER_OF_WINDOWS);
  settings.CalibrationFactor = preferences.getInt("Calibration", 1);
  settings.DisplayBrightness = preferences.getInt("Brightness", DISPLAY_BRIGHTNESS);
  settings.MaximumChargeCurrent = preferences.getInt("MaxCurrent", CHARGE_CURRENT);
  Serial.printf("Volume: %d, LowerThreshold: %d, UpperThreshold: %d, DurationRange: %d, EvaluationRange: %d, NumberOfWindows: %d, CalibrationValue: %d, MaxChargeCurrent: %d, DisplayBrightness: %d\n", settings.Volume, settings.UpperWindspeedThreshold, settings.LowerWindspeedThreshold, settings.WindspeedDurationRange, settings.WindspeedEvaluationRange, settings.WindspeedNumberOfWindows, settings.CalibrationFactor, settings.MaximumChargeCurrent, settings.DisplayBrightness);
  preferences.end(); 
  saveSettings();
  updateSettings();
}

void setupStartupLogo()
{
  File pngLogo = LittleFS.open("/FxWindStartLogo.png", "r");
  Serial.println("Logo size: " + String(pngLogo.size()));
  display.begin();
  display.drawPng(&pngLogo, 1, 18);
  delay(3000);
  display.clear();
}

void setupStartupDisplay()
{
  playSwitchOnSound();
  startupDisplay.setup(255);
  startupDisplay.setupStartButtonCallback(&startButtonCallback);
  startupDisplay.draw();
  while (isStartupActive)
  {
    delay(1);
    startupDisplay.evaluateTouches();
  }
}

void setupM5()
{
  auto config = M5.config();
  config.external_speaker.atomic_spk = true;
  config.external_speaker.module_display = true;
  config.external_rtc = true;
  M5.begin();
}

void switchOffWifi()
{
  Serial.println("Stop Wifi");
  esp_err_t results = esp_wifi_stop();
}

void switchOnWifi()
{
  Serial.println("Start Wifi");
  esp_err_t results = esp_wifi_start();
  if (!isAPModeActive)
  {
    WiFi.reconnect();
  }
}

void setup(void)
{
  // order of initialization is important
  setupM5();
  windSpeed.setup();
  setupWindspeedIO();
  setupSoundModule();
  setupLittleFS();
  setupStartupDisplay();
  setupWifi();
  setupRtc();
  setupServer();
  windSpeedDisplay.setup();
  setupPreferences();
  if (!isWifiOn)
  {
    switchOffWifi();
  }
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
          if (menuX == 4)
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
          if (menuX == 0)
          {
            menuX = 4;
          }
          else
          {
            menuX--;
          }
        }
        windSpeedDisplay.draw((DrawType)menuX);
      }
    }

    if (touchDetail.wasPressed())
    {
      Serial.println("Touch pressed... start timer");
      touchDuration = millis();
    }

    if (touchDetail.wasReleased())
    {
      int duration = millis() - touchDuration;
      Serial.println("Touch released... Duration: " + String(duration));
      touchDuration = 0;
      if (duration > 3000)
      {
        startDeepSleep();
      }
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
  }
}

void loop(void)
{
  M5.delay(1);
  M5.update();
  evaluateTouches();

  if (touchDuration > 0 && (millis() - touchDuration) > 3000)
  {
    playSwitchOffSound();
  }

  long currentMillis = millis();
  if (currentMillis - lastMillis >= SAMPLE_RATE)
  {
    windSpeed.calculateWindspeed(true, true);
    lastMillis = currentMillis;
    windSpeedDisplay.draw((DrawType)menuX);
    Serial.printf("Level: %d, Voltage: %d, Current: %d, IsCharging:%d, ChargeCurrent: %.2f, isACin: %d \n", M5.Power.getBatteryLevel(), M5.Power.getBatteryVoltage(), M5.Power.getBatteryCurrent(), M5.Power.isCharging(), M5.Power.Axp192.getBatteryChargeCurrent(), M5.Power.Axp192.isACIN());
  }
}
