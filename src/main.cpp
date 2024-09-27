
#include <SD.h>
#include <M5GFX.h>
#include <M5Unified.h>
#include <TimeLib.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

M5GFX display;
WiFiUDP Udp;
AsyncWebServer server(80);

// constants
#define WINDSPEED_PIN 21
#define EVALUATION_RANGE 300 
#define SAMPLE_RATE 1000 // ms
#define WINDSPEED_THRESHOLD 8 // m/s
#define WINDSPEED_DURATION_RANGE 20 // samples
#define PLOT_OFFSET_X 20
#define PLOT_OFFSET_Y 5
#define PLOT_HEIGHT 100
#define EVALUATION_BAR_HEIGHT 10
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
static int windspeedHistoryArray[EVALUATION_RANGE];

// global variables
long counter;
long lastCounter;
unsigned long lastMillis;
const char ssid[] = "";
const char pass[] = "";
static const char ntpServerName[] = "de.pool.ntp.org";
unsigned int localPort = 8888;
const int timeZone = 0;
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

struct WindspeedEvaluation
{
  float MaxWindspeed;
  float MinWindspeed;
  float AverageWindspeed;
  int NumberOfExceededRanges;
  int RangeStartIndex[15];
};



String getWindspeedString(float windspeedValue, bool addUnitSymbol = false)
{
  char stringbuffer[100];
  if (addUnitSymbol)
  {
    sprintf(stringbuffer, "%.1f m/s  ", windspeedValue);
  }
  else
  {
    sprintf(stringbuffer, "%.1f", windspeedValue);
  }
  return String(stringbuffer);
}

String getWindspeedEvaluationSingleString(float windspeedValue)
{
  char stringbuffer[100];
  sprintf(stringbuffer, "%.1f", windspeedValue);
  return String(stringbuffer);
}

String getWindspeedEvaluationString(WindspeedEvaluation windspeedEvaluation)
{
  return "MAX:" + getWindspeedEvaluationSingleString(windspeedEvaluation.MaxWindspeed) + " MIN:" + getWindspeedEvaluationSingleString(windspeedEvaluation.MinWindspeed) + " AVG:" + getWindspeedEvaluationSingleString(windspeedEvaluation.AverageWindspeed);
}

String getTimestampString()
{
  time_t t = now();
  char stringbuffer[100];
  sprintf(stringbuffer, "%4u-%02u-%02u %02u:%02u:%02u", year(t), month(t), day(t), hour(t), minute(t), second(t)); 
  return String(stringbuffer);
}

String getLogCsvRow(float windspeedValue, char separationChar = ',')
{
  return getTimestampString() + separationChar + getWindspeedString(windspeedValue);
}

String getLogFilePath() {
  time_t t = now();
  char stringbuffer[100];
  sprintf(stringbuffer, "/logs/%4u-%02u-%02u_windspeed.csv", year(t), month(t), day(t)); 
  return String(stringbuffer);
}

String getLogFileHeader() {
  return "Timestamp, Windspeed[m/s]";
}

// interrupt callback function for impuls counter of windspeed sensor
void incrementCounter()
{
  counter++;
  // ToDo: handle overflow
}

// windspeed in m/s
// 20 impulses in one turn per second ==> 1.75m/s
float calculateWindspeed(int deltaCounter)
{
  return ((float)deltaCounter / 20.0f * 1.75f * 1000 / SAMPLE_RATE);
}

void updateWindspeedArray(float windspeed)
{
  for (size_t i = EVALUATION_RANGE; i > 0; --i)
  {
    windspeedHistoryArray[i] = windspeedHistoryArray[i - 1];
  }
  windspeedHistoryArray[0] = (int)(windspeed * 10.0f);
}

WindspeedEvaluation evaluateWindspeed()
{

  int maxWindspeed = 0;
  int minWindspeed = INT_MAX;
  long sumWindspeed = 0;

  int rangeCounter=0;
  int exceededRangesCounter = 0;
  int exceededRangesIndex[15];

 
  for (size_t i = 0; i < 15; i++)
  {
    //evaluationResult.RangeStartIndex[i]=0;
    exceededRangesIndex[i]=0;
  }
  

  for (size_t i = EVALUATION_RANGE; i > 0; --i)
  {
    if (windspeedHistoryArray[i] > maxWindspeed)
    {
      maxWindspeed = windspeedHistoryArray[i];
    }

    if (windspeedHistoryArray[i] < minWindspeed)
    {
      minWindspeed = windspeedHistoryArray[i];
    }
    sumWindspeed += windspeedHistoryArray[i];

    if (windspeedHistoryArray[i] > WINDSPEED_THRESHOLD*10) 
    {
      rangeCounter++;
    }

    if (rangeCounter == WINDSPEED_DURATION_RANGE)
    {
      exceededRangesIndex[exceededRangesCounter] = i;
      exceededRangesCounter++;
      rangeCounter=0;
    }

    if (windspeedHistoryArray[i] <= WINDSPEED_THRESHOLD * 10)
    {
      rangeCounter = 0;
    }
  }

  WindspeedEvaluation evaluationResult;
  evaluationResult.NumberOfExceededRanges = exceededRangesCounter;
  evaluationResult.MaxWindspeed = (float)maxWindspeed / 10.0f;
  evaluationResult.MinWindspeed = (float)minWindspeed / 10.0f;
  evaluationResult.AverageWindspeed = (float)(sumWindspeed / EVALUATION_RANGE) / 10.0f;
  for (size_t i = 0; i < 15; i++)
  {
    evaluationResult.RangeStartIndex[i]=exceededRangesIndex[i];
  }

  return evaluationResult;
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
    drawMenuButton("ABCD", 31, true); //64
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
    drawMenuButton("IJKL", 223, true); //256
    M5.Speaker.tone(2000, 100);
  }
  else
  {
    drawMenuButton("IJKL", 223, false);
  }
}

