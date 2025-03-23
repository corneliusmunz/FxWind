
#include <SD.h>
#include <M5GFX.h>
#include <M5Unified.h>
#include <WiFiManager.h>
#include <TimeLib.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "WindSpeed.h"


// constants
#define WINDSPEED_PIN 21
#define EVALUATION_RANGE 300
#define SAMPLE_RATE 1000            // ms
#define WINDSPEED_THRESHOLD 8       // m/s
#define WINDSPEED_DURATION_RANGE 20 // samples


#define PLOT_OFFSET_X 20
#define PLOT_OFFSET_Y 5
#define PLOT_HEIGHT 100
#define EVALUATION_BAR_HEIGHT 20
#define BUTTON_HEIGHT 16
#define TXT_DEFAULT_COLOR TFT_WHITE
#define TXT_ALERT_COLOR TFT_RED
#define TXT_DEFAULT_BACKGROUND_COLOR TFT_BLACK
#define TXT_ALERT_BACKGROUND_COLOR TFT_RED
#define TXT_BUTTON_PRESSED_COLOR TFT_NAVY
#define RECTANGLE_BUTTON_DEFAULT_COLOR TFT_WHITE
#define RECTANGLE_BUTTON_PRESSED_COLOR TFT_NAVY
#define GRID_COLOR TFT_DARKGREY
#define PLOT_BAR_DEFAULT_COLOR TFT_GREEN
#define PLOT_BAR_ALERT_COLOR TFT_RED
#define NTP_SYNC_INTERVAL 600

// global variables
WindSpeed windSpeed(WINDSPEED_PIN, EVALUATION_RANGE, WINDSPEED_THRESHOLD, WINDSPEED_DURATION_RANGE);
M5GFX display;
WiFiUDP Udp;
AsyncWebServer server(80);
unsigned long lastMillis;
static const char *hostname = "f3xwind";
static const char ntpServerName[] = "de.pool.ntp.org";
unsigned int localPort = 8888;
const int timeZone = 0;
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];



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

void drawMenuButton(String label, int xPos, bool isPressed = false)
{
  display.setFont(&fonts::DejaVu12);
  int yPos = display.height() - BUTTON_HEIGHT;
  if (isPressed)
  {
    display.setColor(RECTANGLE_BUTTON_PRESSED_COLOR);
    display.setTextColor(TXT_BUTTON_PRESSED_COLOR, TXT_DEFAULT_BACKGROUND_COLOR);
  }
  else
  {
    display.setColor(RECTANGLE_BUTTON_DEFAULT_COLOR);
    display.setTextColor(TXT_DEFAULT_COLOR, TXT_DEFAULT_BACKGROUND_COLOR);
  }
  display.drawRect(xPos, yPos, 64, BUTTON_HEIGHT + 1);
  display.drawCenterString(label, xPos + 33, yPos + 5);
}

void drawMenuButtons()
{

  if (M5.BtnA.isHolding())
  {
    drawMenuButton("ABCD", 31, true); // 64
    if (!M5.Speaker.isPlaying())
    {
      M5.Speaker.tone(1000, 500);
    }
  }
  else
  {
    drawMenuButton("ABCD", 31, false);
  }

  if (M5.BtnB.isPressed())
  {
    drawMenuButton("EFGH", 127, true); // 160
    M5.Speaker.tone(500, 100);
  }
  else
  {
    drawMenuButton("EFGH", 127, false);
  }

  if (M5.BtnC.wasPressed())
  {
    drawMenuButton("IJKL", 223, true); // 256
    M5.Speaker.tone(2000, 100);
  }
  else
  {
    drawMenuButton("IJKL", 223, false);
  }
}

void drawWindspeedDisplayValues(float windspeed, WindspeedEvaluation windspeedEvaluation)
{
  int yPos = PLOT_OFFSET_Y + EVALUATION_BAR_HEIGHT + PLOT_HEIGHT + 12;
  display.setFont(&fonts::DejaVu72);
  int bigFontHeight = display.fontHeight();
  if (windspeed > WINDSPEED_THRESHOLD)
  {
    display.setTextColor(TXT_DEFAULT_COLOR, TXT_ALERT_BACKGROUND_COLOR);
  }
  else
  {
    display.setTextColor(TXT_DEFAULT_COLOR, TXT_DEFAULT_BACKGROUND_COLOR);
  }
  display.drawString(windSpeed.getWindspeedString(true), 1, yPos);
  display.setFont(&fonts::DejaVu18);
  display.setTextColor(TXT_DEFAULT_COLOR, TXT_DEFAULT_BACKGROUND_COLOR);
  String evaluationString = windSpeed.getWindspeedEvaluationString();
  display.drawString(evaluationString, 24, yPos + bigFontHeight + 6);
}

