
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
#define BAR_COUNT 75
#define EVALUATION_RANGE 60
static int windspeedHistoryArray[EVALUATION_RANGE];
static int y[BAR_COUNT];
static uint32_t colors[BAR_COUNT];
static constexpr size_t PLOT_HEIGHT = 100;
uint32_t okColor = display.color888(0, 255, 0);
uint32_t nokColor = display.color888(255, 0, 0);


struct WindspeedEvaluation
{
  float MaxWindspeed;
  float MinWindspeed;
  float AverageWindspeed;
  int NumberOfExceededRanges;
};

// global variables
long counter;
long lastCounter;
int displayUpadteDivider;
unsigned long lastMillis;
const unsigned long period = 1000;
const char ssid[] = "";
const char pass[] = "";
static const char ntpServerName[] = "de.pool.ntp.org";
unsigned int localPort = 8888; 
const int timeZone = 0;
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

String getWindspeedString(float windspeedValue, bool addUnitSymbol = false) {
  char stringbuffer[100];
  if (addUnitSymbol) {
    sprintf(stringbuffer, "%.1f m/s  ", windspeedValue);
  } else {
    sprintf(stringbuffer, "%.1f", windspeedValue);
  }
  return String(stringbuffer);
}

String getWindspeedEvaluationString(WindspeedEvaluation windspeedEvaluation) {
  return 
    "Max:" 
    + String(windspeedEvaluation.MaxWindspeed) 
    + " Min:"
    + String(windspeedEvaluation.MinWindspeed)
    + " Average:"
    + String(windspeedEvaluation.AverageWindspeed);
}

