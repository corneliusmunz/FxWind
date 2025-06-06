#include "WindSpeedDisplay.h"

WindSpeedDisplay::WindSpeedDisplay(uint16_t lowerWindspeedThreshold, uint16_t upperWindspeedThreshold, uint16_t evaluationRange, uint16_t windspeedDurationRange, WindSpeed *windSpeed)
{
    _evaluationRange = evaluationRange;
    _lowerWindspeedThreshold = lowerWindspeedThreshold;
    _upperWindspeedThreshold = upperWindspeedThreshold;
    _windspeedDurationRange = windspeedDurationRange;
    _windSpeed = windSpeed;
    _currentDrawType = DrawType::COMBINED;
}

void WindSpeedDisplay::setup()
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

void WindSpeedDisplay::updateSettings(uint16_t lowerWindspeedThreshold, uint16_t upperWindspeedThreshold, uint16_t evaluationRange, uint16_t windspeedDurationRange, int brightness)
{
    _lowerWindspeedThreshold = lowerWindspeedThreshold;
    _upperWindspeedThreshold = upperWindspeedThreshold;
    _windspeedDurationRange = windspeedDurationRange;
    _display.setBrightness((int)(brightness/100.0f*255.0f));
}

void WindSpeedDisplay::draw(DrawType drawType)
{
    if (_currentDrawType != drawType)
    {
        if(_currentDrawType == DrawType::QR_CODE)
        {
            M5.Lcd.clear();
        }
        else
        {
            _display.clear();
        }
        _currentDrawType = drawType;
    }

    switch (drawType)
    {
    case DrawType::COMBINED:
        drawCombinedView();
        break;

    case DrawType::PLOT:
        drawPlotView();
        break;

    case DrawType::NUMBER:
        drawNumberView();
        break;

    case DrawType::STATUS:
        drawStatusView();
        break;

    case DrawType::QR_CODE:
        drawQRCode();
        break;

    default:
        drawCombinedView();
        break;
    }
}

void WindSpeedDisplay::drawQRCode()
{
    M5.Lcd.qrcode(String("http://") + String("fxwind") + String(".local"), 40, 0, 240);
}

void WindSpeedDisplay::drawStatusView()
{
    _display.waitDisplay();
    drawStatus();
    _display.display();
}

void WindSpeedDisplay::drawNumberView()
{
    _display.waitDisplay();
    drawValues(_windSpeed->getCurrentWindspeed(), _windSpeed->getWindspeedEvaluation(), 0, 0);
    _display.display();
}

void WindSpeedDisplay::drawPlotView()
{
    int evaluationBarHeight = 40;
    int plotHeight = _display.height() - evaluationBarHeight;
    _display.waitDisplay();
    drawBarPlot(plotHeight);
    drawEvaluationBars(_windSpeed->getWindspeedEvaluation(), plotHeight, evaluationBarHeight);
    _display.display();
}

void WindSpeedDisplay::drawCombinedView()
{
    _display.waitDisplay();
    drawValues(_windSpeed->getCurrentWindspeed(), _windSpeed->getWindspeedEvaluation(), PLOT_HEIGHT, EVALUATION_BAR_HEIGHT);
    drawBarPlot(PLOT_HEIGHT);
    drawEvaluationBars(_windSpeed->getWindspeedEvaluation(), PLOT_HEIGHT, EVALUATION_BAR_HEIGHT);
    _display.display();
}

void WindSpeedDisplay::drawStatus()
{
    _display.setFont(&fonts::DejaVu18);
    int fontHeight = _display.fontHeight();
    _display.setTextColor(TXT_DEFAULT_COLOR, TXT_DEFAULT_BACKGROUND_COLOR);
    int yPos = PLOT_OFFSET_Y;
    int spacer = 10;
    int yPosDelta = fontHeight + 4;

    _display.drawString("STATUS DISPLAY", 1, yPos);
    _display.drawString("Date: " + _windSpeed->getTimestampString(), 1, yPos + 1 * yPosDelta + 1 * spacer);
    _display.drawString("Battery Level: " + String(M5.Power.getBatteryLevel()) + " %", 1, yPos + 2 * yPosDelta + 2 * spacer);
    _display.drawString("Power connected: " + String(M5.Power.Axp192.isACIN()), 1, yPos + 3 * yPosDelta + 2 * spacer);
    _display.drawString("Charging: " + String(M5.Power.isCharging()), 1, yPos + 4 * yPosDelta + 2 * spacer);
    _display.drawString("Current: " + String(M5.Power.getBatteryCurrent()) + " mA", 1, yPos + 5 * yPosDelta + 2 * spacer);
    _display.drawString("Wifi IP: " + WiFi.localIP().toString(), 1, yPos + 6 * yPosDelta + 4 * spacer);
    _display.drawString("Wifi RSSI: " + String(WiFi.RSSI()) + " dB", 1, yPos + 7 * yPosDelta + 4 * spacer);
    _display.drawString("FW Version: " + String(FWVERSION), 1, yPos + 8 * yPosDelta + 4 * spacer);
  }

