
#include <M5GFX.h>
#include <M5Unified.h>
#include <TimeLib.h>
M5GFX display;

static constexpr size_t BAR_COUNT = 75;
static int y[BAR_COUNT];
static uint32_t colors[BAR_COUNT];
static constexpr size_t PLOT_HEIGHT = 100;
uint32_t okColor = display.color888(0, 255, 0);
uint32_t nokColor = display.color888(255, 0, 0);
long counter;
long lastCounter;
unsigned long lastMillis;
const unsigned long period = 1000;

void incrementCounter()
{
  counter++;
}

void setup(void)
{
  M5.begin();
  
  pinMode(19, INPUT);
  attachInterrupt(digitalPinToInterrupt(19), incrementCounter, RISING);

  setTime(17, 27, 40, 17, 9, 2024);

  auto cfg = M5.config();

  cfg.external_speaker.atomic_spk = true;

  // If you want to play sound from ModuleDisplay, write this
  cfg.external_speaker.module_display = true;
  M5.Speaker.setVolume(200);
  M5.Speaker.begin();

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

void loop(void)
{
  long currentMillis = millis();            
  if (currentMillis - lastMillis >= period) 
  {
    int deltaCounter = counter - lastCounter;
    int yCurrent = (int)((float)deltaCounter / 20.0f * 1.75f * 10);
    lastMillis = currentMillis;
    lastCounter = counter;
    
    time_t t = now(); // store the current time in time variable t
    Serial.print("time: ");
    Serial.print(hour(t), DEC);
    Serial.print(":");
    Serial.print(minute(t), DEC);
    Serial.print(":");
    Serial.println(second(t), DEC);

    M5.update();
    int h = PLOT_HEIGHT; // display.height();
    int yOffset = display.height() - PLOT_HEIGHT;
    int plotWidth = 300;
    int xOffset = 10;

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

    display.setFont(&fonts::DejaVu72);
    if (yCurrent >= 80)
    {
      display.setTextColor(TFT_WHITE, nokColor);
    }
    else
    {
      display.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    display.drawFloat(yCurrent / 10.0f, 1, display.width() / 2 - 150, 110);

    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawString("m/s", display.width() - 2 * 71, 110);

    display.waitDisplay();
    for (int x = 3; x < BAR_COUNT; ++x)
    {

      if (x == BAR_COUNT - 1)
      {
        y[x] = yCurrent;
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
    display.display();
  }

}