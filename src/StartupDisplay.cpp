#include "StartupDisplay.h"

StartupDisplay::StartupDisplay()
{

}

void StartupDisplay::setup(int displayBrightness, bool isAPEnabled)
{
    _isAPEnabled = isAPEnabled;
    _display.setBrightness(displayBrightness);
    _display.init();
    _display.startWrite();
    _display.fillScreen(TFT_BLACK);
    _display.endWrite();

    if (_display.isEPD())
    {
        _display.setEpdMode(epd_mode_t::epd_fastest);
    }
    if (_display.width() < _display.height())
    {
        _display.setRotation(_display.getRotation() ^ 1);
    }
}

String StartupDisplay::getTimestampString()
{
    time_t t = now();
    char stringbuffer[100];
    sprintf(stringbuffer, "%4u-%02u-%02u %02u:%02u:%02u", year(t), month(t), day(t), hour(t), minute(t), second(t));
    return String(stringbuffer);
}

void StartupDisplay::draw()
{
    _display.startWrite();
    _display.fillScreen(TFT_BLACK);
    _display.endWrite();

    _display.waitDisplay();
    _display.setTextColor(TXT_DEFAULT_COLOR, TFT_WHITE);
    
    int buttonWidth = 100;
    int buttonHeight = 50;
    int buttonSpacing = (_display.width() - 2 * buttonWidth) / 3;
    int buttonLine1StartY = 100;
    int buttonLine2StartY = 180;
    int headlineStartY = 5;
    int buttonColor = TFT_GREEN;

    _display.setFont(&fonts::DejaVu40);
    _display.drawString("F3xWind", 70, headlineStartY);

    _display.setFont(&fonts::DejaVu12);
    _display.drawString("Date: " + getTimestampString(), buttonSpacing, 50);
    _display.drawString("Battery Level: " + String(M5.Power.getBatteryLevel()) + " %", buttonSpacing, 68);

    if (_isAPEnabled)
    {
        _display.fillRoundRect(buttonSpacing, buttonLine1StartY, buttonWidth, buttonHeight, 5, buttonColor);
        _display.drawRoundRect(2 * buttonSpacing + buttonWidth, buttonLine1StartY, buttonWidth, buttonHeight, 5, buttonColor);
    }
    else
    {
        _display.drawRoundRect(buttonSpacing, buttonLine1StartY, buttonWidth, buttonHeight, 5, buttonColor);
        _display.fillRoundRect(2 * buttonSpacing + buttonWidth, buttonLine1StartY, buttonWidth, buttonHeight, 5, buttonColor);
    }

    if (_isStartButtonPressed)
    {
        _display.fillRoundRect(buttonSpacing, buttonLine2StartY, 2 * buttonWidth + buttonSpacing, buttonHeight, 5, buttonColor);
    }
    else
    {
        _display.drawRoundRect(buttonSpacing, buttonLine2StartY, 2 * buttonWidth + buttonSpacing, buttonHeight, 5, buttonColor);
    }

    _display.setFont(&fonts::DejaVu24);
    int fontHeight = _display.fontHeight();
    _display.drawString("AP", buttonSpacing+30, buttonLine1StartY + (buttonHeight - fontHeight) / 2);
    _display.drawString("WiFi", 2 * buttonSpacing + buttonWidth + 20, buttonLine1StartY + (buttonHeight - fontHeight) / 2);
    _display.drawString("START", buttonSpacing + 80, buttonLine2StartY + (buttonHeight - fontHeight) / 2);

    _display.display();
}

void StartupDisplay::evaluateTouches()
{
    int buttonWidth = 100;
    int buttonHeight = 60;
    int buttonSpacing = (_display.width() - 2 * buttonWidth) / 3;
    int buttonLine1StartY = 80;
    int buttonLine2StartY = 175;
    int headlineStartY = 10;

    M5.update();

    auto count = M5.Touch.getCount();
    if (count != 0)
    {
        static m5::touch_state_t prev_state;
        auto touchDetail = M5.Touch.getDetail();

        if (touchDetail.wasReleased()) 
        {
            // Start Button pressed
            if (touchDetail.x > buttonSpacing && touchDetail.x < _display.width() - buttonSpacing && touchDetail.y > buttonLine2StartY && touchDetail.y < buttonLine2StartY + buttonHeight)
            {
                Serial.println("Start Button released");
                if (_startButtonCallback != nullptr)
                {
                    _startButtonCallback(_isAPEnabled);
                }
                _isStartButtonPressed = false;
                draw();
            }
        }

        if (touchDetail.wasPressed())
        {
            // AP Button pressed
            if(touchDetail.x > buttonSpacing && touchDetail.x < buttonSpacing+buttonWidth 
                && touchDetail.y > buttonLine1StartY && touchDetail.y < buttonLine1StartY + buttonHeight)
            {
                Serial.println("AP Button pressed");
                _isAPEnabled = true;
                draw();
            }

            // Wifi Button pressed
            if (touchDetail.x > 2*buttonSpacing+buttonWidth && touchDetail.x < _display.width()-buttonSpacing 
                && touchDetail.y > buttonLine1StartY && touchDetail.y < buttonLine1StartY + buttonHeight)
            {
                Serial.println("WiFi Button pressed");
                _isAPEnabled = false;
                draw();
            }

            // Start Button pressed
            if (touchDetail.x > buttonSpacing && touchDetail.x < _display.width() - buttonSpacing 
                && touchDetail.y > buttonLine2StartY && touchDetail.y < buttonLine2StartY + buttonHeight)
            {
                Serial.println("Start Button pressed");
                _isStartButtonPressed = true;
                draw();
            }
        }
    }
}

void StartupDisplay::setupStartButtonCallback(std::function<void(bool)> startButtonCallback)
{
    _startButtonCallback = startButtonCallback;
}