String getTimestampString() {
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
float calculateWindspeed(int deltaCounter) {
  return ((float)deltaCounter / 20.0f * 1.75f);
}

float getMaxValueOfWindspeedHistory(int evaluationSamples=4) {
  int maxValue = 0;
  for (size_t i = 0; i < evaluationSamples; i++)
  {
    if (windspeedHistoryArray[i]>maxValue) {
      maxValue = windspeedHistoryArray[i];
    }
  }
  return (float)maxValue/10.0f;
}

void updateWindspeedArray(float windspeed){
  for (size_t i = EVALUATION_RANGE; i > 0; i--)
  {
    windspeedHistoryArray[i] = windspeedHistoryArray[i-1];
  }
  windspeedHistoryArray[0] = (int)(windspeed*10.0f);

  // for (size_t i = 0; i < EVALUATION_RANGE; i++)
  // {
  //   Serial.print(i, DEC);
  //   Serial.print(":");
  //   Serial.print(windspeedHistoryArray[i], DEC);
  //   Serial.print(", ");
  // }
  // Serial.println("");
  
}

WindspeedEvaluation evaluateWindspeed() {

  int maxWindspeed=0;
  int minWindspeed=INT_MAX;
  long sumWindspeed=0;
  for (size_t i = 0; i < EVALUATION_RANGE; i++)
  {
    if (windspeedHistoryArray[i]>maxWindspeed) {
      maxWindspeed=windspeedHistoryArray[i];
    }
    if (windspeedHistoryArray[i]<minWindspeed)
    {
      minWindspeed = windspeedHistoryArray[i];
    }

    sumWindspeed += windspeedHistoryArray[i];
  }

  WindspeedEvaluation evaluationResult;

  evaluationResult.MaxWindspeed = (float)maxWindspeed/10.0f;
  evaluationResult.MinWindspeed = (float)minWindspeed/10.0f;
  evaluationResult.AverageWindspeed = (float)(sumWindspeed/EVALUATION_RANGE)/10.0f;

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

void drawMenuButton(String label, int xPos, bool isPressed = false) {
  display.setFont(&fonts::AsciiFont8x16);
  if (!isPressed)
  {
    display.setColor(TFT_NAVY);
    display.setTextColor(TFT_NAVY, TFT_BLACK);
  }
  else
  {
    display.setColor(TFT_WHITE);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  display.drawRect(xPos, 218, 64, 24);
  display.drawCenterString(label, 64, 223);
}

void drawMenuButtons() {
  display.setFont(&fonts::AsciiFont8x16);
  if (M5.BtnA.isHolding())
  {
    display.setColor(TFT_NAVY);
    display.setTextColor(TFT_NAVY, TFT_BLACK);
    if (!M5.Speaker.isPlaying())
    {
      M5.Speaker.tone(1000, 500);
    }
  }
  else
  {
    display.setColor(TFT_WHITE);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  display.drawRect(31, 218, 64, 24);
  display.drawCenterString("ABCD", 64, 223);

  if (M5.BtnB.isPressed())
  {
    display.setColor(TFT_NAVY);
    display.setTextColor(TFT_NAVY, TFT_BLACK);
    M5.Speaker.tone(500, 100);
  }
  else
  {
    display.setColor(TFT_WHITE);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  display.drawRect(127, 218, 64, 24);
  display.drawCenterString("EFGH", 160, 223);

  if (M5.BtnC.wasPressed())
  {
    display.setColor(TFT_NAVY);
    display.setTextColor(TFT_NAVY, TFT_BLACK);
    M5.Speaker.tone(2000, 100);
  }
  else
  {
    display.setColor(TFT_WHITE);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  display.drawRect(223, 218, 64, 24);
  display.drawCenterString("IJKL", 256, 223);
}

void drawWindspeedDisplayValues(float windspeed, WindspeedEvaluation windspeedEvaluation)
{
  display.setFont(&fonts::DejaVu72);
  if (windspeed > 8.0f)
  {
    display.setTextColor(TFT_WHITE, nokColor);
  }
  else
  {
    display.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  display.drawString(getWindspeedString(windspeed, true), 1, 110);

  display.setFont(&fonts::DejaVu18);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString(getWindspeedEvaluationString(windspeedEvaluation), 1, 185);
}

void drawWindspeedDisplayBarplot(float windspeed)
{
  int h = PLOT_HEIGHT;
  int yOffset = display.height() - PLOT_HEIGHT;
  int plotWidth = 300;
  int xOffset = 10;

  for (int x = 3; x < BAR_COUNT; ++x)
  {

    if (x == BAR_COUNT - 1)
    {
      y[x] = (int)(windspeed * 10.0f);
    }
    else
    {
      y[x] = y[x + 1];
    }

    int xpos = xOffset + (x * plotWidth / BAR_COUNT);
    int barWidth = 3; // plotWidth / BAR_COUNT;

    display.fillRect(xpos, 0, barWidth, PLOT_HEIGHT, TFT_BLACK);
    if (y[x] >= 80 && y[x - 1] >= 80 && y[x - 2] >= 80 && y[x - 3] >= 80)
    {
      display.fillRect(xpos, PLOT_HEIGHT - y[x], barWidth, y[x], TFT_RED);
    }
    else
    {
      if (y[x] >= 80 && y[x - 1] >= 80 && y[x - 2] >= 80)
      {
        display.fillRect(xpos, PLOT_HEIGHT - y[x], barWidth, y[x], TFT_ORANGE);
      }
      else
      {
        if (y[x] >= 80)
        {
          display.fillRect(xpos, PLOT_HEIGHT - y[x], barWidth, y[x], TFT_YELLOW);
        }
        else
        {
          display.fillRect(xpos, PLOT_HEIGHT - y[x], barWidth, y[x], TFT_GREEN);
        }
      }
    }
  }
}

void setupSoundModule() {
  auto cfg = M5.config();
  cfg.external_speaker.atomic_spk = true;
  cfg.external_speaker.module_display = true;
  M5.Speaker.setVolume(200);
  M5.Speaker.begin();
}

void setupWifi() {
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
}

void setupNtpTimeSyncProvider() {
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
}

void setupWindspeedIO() {
  pinMode(WINDSPEED_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(WINDSPEED_PIN), incrementCounter, RISING);
}

void setupDisplay() {
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

  for (int x = 0; x < BAR_COUNT; ++x)
  {
    y[x] = 0;
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
  if (currentMillis - lastMillis >= period) 
  {
    M5.update();
    Serial.println("counter: "+String(counter));
    float windspeed = calculateWindspeed(counter - lastCounter);
    updateWindspeedArray(windspeed);
    WindspeedEvaluation evaluationResult = evaluateWindspeed();
    lastMillis = currentMillis;
    lastCounter = counter;

    display.waitDisplay();
    drawWindspeedDisplayValues(windspeed, evaluationResult);
    displayUpadteDivider++;
    if (displayUpadteDivider==4) {
      drawWindspeedDisplayBarplot(getMaxValueOfWindspeedHistory());
      displayUpadteDivider=0;
    }
    drawMenuButtons();
    display.display();

    // Serial.print("Save data: ");
    // Serial.println(getLogCsvRow(windspeed));
  }

}