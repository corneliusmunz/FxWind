#include "BatteryDisplay.h"

BatteryDisplay::BatteryDisplay()
{

}

void BatteryDisplay::setup(int displayBrightness)
{
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

void BatteryDisplay::draw()
{
    _display.startWrite();
    _display.fillScreen(TFT_BLACK);
    _display.endWrite();

    _display.waitDisplay();

    int batteryLevel = M5.Power.getBatteryLevel();
    drawBattery(batteryLevel);
    drawBatteryLevel(batteryLevel);
    if (M5.Power.Axp192.isACIN())
    {
        drawPlug();
        drawChargingCurrent(M5.Power.Axp192.getBatteryChargeCurrent());
    }

    _display.display();
}

void BatteryDisplay::drawBattery(int batteryLevel)
{
    int displayWidth = _display.width();
    int displayHeight = _display.height();
    int y0 = 20;
    int batteryHeight = 80;
    int batteryKnobHeight = 26;
    int barWidth = (int)(batteryLevel / 100.0 * (displayWidth - 2 * BATTERY_PADDING - 2 * BATTERY_BORDER_WIDTH));

    _display.fillRect(BATTERY_PADDING, y0, displayWidth - 2*BATTERY_PADDING, batteryHeight, BORDER_COLOR);
    _display.fillRect(BATTERY_PADDING + BATTERY_BORDER_WIDTH, y0 + BATTERY_BORDER_WIDTH, displayWidth - 2*BATTERY_PADDING -2*BATTERY_BORDER_WIDTH, batteryHeight - 2 * BATTERY_BORDER_WIDTH, DEFAULT_BACKGROUND_COLOR);
    _display.fillRect(displayWidth-BATTERY_PADDING, y0+(int)(batteryHeight/2.0)-(int)(batteryKnobHeight/2.0), 2*BATTERY_BORDER_WIDTH, batteryKnobHeight, BORDER_COLOR);

    _display.fillRect(BATTERY_PADDING + BATTERY_BORDER_WIDTH, y0 + BATTERY_BORDER_WIDTH, barWidth, batteryHeight - 2 * BATTERY_BORDER_WIDTH, TFT_GREEN);
    //_display.fillRect(x,y, width, height, DEFAULT_BACKGROUND_COLOR);
    //_display.drawFastHLine(PLOT_OFFSET_X - 4 + j, PLOT_OFFSET_Y + plotHeight - i * scaleTicks, 4, GRID_COLOR);
    //_display.drawFastVLine(xpos, PLOT_OFFSET_Y, plotHeight, TFT_BLACK);
}

void BatteryDisplay::drawBatteryLevel(int batteryLevel)
{
    int yPos = 20+26;
    int xPos = BATTERY_PADDING + BATTERY_BORDER_WIDTH + 50;
    _display.setFont(&fonts::DejaVu40);
    int bigFontHeight = _display.fontHeight();
    _display.setTextColor(TXT_DEFAULT_COLOR, TFT_WHITE);
    _display.drawString(String(batteryLevel) + "%", xPos, yPos);
}

void BatteryDisplay::drawPlug()
{
    int xPos = (int)_display.width()/2.0-10;
    int yPos = 150;
    // _display.drawArc(xPos, yPos, 10, 14, 90, 270, TFT_GREEN);
    M5.Lcd.pushImage(xPos, yPos, 64, 64, (m5gfx::rgb565_t *)lcd_plug_image);
}
    // 

void BatteryDisplay::drawChargingCurrent(int chargingCurrent)
{

}


