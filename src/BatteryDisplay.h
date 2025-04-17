#ifndef BatteryDisplay_h
#define BatteryDisplay_h

#include "Arduino.h"
#include <M5GFX.h>
#include <M5Unified.h>
#include "plugImage.h"


#define BORDER_COLOR TFT_WHITE
#define TXT_DEFAULT_COLOR TFT_WHITE
#define DEFAULT_BACKGROUND_COLOR TFT_BLACK
#define BATTERY_FILL_COLOR TFT_GREEN
#define BATTERY_PADDING 40 
#define BATTERY_KNOB_WIDTH 10
#define BATTERY_BORDER_WIDTH 5

class BatteryDisplay
{

public:
    BatteryDisplay();
    void setup(int displayBrightness);
    void draw();

private:
    M5GFX _display;

    void drawBattery(int batteryLevel);
    void drawBatteryLevel(int batteryLevel);
    void drawPlug();
    void drawChargingCurrent(int chargingCurrent);
};

#endif