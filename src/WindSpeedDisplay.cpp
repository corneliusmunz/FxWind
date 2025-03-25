#include "WindSpeedDisplay.h"

WindSpeedDisplay::WindSpeedDisplay(uint16_t evaluationRange, uint16_t windspeedThreshold, uint16_t windspeedDurationRange, WindSpeed *windSpeed)
{
    _evaluationRange = evaluationRange;
    _windspeedThreshold = windspeedThreshold;
    _windspeedDurationRange = windspeedDurationRange;
    _windSpeed = windSpeed;
    _currentDrawType = DrawType::COMBINED;
}

void WindSpeedDisplay::setup()
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

void WindSpeedDisplay::draw(DrawType drawType) 
{
    Serial.println();
    Serial.printf("DrawType: %d", drawType);
    if (_currentDrawType != drawType) {
        _display.clear();
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

    case DrawType::STATISTIC:
        drawStatisticView();
        break;
    
    default:
        drawCombinedView();
        break;
    }
}

void WindSpeedDisplay::drawStatisticView() 
{
    Serial.println("Statistic view");
    _display.waitDisplay();
    drawStatistic(_windSpeed->getWindspeedEvaluation());
    _display.display();
}

void WindSpeedDisplay::drawNumberView()
{
    Serial.println("Number view");
    _display.waitDisplay();
    drawValues(_windSpeed->getCurrentWindspeed(), _windSpeed->getWindspeedEvaluation());
    _display.display();
}

void WindSpeedDisplay::drawPlotView()
{
    Serial.println("Plot view");
    _display.waitDisplay();
    drawBarPlot();
    drawEvaluationBars(_windSpeed->getWindspeedEvaluation());
    _display.display();
}

void WindSpeedDisplay::drawCombinedView()
{
    Serial.println("Combined view");
    _display.waitDisplay();
    drawValues(_windSpeed->getCurrentWindspeed(), _windSpeed->getWindspeedEvaluation());
    drawBarPlot();
    drawEvaluationBars(_windSpeed->getWindspeedEvaluation());
    _display.display();
}

void WindSpeedDisplay::drawStatistic(WindspeedEvaluation windspeedEvaluation)
{
    int yPos = PLOT_OFFSET_Y + EVALUATION_BAR_HEIGHT + PLOT_HEIGHT + 12;
    _display.setFont(&fonts::DejaVu72);
    int bigFontHeight = _display.fontHeight();
    _display.setTextColor(TXT_DEFAULT_COLOR, TXT_DEFAULT_BACKGROUND_COLOR);
    _display.drawString("Statistic", 1, yPos);
    _display.setFont(&fonts::DejaVu18);
    char stringbuffer[100];
    sprintf(stringbuffer, "%.1f", windspeedEvaluation.MaxWindspeed);
    String evaluationString = "Max: " + String(stringbuffer);
    _display.drawString(evaluationString, 24, yPos + bigFontHeight + 6);
}

void WindSpeedDisplay::drawValues(float windspeed, WindspeedEvaluation windspeedEvaluation)
{
    int yPos = PLOT_OFFSET_Y + EVALUATION_BAR_HEIGHT + PLOT_HEIGHT + 12;
    _display.setFont(&fonts::DejaVu72);
    int bigFontHeight = _display.fontHeight();
    if (windspeed > _windspeedThreshold)
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

void WindSpeedDisplay::drawGrid()
{
    _display.setFont(&fonts::DejaVu12);
    _display.setTextColor(TXT_DEFAULT_COLOR, TXT_DEFAULT_BACKGROUND_COLOR);
    int fontOffsetY = (int)(_display.fontHeight() / 2.0f);
    for (size_t i = 0; i <= 10; i += 2)
    {
        int fontOffsetX = i < 10 ? _display.fontWidth() : 2 * _display.fontWidth();
        _display.drawString(String(i), PLOT_OFFSET_X - fontOffsetX - 10, PLOT_OFFSET_Y - fontOffsetY + PLOT_HEIGHT - i * 10);
    }

    for (size_t i = 0; i <= 10; i += 1)
    {
        for (size_t j = 0; j < 1; j += 10)
        {
            _display.drawFastHLine(PLOT_OFFSET_X - 4 + j, PLOT_OFFSET_Y + PLOT_HEIGHT - i * 10, 4, GRID_COLOR);
        }
    }

    // top and bottom line
    // display.drawLine(PLOT_OFFSET_X, PLOT_OFFSET_Y - 1, PLOT_OFFSET_X + _evaluationRange, PLOT_OFFSET_Y - 1, GRID_COLOR);
    // display.drawLine(PLOT_OFFSET_X, PLOT_HEIGHT+PLOT_OFFSET_Y+1, PLOT_OFFSET_X + _evaluationRange, PLOT_HEIGHT+PLOT_OFFSET_Y+1, GRID_COLOR);
}

void WindSpeedDisplay::drawBarPlot()
{
    int h = PLOT_HEIGHT;

    drawGrid();

    for (int x = 0; x < _evaluationRange; x++)
    {
        int xpos = PLOT_OFFSET_X + _evaluationRange - x - 1;

        _display.drawFastVLine(xpos, PLOT_OFFSET_Y, PLOT_HEIGHT, TFT_BLACK);
        if (_windSpeed->getWindSpeedHistoryArrayElement(x) >= _windspeedThreshold * 10)
        {
            _display.drawFastVLine(xpos, PLOT_OFFSET_Y + PLOT_HEIGHT - min(_windSpeed->getWindSpeedHistoryArrayElement(x), 100), min(_windSpeed->getWindSpeedHistoryArrayElement(x), 100), PLOT_BAR_ALERT_COLOR);
        }
        else
        {
            _display.drawFastVLine(xpos, PLOT_OFFSET_Y + PLOT_HEIGHT - min(_windSpeed->getWindSpeedHistoryArrayElement(x), 100), min(_windSpeed->getWindSpeedHistoryArrayElement(x), 100), PLOT_BAR_DEFAULT_COLOR);
        }
    }
}

void WindSpeedDisplay::drawEvaluationBars(WindspeedEvaluation windspeedEvaluation)
{

    _display.setFont(&fonts::DejaVu12);
    _display.setTextColor(TXT_DEFAULT_COLOR, TXT_ALERT_BACKGROUND_COLOR);
    int y = PLOT_OFFSET_Y + PLOT_HEIGHT + 3;
    _display.fillRect(PLOT_OFFSET_X - 1, y, _evaluationRange, EVALUATION_BAR_HEIGHT, TFT_GREEN);
    for (size_t i = 0; i < windspeedEvaluation.NumberOfExceededRanges; i++)
    {
        int x = PLOT_OFFSET_X + _evaluationRange - windspeedEvaluation.RangeStartIndex[i] - _windspeedDurationRange;
        _display.fillRect(x, y, _windspeedDurationRange, EVALUATION_BAR_HEIGHT, TFT_RED);
        _display.drawString(String(i + 1), x + 7, y + 4);
    }
}