void drawGrid()
{
  display.setFont(&fonts::DejaVu12);
  display.setTextColor(TXT_DEFAULT_COLOR, TXT_DEFAULT_BACKGROUND_COLOR);
  int fontOffsetY = (int)(display.fontHeight() / 2.0f);
  for (size_t i = 0; i <= 10; i += 2)
  {
    int fontOffsetX = i < 10 ? display.fontWidth() : 2 * display.fontWidth();
    display.drawString(String(i), PLOT_OFFSET_X - fontOffsetX - 10, PLOT_OFFSET_Y - fontOffsetY + PLOT_HEIGHT - i * 10);
  }

  for (size_t i = 0; i <= 10; i += 1)
  {
    for (size_t j = 0; j < 1; j += 10)
    {
      display.drawFastHLine(PLOT_OFFSET_X - 4 + j, PLOT_OFFSET_Y + PLOT_HEIGHT - i * 10, 4, GRID_COLOR);
    }
  }

  // top and bottom line
  // display.drawLine(PLOT_OFFSET_X, PLOT_OFFSET_Y - 1, PLOT_OFFSET_X + EVALUATION_RANGE, PLOT_OFFSET_Y - 1, GRID_COLOR);
  // display.drawLine(PLOT_OFFSET_X, PLOT_HEIGHT+PLOT_OFFSET_Y+1, PLOT_OFFSET_X + EVALUATION_RANGE, PLOT_HEIGHT+PLOT_OFFSET_Y+1, GRID_COLOR);
}

void drawWindspeedDisplayBarplot()
{
  int h = PLOT_HEIGHT;

  drawGrid();

  for (int x = 0; x < EVALUATION_RANGE; x++)
  {
    int xpos = PLOT_OFFSET_X + EVALUATION_RANGE - x - 1;

    display.drawFastVLine(xpos, PLOT_OFFSET_Y, PLOT_HEIGHT, TFT_BLACK);
    if (windSpeed.getWindSpeedHistoryArrayElement(x) >= WINDSPEED_THRESHOLD * 10)
    {
      display.drawFastVLine(xpos, PLOT_OFFSET_Y + PLOT_HEIGHT - min(windSpeed.getWindSpeedHistoryArrayElement(x), 100), min(windSpeed.getWindSpeedHistoryArrayElement(x), 100), PLOT_BAR_ALERT_COLOR);
    }
    else
    {
      display.drawFastVLine(xpos, PLOT_OFFSET_Y + PLOT_HEIGHT - min(windSpeed.getWindSpeedHistoryArrayElement(x), 100), min(windSpeed.getWindSpeedHistoryArrayElement(x), 100), PLOT_BAR_DEFAULT_COLOR);
    }
  }
}

void drawWindspeedEvaluationBars(WindspeedEvaluation windspeedEvaluation)
{

  display.setFont(&fonts::DejaVu12);
  display.setTextColor(TXT_DEFAULT_COLOR, TXT_ALERT_BACKGROUND_COLOR);
  int y = PLOT_OFFSET_Y + PLOT_HEIGHT + 3;
  display.fillRect(PLOT_OFFSET_X - 1, y, EVALUATION_RANGE, EVALUATION_BAR_HEIGHT, TFT_GREEN);
  for (size_t i = 0; i < windspeedEvaluation.NumberOfExceededRanges; i++)
  {
    int x = PLOT_OFFSET_X + EVALUATION_RANGE - windspeedEvaluation.RangeStartIndex[i] - WINDSPEED_DURATION_RANGE;
    display.fillRect(x, y, WINDSPEED_DURATION_RANGE, EVALUATION_BAR_HEIGHT, TFT_RED);
    display.drawString(String(i + 1), x + 7, y+4);
  }
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


void setupSoundModule()
{
  auto cfg = M5.config();
  cfg.external_speaker.atomic_spk = true;
  cfg.external_speaker.module_display = true;
  M5.Speaker.setVolume(128);
  M5.Speaker.begin();
}

void setupWifi()
{
  WiFi.mode(WIFI_STA);

  WiFiManager wm;
  wm.setHostname(hostname);
  bool res;

  //wm.resetSettings();

  wm.autoConnect("F3XWind");

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

void setupWindspeedIO()
{
  windSpeed.setupInterruptCallback(interruptCallback);
}

void setupDisplay()
{
  display.init();
  display.startWrite();
  display.fillScreen(TFT_BLACK);
  display.endWrite();

  if (display.isEPD())
  {
    display.setEpdMode(epd_mode_t::epd_fastest);
  }
  if (display.width() < display.height())
  {
    display.setRotation(display.getRotation() ^ 1);
  }
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
    AsyncWebParameter* parameter = request->getParam(0);
    Serial.println(parameter->name());
    Serial.println(parameter->value());
    String filename = request->getParam("filename")->value();
    Serial.println("Download Filename: " + filename);
    //AsyncWebServerResponse *response = request->beginResponse(SD, "/logs/" + filename, String(), true);
    request->send(SD, "/logs/" + filename, String(), true);
    return;
  } else {
    request->send(200, "text/html", printDirectory());
  }
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
  windSpeed.setupSDCard();
  setupWindspeedIO();
  setupWifi();
  setupNtpTimeSyncProvider();
  setupLittleFS();
  setupServer();
  setupSoundModule();
  setupDisplay();
}

void loop(void)
{
  long currentMillis = millis();
  if (currentMillis - lastMillis >= SAMPLE_RATE)
  {
    M5.update();
    windSpeed.calculateWindspeed(true, true);
    lastMillis = currentMillis;
    display.waitDisplay();
    drawWindspeedDisplayValues(windSpeed.getCurrentWindspeed(), windSpeed.getWindspeedEvaluation());
    drawWindspeedDisplayBarplot();
    drawWindspeedEvaluationBars(windSpeed.getWindspeedEvaluation());
    //drawMenuButtons();
    display.display();

  }
}