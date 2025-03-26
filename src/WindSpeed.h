#ifndef WindSpeed_h
#define WindSpeed_h

#include "Arduino.h"
#include <TimeLib.h>
#include <SD.h>
#include <ArduinoJson.h>

// structs, enums
struct WindspeedEvaluation
{
    float MaxWindspeed;
    float MinWindspeed;
    float AverageWindspeed;
    int NumberOfExceededRanges;
    int RangeStartIndex[15];
    int RangeStopIndex[15];
};

class WindSpeed
{
public:
    WindSpeed(uint8_t sensorPin, uint16_t evaluationRange = 300, uint16_t windspeedThreshold = 8, uint16_t windspeedDurationRange = 20);
    void setupInterruptCallback(void (*externalInterruptCallback)(void));
    void setupEvaluationCallback(std::function<void(void)> evaluationCallback);
    void setup();
    void interruptCallback();
    void calculateWindspeed(bool evaluate = true, bool log = false);
    float getCurrentWindspeed();
    WindspeedEvaluation getWindspeedEvaluation();
    String getWindspeedJson();
    String getWindspeedEvaluationJson();
    String getWindspeedEvaluationString();
    String getWindspeedString(bool addUnitSymbol = false);
    int getWindSpeedHistoryArrayElement(int i);
    String getTimestampString();

private:
    uint8_t _sensorPin;
    uint16_t _evaluationRange = 300;
    uint16_t _windspeedThreshold = 8;
    uint16_t _windspeedDurationRange = 20;
    uint16_t _numberOfRangesThreshold = 3;
    uint16_t _sampleRate = 1000;
    uint32_t _counter = 0;
    uint32_t _lastCounter = 0;
    bool _isCallbackAlreadySent = false;
    std::function<void(void)> _evaluationCallback = nullptr;
    WindspeedEvaluation _windspeedEvaluation;
    int _windspeedHistoryArray[300];
    void setupSDCard();
    void logWindspeedToSDCard(fs::FS &fs);
    void evaluateWindspeed();
    void updateWindspeedArray(float currentWindspeed);
    String getWindspeedEvaluationSingleString(float windspeedValue);
    String getLogCsvRow(float windspeedValue, char separationChar = ',');
    String getLogFilePath();
    String getLogFileHeader();
    void appendLineToFile(fs::FS &fs, const char *path, const char *message);
    void appendFile(fs::FS &fs, const char *path, const char *message);
    void writeLineToFile(fs::FS &fs, const char *path, const char *message);
    void writeFile(fs::FS &fs, const char *path, const char *message);
    void readFile(fs::FS &fs, const char *path);
    void createDir(fs::FS &fs, const char *path);
};

#endif