void drawWindspeedDisplayValues(float windspeed, WindspeedEvaluation windspeedEvaluation)
{
  int yPos = PLOT_OFFSET_Y + EVALUATION_BAR_HEIGHT + PLOT_HEIGHT + 8;
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
  display.drawString(getWindspeedString(windspeed, true), 1, yPos);
  display.setFont(&fonts::DejaVu18);
  display.setTextColor(TXT_DEFAULT_COLOR, TXT_DEFAULT_BACKGROUND_COLOR);
  String evaluationString = getWindspeedEvaluationString(windspeedEvaluation);
  display.drawString(evaluationString, 24, yPos + bigFontHeight + 2);
}

void drawGrid() {
  display.setFont(&fonts::DejaVu12);
  display.setTextColor(TXT_DEFAULT_COLOR, TXT_DEFAULT_BACKGROUND_COLOR);
  int fontOffsetY = (int)(display.fontHeight()/2.0f);
  for (size_t i = 0; i <= 10; i+=2)
  {
    int fontOffsetX = i < 10 ? display.fontWidth() : 2 * display.fontWidth();
    display.drawString(String(i), PLOT_OFFSET_X - fontOffsetX - 10, PLOT_OFFSET_Y - fontOffsetY + PLOT_HEIGHT - i * 10 );
  }

  for (size_t i = 0; i <= 10; i+=1)
  {
    for (size_t j = 0; j < 1; j+=10)
    {
      display.drawFastHLine(PLOT_OFFSET_X - 4 + j, PLOT_OFFSET_Y + PLOT_HEIGHT - i * 10, 4, GRID_COLOR);
    }
  }
  
  // top and bottom line
  //display.drawLine(PLOT_OFFSET_X, PLOT_OFFSET_Y - 1, PLOT_OFFSET_X + EVALUATION_RANGE, PLOT_OFFSET_Y - 1, GRID_COLOR);
  //display.drawLine(PLOT_OFFSET_X, PLOT_HEIGHT+PLOT_OFFSET_Y+1, PLOT_OFFSET_X + EVALUATION_RANGE, PLOT_HEIGHT+PLOT_OFFSET_Y+1, GRID_COLOR);
} 

void drawWindspeedDisplayBarplot()
{
  int h = PLOT_HEIGHT;

  drawGrid();

  for (int x = 0; x < EVALUATION_RANGE; x++)
  {
    int xpos = PLOT_OFFSET_X + EVALUATION_RANGE - x - 1;

    display.drawFastVLine(xpos, PLOT_OFFSET_Y, PLOT_HEIGHT, TFT_BLACK);
    if (windspeedHistoryArray[x] >= WINDSPEED_THRESHOLD*10)
    {
      display.drawFastVLine(xpos, PLOT_OFFSET_Y + PLOT_HEIGHT - min(windspeedHistoryArray[x], 100), min(windspeedHistoryArray[x], 100), PLOT_BAR_ALERT_COLOR);
    }
    else
    {
      display.drawFastVLine(xpos, PLOT_OFFSET_Y + PLOT_HEIGHT - min(windspeedHistoryArray[x], 100), min(windspeedHistoryArray[x], 100), PLOT_BAR_DEFAULT_COLOR);
    }
  }
}

void drawWindspeedEvaluationBars(WindspeedEvaluation windspeedEvaluation) {

  display.setFont(&fonts::DejaVu9);
  display.setTextColor(TXT_DEFAULT_COLOR, TXT_ALERT_BACKGROUND_COLOR);
  int y = PLOT_OFFSET_Y + PLOT_HEIGHT + 3;
  display.fillRect(PLOT_OFFSET_X - 1, y, EVALUATION_RANGE, EVALUATION_BAR_HEIGHT, TFT_GREEN);
  for (size_t i = 0; i < windspeedEvaluation.NumberOfExceededRanges; i++)
  {
    int x = PLOT_OFFSET_X + EVALUATION_RANGE - windspeedEvaluation.RangeStartIndex[i] - WINDSPEED_DURATION_RANGE;
    display.fillRect(x, y, WINDSPEED_DURATION_RANGE, EVALUATION_BAR_HEIGHT, TFT_RED);
    display.drawString(String(i+1), x+7, y);
  }
}

