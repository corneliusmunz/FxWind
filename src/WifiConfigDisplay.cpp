#include "WifiConfigDisplay.h"

WifiConfigDisplay::WifiConfigDisplay()
{
}

void WifiConfigDisplay::setup()
{
    _display.setBrightness(255);
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

void WifiConfigDisplay::draw(String Ssid, String Ip)
{
    _display.startWrite();
    _display.fillScreen(TFT_BLACK);
    _display.endWrite();

    _display.waitDisplay();
    _display.setTextColor(TXT_DEFAULT_COLOR, TFT_WHITE);

    _display.setFont(&fonts::DejaVu40);
    _display.drawString("FxWind", 75, 5);

    _display.setFont(&fonts::DejaVu18);
    _display.drawString("To configure the Wifi", 10, 55);
    _display.drawString("connection, please open the", 10, 85);
    _display.drawString("Wifi with SSID: ", 10, 115);
    _display.setTextColor(TFT_GREEN);
    _display.drawString(Ssid, 10, 145);

    _display.setTextColor(TXT_DEFAULT_COLOR, TFT_WHITE);
    _display.drawString("The device will afterwards", 10, 175);
    _display.drawString("restart automatically", 10, 205);

    _display.display();
}