void WindSpeedDisplay::drawValues(float windspeed, WindspeedEvaluation windspeedEvaluation, int plotHeight, int evaluationBarHeight)
{
    _display.setFont(&fonts::DejaVu72);
    int bigFontHeight = _display.fontHeight();

    int yPos = PLOT_OFFSET_Y + evaluationBarHeight + plotHeight + 12;
    if (plotHeight == 0 && evaluationBarHeight == 0)
    {
        yPos = (int)(_display.height() / 2.0) - (int)(bigFontHeight / 2.0) - 9;
    }
    if (windspeed > _upperWindspeedThreshold || windspeed < _lowerWindspeedThreshold)
    {
        _display.setTextColor(TXT_DEFAULT_COLOR, TXT_ALERT_BACKGROUND_COLOR);
    }
    else
    {
        _display.setTextColor(TXT_DEFAULT_COLOR, TXT_DEFAULT_BACKGROUND_COLOR);
    }
    _display.drawString(_windSpeed->getWindspeedString(true), 1, yPos);
    _display.setFont(&fonts::DejaVu18);
    _display.setTextColor(TXT_DEFAULT_COLOR, TXT_DEFAULT_BACKGROUND_COLOR);
    String evaluationString = _windSpeed->getWindspeedEvaluationString();
    _display.drawString(evaluationString, 24, yPos + bigFontHeight + 6);
}

void WindSpeedDisplay::drawGrid(int plotHeight)
{
    _display.setFont(&fonts::DejaVu12);
    _display.setTextColor(TXT_DEFAULT_COLOR, TXT_DEFAULT_BACKGROUND_COLOR);
    int fontOffsetY = (int)(_display.fontHeight() / 2.0f);
    int scaleTicks = (int)(plotHeight / 10.0);
    for (size_t i = 0; i <= 10; i += 2)
    {
        int fontOffsetX = i < 10 ? _display.fontWidth() : 2 * _display.fontWidth();
        _display.drawString(String(i), PLOT_OFFSET_X - fontOffsetX - 10, PLOT_OFFSET_Y - fontOffsetY + plotHeight - i * scaleTicks);
    }

    for (size_t i = 0; i <= 10; i += 1)
    {
        for (size_t j = 0; j < 1; j += 10)
        {
            _display.drawFastHLine(PLOT_OFFSET_X - 4 + j, PLOT_OFFSET_Y + plotHeight - i * scaleTicks, 4, GRID_COLOR);
        }
    }
}

void WindSpeedDisplay::drawBarPlot(int plotHeight)
{
    drawGrid(plotHeight);

    for (int x = 0; x < _evaluationRange; x++)
    {
        int xpos = PLOT_OFFSET_X + _evaluationRange - x - 1;
        int xValue = _windSpeed->getWindSpeedHistoryArrayElement(x);
        int xValueScaled = (int)(xValue / 100.0 * plotHeight);
        int xValueScaledLimited = min(xValueScaled, plotHeight);

        _display.drawFastVLine(xpos, PLOT_OFFSET_Y, plotHeight, TFT_BLACK);
        if (xValue > _upperWindspeedThreshold * 10 || xValue < _lowerWindspeedThreshold * 10)
        {
            _display.drawFastVLine(xpos, PLOT_OFFSET_Y + plotHeight - xValueScaledLimited, xValueScaledLimited, PLOT_BAR_ALERT_COLOR);
        }
        else
        {
            _display.drawFastVLine(xpos, PLOT_OFFSET_Y + plotHeight - xValueScaledLimited, xValueScaledLimited, PLOT_BAR_DEFAULT_COLOR);
        }
    }
}

void WindSpeedDisplay::drawEvaluationBars(WindspeedEvaluation windspeedEvaluation, int plotHeight, int evaluationBarHeight)
{

    _display.setFont(&fonts::DejaVu12);
    _display.setTextColor(TXT_DEFAULT_COLOR, TXT_ALERT_BACKGROUND_COLOR);
    int y = PLOT_OFFSET_Y + plotHeight + 3;
    _display.fillRect(PLOT_OFFSET_X - 1, y, _evaluationRange, evaluationBarHeight, TFT_GREEN);
    int numberTextXOffset = (int) (_windspeedDurationRange - 10)/2 + 1;
    for (size_t i = 0; i < windspeedEvaluation.NumberOfExceededRanges; i++)
    {
        int x = PLOT_OFFSET_X + _evaluationRange - windspeedEvaluation.RangeStartIndex[i] - _windspeedDurationRange;
        _display.fillRect(x, y, _windspeedDurationRange, evaluationBarHeight, TFT_RED);
        _display.drawString(String(i + 1), x + numberTextXOffset, y + evaluationBarHeight / 5);
    }
}