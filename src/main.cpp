
#include <SD.h>
#include <M5GFX.h>
#include <M5Unified.h>
#include <TimeLib.h>
#include <WiFi.h>
#include <WiFiUdp.h>

M5GFX display;
WiFiUDP Udp;

// constants
#define WINDSPEED_PIN 21
#define EVALUATION_RANGE 300 
#define SAMPLE_RATE 1000 // ms
#define WINDSPEED_THRESHOLD 8 // m/s
#define PLOT_OFFSET_X 20
#define PLOT_OFFSET_Y 5
#define PLOT_HEIGHT 100
#define EVALUATION_BAR_HEIGHT 10
#define BUTTON_HEIGHT 16
#define TXT_DEFAULT_COLOR TFT_WHITE
#define TXT_DEFAULT_BACKGROUND_COLOR TFT_BLACK
#define TXT_ALERT_BACKGROUND_COLOR TFT_RED
#define TXT_BUTTON_PRESSED_COLOR TFT_NAVY
#define RECTANGLE_BUTTON_DEFAULT_COLOR TFT_WHITE
#define RECTANGLE_BUTTON_PRESSED_COLOR TFT_NAVY
#define GRID_COLOR TFT_DARKGREY
#define PLOT_BAR_DEFAULT_COLOR TFT_GREEN
#define PLOT_BAR_ALERT_COLOR TFT_RED
static int windspeedHistoryArray[EVALUATION_RANGE];
static int y[EVALUATION_RANGE];


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
  return "MAX:" + getWindspeedEvaluationSingleString(windspeedEvaluation.MaxWindspeed) + " MIN:" + getWindspeedEvaluationSingleString(windspeedEvaluation.MinWindspeed) + " AVRG:" + getWindspeedEvaluationSingleString(windspeedEvaluation.AverageWindspeed);
}

String getTimestampString()
{
  time_t t = now();
  char stringbuffer[100];
  sprintf(stringbuffer, "%4u-%02u-%02u %02u:%02u:%02u", year(t), month(t), day(t), hour(t), minute(t), second(t)); // String(year(t)) + "-" + String(month(t)) + "-" + String(day(t)) + " " + String(hour(t)) + ":" + String(minute(t)) + ":" + String(second(t));
  return String(stringbuffer);
}

String getLogCsvRow(float windspeedValue, char separationChar = ',')
{
  return getTimestampString() + separationChar + getWindspeedString(windspeedValue);
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
  for (size_t i = EVALUATION_RANGE; i > 0; i--)
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
  

  for (size_t i = EVALUATION_RANGE; i > 0; i--)
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

    if (rangeCounter >= 20)
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
      display.drawLine(PLOT_OFFSET_X - 4 + j, PLOT_OFFSET_Y + PLOT_HEIGHT - i * 10, PLOT_OFFSET_X + j, PLOT_OFFSET_Y + PLOT_HEIGHT - i * 10, GRID_COLOR);
    }
  }
  
  // top and bottom line
  //display.drawLine(PLOT_OFFSET_X, PLOT_OFFSET_Y - 1, PLOT_OFFSET_X + EVALUATION_RANGE, PLOT_OFFSET_Y - 1, GRID_COLOR);
  //display.drawLine(PLOT_OFFSET_X, PLOT_HEIGHT+PLOT_OFFSET_Y+1, PLOT_OFFSET_X + EVALUATION_RANGE, PLOT_HEIGHT+PLOT_OFFSET_Y+1, GRID_COLOR);
} 

void drawWindspeedDisplayBarplot()
{
  int h = PLOT_HEIGHT;
  int barWidth = 1;

  drawGrid();

  for (int x = 0; x < EVALUATION_RANGE; x++)
  {
    int xpos = PLOT_OFFSET_X + EVALUATION_RANGE - x;

    display.fillRect(xpos, PLOT_OFFSET_Y, barWidth, PLOT_HEIGHT, TFT_BLACK); // erase drawing
    if (windspeedHistoryArray[x] >= WINDSPEED_THRESHOLD*10)
    {
      display.fillRect(xpos, PLOT_OFFSET_Y + PLOT_HEIGHT - min(windspeedHistoryArray[x], 100), barWidth, min(windspeedHistoryArray[x], 100), PLOT_BAR_ALERT_COLOR);
    }
    else
    {
      display.fillRect(xpos, PLOT_OFFSET_Y + PLOT_HEIGHT - min(windspeedHistoryArray[x], 100), barWidth, min(windspeedHistoryArray[x], 100), PLOT_BAR_DEFAULT_COLOR);
    }
  }
}

void drawWindspeedEvaluationBars(WindspeedEvaluation windspeedEvaluation) {

  display.setFont(&fonts::DejaVu9);
  display.setTextColor(TXT_DEFAULT_COLOR, TXT_ALERT_BACKGROUND_COLOR);
  int y = PLOT_HEIGHT + PLOT_OFFSET_Y + 3;
  display.fillRect(PLOT_OFFSET_X, y, EVALUATION_RANGE, EVALUATION_BAR_HEIGHT, TFT_GREEN);
  for (size_t i = 0; i < windspeedEvaluation.NumberOfExceededRanges; i++)
  {
    int x = PLOT_OFFSET_X + EVALUATION_RANGE - windspeedEvaluation.RangeStartIndex[i] - 20;

    int width = min(20, x - 20);
    if (i==0) {
      Serial.print("index: ");
      Serial.print(windspeedEvaluation.RangeStartIndex[i], DEC);
      Serial.print(" x: ");
      Serial.print(x, DEC);
      Serial.print(" width:");
      Serial.println(width, DEC);
    }

    display.fillRect(x, y, width, EVALUATION_BAR_HEIGHT, TFT_RED);
    display.drawString(String(i+1), x+7, y);
  }
}

void setupSoundModule()
{
  auto cfg = M5.config();
  cfg.external_speaker.atomic_spk = true;
  cfg.external_speaker.module_display = true;
  M5.Speaker.setVolume(200);
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

  if (display.isEPD())
  {
    display.setEpdMode(epd_mode_t::epd_fastest);
  }
  if (display.width() < display.height())
  {
    display.setRotation(display.getRotation() ^ 1);
  }
}

void setup(void)
{
  M5.begin();

  setupWindspeedIO();
  setupWifi();
  setupNtpTimeSyncProvider();
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
    updateWindspeedArray(windspeed);
    WindspeedEvaluation evaluationResult = evaluateWindspeed();

    // if (evaluationResult.NumberOfExceededRanges > 0) {
    //   Serial.print("EvaluationRanges:");
    //   Serial.print(evaluationResult.NumberOfExceededRanges, DEC);
    //   Serial.print(" StartingIndexes:");
    //   for (size_t i = 0; i < evaluationResult.NumberOfExceededRanges; i++)
    //   {
    //     Serial.print(evaluationResult.RangeStartIndex[i]);
    //     Serial.print(",");
    //   }
    //   Serial.println();
    // }

    
    lastMillis = currentMillis;
    lastCounter = counter;

    display.waitDisplay();
    drawWindspeedDisplayValues(windspeed, evaluationResult);
    drawWindspeedDisplayBarplot();
    drawWindspeedEvaluationBars(evaluationResult);
    drawMenuButtons();
    display.display();

    // Serial.print("Save data: ");
    // Serial.println(getLogCsvRow(windspeed));
  }
}