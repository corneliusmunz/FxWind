#ifndef WindSpeedDisplay_h
#define WindSpeedDisplay_h

#include "Arduino.h"
#include "WindSpeed.h"
#include <M5GFX.h>
#include <M5Unified.h>

enum struct DrawType
{
    COMBINED = 0,
    PLOT = 1,
    NUMBER = 2,
    STATISTIC = 3
};

#define PLOT_OFFSET_X 20
#define PLOT_OFFSET_Y 5
#define PLOT_HEIGHT 100
#define EVALUATION_BAR_HEIGHT 20
#define TXT_DEFAULT_COLOR TFT_WHITE
#define TXT_ALERT_COLOR TFT_RED
#define TXT_DEFAULT_BACKGROUND_COLOR TFT_BLACK
#define TXT_ALERT_BACKGROUND_COLOR TFT_RED
#define GRID_COLOR TFT_DARKGREY
#define PLOT_BAR_DEFAULT_COLOR TFT_GREEN
#define PLOT_BAR_ALERT_COLOR TFT_RED
class WindSpeedDisplay
{

public:
    WindSpeedDisplay(uint16_t evaluationRange = 300, uint16_t windspeedThreshold = 8, uint16_t windspeedDurationRange = 20, WindSpeed *windSpeed = nullptr);
    void setup();
    void clear();
    void draw(DrawType drawType);

private:
    M5GFX _display;
    uint16_t _evaluationRange = 300;
    uint16_t _windspeedThreshold = 8;
    uint16_t _windspeedDurationRange = 20;
    WindSpeed *_windSpeed;
    DrawType _currentDrawType;

    void drawStatisticView();
    void drawPlotView();
    void drawCombinedView();
    void drawNumberView();

    void drawStatistic(WindspeedEvaluation windspeedEvaluation);
    void drawValues(float windspeed, WindspeedEvaluation windspeedEvaluation);
    void drawBarPlot();
    void drawEvaluationBars(WindspeedEvaluation windspeedEvaluation);
    void drawGrid();
};

#endif