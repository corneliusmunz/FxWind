#include "StatusDisplay.h"

StatusDisplay::StatusDisplay()
{

}

void StatusDisplay::setup()
{
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

void StatusDisplay::draw(String wifiStatus)
{
    _display.waitDisplay();
    _display.clear();
    _display.setTextColor(TXT_DEFAULT_COLOR, TXT_DEFAULT_BACKGROUND_COLOR);
    _display.setFont(&fonts::DejaVu18);
    _display.drawString("Status", 24, 0);
    _display.drawString(wifiStatus, 24, 0 + 24);
    _display.display();
}

void StatusDisplay::clear(){
    _display.waitDisplay();
    _display.clear();
    _display.display();
}