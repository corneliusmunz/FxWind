#ifndef WindSpeed_h
#define WindSpeed_h

#include "Arduino.h"
#include <TimeLib.h>
#include <SD.h>

// structs, enums
struct WindspeedEvaluation
{
    float MaxWindspeed;
    float MinWindspeed;
    float AverageWindspeed;
    int NumberOfExceededRanges;
    int RangeStartIndex[15];
};

class WindSpeed
{
public:
    WindSpeed(uint8_t sensorPin, uint16_t evaluationRange = 300, uint16_t windspeedThreshold = 8, uint16_t windspeedDurationRange = 20);
    void setupInterruptCallback(void (*externalInterruptCallback)(void));
    void interruptCallback();
    void calculateWindspeed();
    float getCurrentWindspeed();
    void  timerCallback();
    void updateWindspeedArray(float currentWindspeed);
    String getTimestampString();
    String getWindspeedEvaluationSingleString(float windspeedValue);
    String getWindspeedEvaluationString(WindspeedEvaluation windspeedEvaluation);
    String getWindspeedString(float windspeedValue, bool addUnitSymbol);
    String getLogCsvRow(float windspeedValue, char separationChar = ',');
    String getLogFilePath();
    String getLogFileHeader();
    void appendLineToFile(fs::FS &fs, const char *path, const char *message);
    void appendFile(fs::FS &fs, const char *path, const char *message);
    void writeLineToFile(fs::FS &fs, const char *path, const char *message);
    void writeFile(fs::FS &fs, const char *path, const char *message);
    void readFile(fs::FS &fs, const char *path);
    void createDir(fs::FS &fs, const char *path);
    void logWindspeedToSDCard(fs::FS &fs, float windspeed);
    WindspeedEvaluation evaluateWindspeed();

        private : uint8_t _sensorPin;
    uint16_t _evaluationRange = 300;
    uint16_t _windspeedThreshold = 8;
    uint16_t _windspeedDurationRange = 20;
    uint16_t _sampleRate = 1000;
    uint32_t _counter = 0;
    uint32_t _lastCounter = 0;
    int _windspeedHistoryArray[300];
};

#endif