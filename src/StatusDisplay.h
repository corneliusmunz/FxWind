#ifndef StatusDisplay_h
#define StatusDisplay_h

#include "Arduino.h"
#include <M5GFX.h>
#include <M5Unified.h>

#define TXT_DEFAULT_COLOR TFT_WHITE
#define TXT_DEFAULT_BACKGROUND_COLOR TFT_BLACK

class StatusDisplay
{

public:
    StatusDisplay();
    void setup();
    void clear();
    void draw(String wifiStatus);

private:
    M5GFX _display;
};

#endif