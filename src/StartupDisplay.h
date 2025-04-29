#ifndef StartupDisplay_h
#define StartupDisplay_h

#include "Arduino.h"
#include <M5GFX.h>
#include <M5Unified.h>
#include <TimeLib.h>

#define TXT_DEFAULT_COLOR TFT_WHITE
#define DEFAULT_BACKGROUND_COLOR TFT_BLACK

class StartupDisplay
{

public:
    StartupDisplay();
    void setup(int displayBrightness, bool isAPEnabled = true);
    void setupStartButtonCallback(std::function<void(bool)> startButtonCallback);
    void draw();
    void evaluateTouches();

private:
    M5GFX _display;
    String getTimestampString();
    bool _isAPEnabled = true;
    bool _isStartButtonPressed = false;
    std::function<void(bool)> _startButtonCallback = nullptr;
};

#endif