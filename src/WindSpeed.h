#ifndef WindSpeed_h
#define WindSpeed_h

#include "Arduino.h"
#include <TimeLib.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <M5Unified.h>

// structs, enums
struct WindspeedEvaluation
{
    float MaxWindspeed;
    float MinWindspeed;
    float AverageWindspeed;
    int NumberOfExceededRanges;
    int RangeStartIndex[30];
    int RangeStopIndex[30];
};

class WindSpeed
{
public:
    WindSpeed(uint8_t sensorPin, uint16_t windspeedLowerThreshold = 0, uint16_t windspeedUpperThreshold = 8, uint16_t windspeedDurationRange = 20, uint16_t evaluationRange = 300, uint16_t numberOfWindowsThreshold = 3, uint16_t calibrationFactor = 1);
    void setupInterruptCallback(void (*externalInterruptCallback)(void));
    void setupEvaluationCallback(std::function<void(void)> evaluationCallback);
    void setup();
    void updateSettings(uint16_t windspeedLowerThreshold, uint16_t windspeedUpperThreshold, uint16_t windspeedDurationRange, uint16_t evaluationRange, uint16_t numberOfWindowsThreshold, uint16_t calibrationValue);
    void interruptCallback();
    void calculateWindspeed(bool evaluate = true, bool log = false);
    float getCurrentWindspeed();
    WindspeedEvaluation getWindspeedEvaluation();
    String getWindspeedJson();
    String getWindspeedEvaluationJson();
    String getWindspeedEvaluationString();
    String getWindspeedEvaluationString(float windspeedValue);
    String getWindspeedString(bool addUnitSymbol = false);
    int getWindSpeedHistoryArrayElement(int i);
    String getTimestampString();

private:
    uint8_t _sensorPin;
    uint16_t _calibrationFactor = 1;
    uint16_t _evaluationRange = 300;
    uint16_t _windspeedLowerThreshold = 0;
    uint16_t _windspeedUpperThreshold = 8;
    uint16_t _windspeedDurationRange = 20;
    uint16_t _numberOfWindowsThreshold = 3;
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
    String getLogCsvRow(char separationChar = ',');
    String getLogFilePath();
    String getLogFileHeader();
    void appendLineToFile(fs::FS &fs, const char *path, const char *message);
    void appendFile(fs::FS &fs, const char *path, const char *message);
    void writeLineToFile(fs::FS &fs, const char *path, const char *message);
    void writeFile(fs::FS &fs, const char *path, const char *message);
    void readFile(fs::FS &fs, const char *path);
    void createDir(fs::FS &fs, const char *path);
    void storeSnapshot();
    void storeJsonSnapshot();
    void storeJsonEvaluationSnapshot();
    void storeCsvSnapshot();
    String getSnapshotCsvRow(time_t time, float windspeedValue, char separationChar = ',');
    String getSnapshotFilePath(String fileType);
    String getSnapshotLogFileHeader();
    String getTimestampString(time_t time);
    String getSnapshotBaseFilePath();
};

#endif