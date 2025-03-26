
#include <SD.h>
#include <M5GFX.h>
#include <M5Unified.h>
#include <WiFiManager.h>
#include <TimeLib.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "WindSpeed.h"
#include "WindSpeedDisplay.h"


// constants
#define WINDSPEED_PIN 19
#define EVALUATION_RANGE 300
#define SAMPLE_RATE 1000            // ms
#define WINDSPEED_THRESHOLD 8       // m/s
#define WINDSPEED_DURATION_RANGE 20 // samples
#define NTP_SYNC_INTERVAL 600


// global variables
WiFiManager wifiManager;
WindSpeed windSpeed(WINDSPEED_PIN, EVALUATION_RANGE, WINDSPEED_THRESHOLD, WINDSPEED_DURATION_RANGE);
WindSpeedDisplay windSpeedDisplay(EVALUATION_RANGE, WINDSPEED_THRESHOLD, WINDSPEED_DURATION_RANGE, &windSpeed);

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

static constexpr const char *menu_x_items[4] = {"Combined", "Plot", "Number", "Stats"};

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

String getDownloadFilesJson() {

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
        arrayDocument["filename"] = String(file.name());
        arrayDocument["filesize"] = fsize;
        arrayDocument["downloadUrl"] = "./downloads?filename=" + String(file.name());
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

void handleDownloadRequest(AsyncWebServerRequest *request)
{

  Serial.print("Method: ");
  Serial.println(request->method());

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
    // AsyncWebServerResponse *response = request->beginResponse(SD, "/logs/" + filename, String(), true);
    request->send(SD, "/logs/" + filename, String(), true);
    return;
  }
  else
  {
    request->send(200, "text/html", printDirectory());
  }
}

void playSound(int duration = 2000) {
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
  M5.Speaker.setVolume(128);
  M5.Speaker.begin();
}

void setupWifi()
{
  WiFi.mode(WIFI_STA);

  wifiManager.setHostname(hostname);
  bool res;

  //wifiManager.resetSettings();

  wifiManager.autoConnect("F3XWind");

  if (!res) {
    Serial.println("Failed to connect");
  } else {
    Serial.println("Connected successfull");
  }

  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
}

void setupNtpTimeSyncProvider()
{
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(NTP_SYNC_INTERVAL);
}

void evaluationCallback() {
    playSound(5000);
}

void setupWindspeedIO()
{
  windSpeed.setupInterruptCallback(interruptCallback);
  windSpeed.setupEvaluationCallback(&evaluationCallback);
}

void setupServer()
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/index.html"); });
  server.on("/windspeed", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "application/json", windSpeed.getWindspeedJson().c_str()); });
  server.on("/statistic", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "application/json", windSpeed.getWindspeedEvaluationJson().c_str()); });
  server.on("/downloadtest", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SD, "/logs/2024-09-27_windspeed.csv", String(), true); });
  server.on("/downloads", HTTP_GET, [](AsyncWebServerRequest *request)
            { handleDownloadRequest(request);});
  server.on("/downloadsjson", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "application/json", getDownloadFilesJson().c_str()); });
  // server.on("/downloads", HTTP_GET, [](AsyncWebServerRequest *request)
  //           { request->send(200, "text/html", printDirectory()); });

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



void setup(void)
{
  M5.begin();
  windSpeed.setup();
  setupWindspeedIO();
  setupWifi();
  setupNtpTimeSyncProvider();
  setupLittleFS();
  setupServer();
  setupSoundModule();
  windSpeedDisplay.setup();
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
      }
    }
    if (touchDetail.wasHold())
    {
      stopSound();
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
  }
}
