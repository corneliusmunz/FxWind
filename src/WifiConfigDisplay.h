#ifndef WifiConfigDisplay_h
#define WifiConfigDisplay_h

#include "Arduino.h"
#include <M5GFX.h>
#include <M5Unified.h>
#include <TimeLib.h>

#define TXT_DEFAULT_COLOR TFT_WHITE
#define DEFAULT_BACKGROUND_COLOR TFT_BLACK

class WifiConfigDisplay
{

public:
    void setup();
    WifiConfigDisplay();
    void draw(String Ssid, String Ip);

private:
    M5GFX _display;
};

#endif