String getWindspeedStatisticJson()
{
  WindspeedEvaluation windspeedEvaluation = evaluateWindspeed();

  JsonDocument jsonDocument;

  jsonDocument["currentWindspeed"] = windspeedHistoryArray[0];
  jsonDocument["minWindspeed"] = windspeedEvaluation.MinWindspeed;
  jsonDocument["maxWindspeed"] = windspeedEvaluation.MaxWindspeed;
  jsonDocument["averageWindspeed"] = windspeedEvaluation.AverageWindspeed;
  jsonDocument["numberOfExceededRanges"] = windspeedEvaluation.NumberOfExceededRanges;

  JsonArray evaluationArray = jsonDocument["evaluationArray"].to<JsonArray>();

  // if (windspeedEvaluation.NumberOfExceededRanges > 0)
  // for (size_t i = 0; i < windspeedEvaluation.NumberOfExceededRanges; i++)
  // {
  //   /* code */
  // }

  evaluationArray.add(0);
  evaluationArray.add(0);
  evaluationArray.add(0);
  evaluationArray.add(1);
  evaluationArray.add(1);
  evaluationArray.add(1);
  evaluationArray.add(0);
  evaluationArray.add(0);

  String jsonString;
  jsonDocument.shrinkToFit();
  serializeJson(jsonDocument, jsonString);
  return jsonString;
}

String getWindspeedJson()
{
  JsonDocument jsonDocument;

  for (size_t i = 0; i < EVALUATION_RANGE; i++)
  {
    JsonObject arrayDocument = jsonDocument.add<JsonObject>();
    arrayDocument["x"] = i;
    arrayDocument["y"] = windspeedHistoryArray[EVALUATION_RANGE - 1 - i] / 10.0f;
  }

  String jsonString;
  jsonDocument.shrinkToFit();
  serializeJson(jsonDocument, jsonString);

  return jsonString;
}

void createDir(fs::FS &fs, const char *path)
{
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path))
  {
    Serial.println("Dir created");
  }
  else
  {
    Serial.println("mkdir failed");
  }
}

void readFile(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while (file.available())
  {
    Serial.write(file.read());
  }
  file.close();
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("File written");
  }
  else
  {
    Serial.println("Write failed");
  }
  file.close();
}

void writeLineToFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.println(message))
  {
    Serial.println("File written");
  }
  else
  {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file)
  {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message))
  {
    Serial.println("Message appended");
  }
  else
  {
    Serial.println("Append failed");
  }
  file.close();
}

void appendLineToFile(fs::FS &fs, const char *path, const char *message)
{

  File file = fs.open(path, FILE_APPEND);
  if (!file)
  {
    Serial.println("Failed to open file for appending");
    writeLineToFile(fs, path, getLogFileHeader().c_str());
    return;
  }
  if (!file.println(message))
  {
    Serial.println("Append failed");
  }
  file.close();
}

void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root)
  {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file)
  {
    if (file.isDirectory())
    {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels)
      {
        listDir(fs, file.name(), levels - 1);
      }
    }
    else
    {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
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
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
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
  setSyncInterval(300);
}

void setupWindspeedIO()
{
  pinMode(WINDSPEED_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(WINDSPEED_PIN), incrementCounter, RISING);
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

void setupServer() {

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/index.html"); });
  server.on("/windspeed", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "application/json", getWindspeedJson().c_str()); });
  server.on("/statistic", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "application/json", getWindspeedStatisticJson().c_str()); });

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  server.begin();
}

void setupLittleFS() {
  if (!LittleFS.begin())
  {
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }
}

void setupSDCard() {
  if (!SD.begin(GPIO_NUM_4, SPI, 25000000))
  {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE)
  {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC)
  {
    Serial.println("MMC");
  }
  else if (cardType == CARD_SD)
  {
    Serial.println("SDSC");
  }
  else if (cardType == CARD_SDHC)
  {
    Serial.println("SDHC");
  }
  else
  {
    Serial.println("UNKNOWN");
  }
  Serial.printf("SD Card Size: %lluMB\n", SD.cardSize() / (1024 * 1024));
  Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
  Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));

  createDir(SD,"/logs");
  listDir(SD, "/logs", 0);
}


void setup(void)
{
  M5.begin();
  setupSDCard();
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
    float windspeed = calculateWindspeed(counter - lastCounter);
    lastMillis = currentMillis;
    lastCounter = counter;
    updateWindspeedArray(windspeed);
    WindspeedEvaluation evaluationResult = evaluateWindspeed();
    appendLineToFile(SD, getLogFilePath().c_str(), getLogCsvRow(windspeed).c_str());

    display.waitDisplay();
    drawWindspeedDisplayValues(windspeed, evaluationResult);
    drawWindspeedDisplayBarplot();
    drawWindspeedEvaluationBars(evaluationResult);
    drawMenuButtons();
    display.display();
  }
}