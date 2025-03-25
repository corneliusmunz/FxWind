#ifndef ConfigurationDisplay_h
#define ConfigurationDisplay_h

#include "Arduino.h"
#include <M5GFX.h>
#include <M5Unified.h>

#define TXT_DEFAULT_COLOR TFT_WHITE
#define TXT_DEFAULT_BACKGROUND_COLOR TFT_BLACK

class ConfigurationDisplay
{

public:
    ConfigurationDisplay();
    void setup();
    void clear();
    void draw(String configuration);

private:
    M5GFX _display;
};

#endif