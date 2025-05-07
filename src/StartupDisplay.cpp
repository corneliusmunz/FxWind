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
    auto dateTime = M5.Rtc.getDateTime();
    char stringbuffer[100];
    sprintf(stringbuffer, "%4u-%02u-%02u %02u:%02u:%02u", dateTime.date.year, dateTime.date.month, dateTime.date.date, dateTime.time.hours, dateTime.time.minutes, dateTime.time.seconds);
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
    int disabledButtonColor = TFT_DARKGREY;

    _display.setFont(&fonts::DejaVu40);
    _display.drawString("FxWind", 75, headlineStartY);

    _display.setFont(&fonts::DejaVu12);
    _display.drawString("Date: " + getTimestampString(), buttonSpacing, 50);
    _display.drawString("Battery Level: " + String(M5.Power.getBatteryLevel()) + " %", buttonSpacing, 68);

    if (_isWifiEnabled)
    {
        _display.fillRoundRect(buttonSpacing, buttonLine1StartY, buttonWidth, buttonHeight, 5, buttonColor);
    }
    else
    {
        _display.drawRoundRect(buttonSpacing, buttonLine1StartY, buttonWidth, buttonHeight, 5, buttonColor);
    }

    if (_isAPEnabled)
    {
        _display.fillRoundRect(2 * buttonSpacing + buttonWidth, buttonLine1StartY, buttonWidth, buttonHeight, 5, _isWifiEnabled ? buttonColor : disabledButtonColor);
    }
    else
    {
        _display.drawRoundRect(2 * buttonSpacing + buttonWidth, buttonLine1StartY, buttonWidth, buttonHeight, 5, _isWifiEnabled ? buttonColor : disabledButtonColor);
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
    _display.drawString("WiFi", buttonSpacing + 22, buttonLine1StartY + (buttonHeight - fontHeight) / 2);
    _display.drawString("AP", 2 * buttonSpacing + buttonWidth + 35, buttonLine1StartY + (buttonHeight - fontHeight) / 2);
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
                    _startButtonCallback(_isWifiEnabled, _isAPEnabled);
                }
                _isStartButtonPressed = false;
                draw();
            }
        }

        if (touchDetail.wasPressed())
        {
            // WiFi Button pressed
            if (touchDetail.x > buttonSpacing && touchDetail.x < buttonSpacing + buttonWidth && touchDetail.y > buttonLine1StartY && touchDetail.y < buttonLine1StartY + buttonHeight)
            {
                Serial.println("WiFi Button pressed");
                _isWifiEnabled = !_isWifiEnabled;
                draw();
            }

            // AP Button pressed
            if (touchDetail.x > 2 * buttonSpacing + buttonWidth && touchDetail.x < _display.width() - buttonSpacing && touchDetail.y > buttonLine1StartY && touchDetail.y < buttonLine1StartY + buttonHeight)
            {
                Serial.println("AP Button pressed");
                if (_isWifiEnabled) {
                    _isAPEnabled = !_isAPEnabled;
                    draw();
                }
            }

            // Start Button pressed
            if (touchDetail.x > buttonSpacing && touchDetail.x < _display.width() - buttonSpacing && touchDetail.y > buttonLine2StartY && touchDetail.y < buttonLine2StartY + buttonHeight)
            {
                Serial.println("Start Button pressed");
                _isStartButtonPressed = true;
                draw();
            }
        }
    }
}

void StartupDisplay::setupStartButtonCallback(std::function<void(bool, bool)> startButtonCallback)
{
    _startButtonCallback = startButtonCallback;
}
