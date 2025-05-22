#include "WindSpeed.h"

WindSpeed::WindSpeed(uint8_t sensorPin, uint16_t windspeedLowerThreshold, uint16_t windspeedUpperThreshold, uint16_t windspeedDurationRange, uint16_t evaluationRange, uint16_t numberOfWindowsThreshold, uint16_t calibrationFactor)
{
    _sensorPin = sensorPin;
    pinMode(_sensorPin, INPUT_PULLUP);
    _evaluationRange = evaluationRange;
    _windspeedLowerThreshold = windspeedLowerThreshold;
    _windspeedUpperThreshold = windspeedUpperThreshold;
    _windspeedDurationRange = windspeedDurationRange;
    _numberOfWindowsThreshold = numberOfWindowsThreshold;
    _calibrationFactor = calibrationFactor;
}

void WindSpeed::setup()
{
    setupSDCard();
}

void WindSpeed::updateSettings(uint16_t windspeedLowerThreshold, uint16_t windspeedUpperThreshold, uint16_t windspeedDurationRange, uint16_t evaluationRange, uint16_t numberOfWindowsThreshold, uint16_t calibrationFactor)
{
    _windspeedLowerThreshold = windspeedLowerThreshold;
    _windspeedUpperThreshold = windspeedUpperThreshold;
    _windspeedDurationRange = windspeedDurationRange;
    _evaluationRange = evaluationRange;
    _numberOfWindowsThreshold = numberOfWindowsThreshold;
    _calibrationFactor = calibrationFactor;
}

void WindSpeed::setupSDCard()
{
    if (!SD.begin(GPIO_NUM_4, SPI, 25000000))
    {
        Serial.println("Card Mount Failed");
        return;
    }
    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE)
    {
        Serial.println("No SD card attached");
        return;
    }

    Serial.print("SD Card Type: ");
    if (cardType == CARD_MMC)
    {
        Serial.println("MMC");
    }
    else if (cardType == CARD_SD)
    {
        Serial.println("SDSC");
    }
    else if (cardType == CARD_SDHC)
    {
        Serial.println("SDHC");
    }
    else
    {
        Serial.println("UNKNOWN");
    }
    Serial.printf("SD Card Size: %lluMB\n", SD.cardSize() / (1024 * 1024));
    Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
    Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));

    createDir(SD, "/logs");
}

void WindSpeed::setupInterruptCallback(void (*externalInterruptCallback)(void))
{
    attachInterrupt(digitalPinToInterrupt(_sensorPin), externalInterruptCallback, RISING);
}

void WindSpeed::setupEvaluationCallback(std::function<void(void)> evaluationCallback)
{
    _evaluationCallback = evaluationCallback;
}

// interrupt callback function for impuls counter of windspeed sensor
void WindSpeed::interruptCallback()
{
    _counter++;
}

// windspeed in m/s
void WindSpeed::calculateWindspeed(bool evaluate, bool log)
{
    float windspeed = ((float)(_counter - _lastCounter) / 20.0f * 1.75f * 1000 / _sampleRate);
    _lastCounter = _counter;
    updateWindspeedArray(windspeed);
    if (evaluate)
    {
        evaluateWindspeed();
    }
    if (log)
    {
        logWindspeedToSDCard(SD);
    }
}

float WindSpeed::getCurrentWindspeed()
{
    float currentWindspeed = ((float)_windspeedHistoryArray[0] / 10.0f);
    return currentWindspeed;
}

WindspeedEvaluation WindSpeed::getWindspeedEvaluation()
{
    return _windspeedEvaluation;
}

int WindSpeed::getWindSpeedHistoryArrayElement(int i)
{
    return _windspeedHistoryArray[i];
}

void WindSpeed::evaluateWindspeed()
{

    int maxWindspeed = 0;
    int minWindspeed = INT_MAX;
    long sumWindspeed = 0;

    int rangeCounter = 0;
    int exceededRangesCounter = 0;
    int numberOfRanges = _evaluationRange / _windspeedDurationRange;
    int exceededRangesIndex[numberOfRanges];

    for (size_t i = 0; i < 30; i++)
    {
        _windspeedEvaluation.RangeStartIndex[i] = 0;
        _windspeedEvaluation.RangeStopIndex[i] = 0;
    }

    for (size_t i = 0; i < numberOfRanges; i++)
    {
        exceededRangesIndex[i] = 0;
    }

    for (size_t i = _evaluationRange; i > 0; --i)
    {
        if (_windspeedHistoryArray[i] > maxWindspeed)
        {
            maxWindspeed = _windspeedHistoryArray[i];
        }

        if (_windspeedHistoryArray[i] < minWindspeed)
        {
            minWindspeed = _windspeedHistoryArray[i];
        }
        sumWindspeed += _windspeedHistoryArray[i];

        if (_windspeedHistoryArray[i] < _windspeedLowerThreshold * 10 
            || _windspeedHistoryArray[i] > _windspeedUpperThreshold * 10)
        {
            rangeCounter++;
        }

        if (rangeCounter == _windspeedDurationRange)
        {
            exceededRangesIndex[exceededRangesCounter] = i;
            exceededRangesCounter++;
            rangeCounter = 0;
        }

        if (_windspeedHistoryArray[i] >= _windspeedLowerThreshold * 10 
            && _windspeedHistoryArray [i] <= _windspeedUpperThreshold * 10)
        {
            rangeCounter = 0;
        }
    }

    _windspeedEvaluation.NumberOfExceededRanges = exceededRangesCounter;
    _windspeedEvaluation.MaxWindspeed = (float)maxWindspeed / 10.0f;
    _windspeedEvaluation.MinWindspeed = (float)minWindspeed / 10.0f;
    _windspeedEvaluation.AverageWindspeed = (float)(sumWindspeed / _evaluationRange) / 10.0f;
    for (size_t i = 0; i < numberOfRanges; i++)
    {
        _windspeedEvaluation.RangeStartIndex[i] = exceededRangesIndex[i];
        _windspeedEvaluation.RangeStopIndex[i] = exceededRangesIndex[i] + _windspeedDurationRange;
    }

    if (exceededRangesCounter < _numberOfWindowsThreshold)
    {
        _isCallbackAlreadySent = false;
    }

    if (exceededRangesCounter >= _numberOfWindowsThreshold && !_isCallbackAlreadySent && _evaluationCallback != nullptr)
    {
        _isCallbackAlreadySent = true;
        _evaluationCallback();
        storeSnapshot();
    }
}

String WindSpeed::getWindspeedString(bool addUnitSymbol)
{
    char stringbuffer[100];
    if (addUnitSymbol)
    {
        sprintf(stringbuffer, "%.1f m/s  ", getCurrentWindspeed());
    }
    else
    {
        sprintf(stringbuffer, "%.1f", getCurrentWindspeed());
    }
    return String(stringbuffer);
}

String WindSpeed::getWindspeedEvaluationSingleString(float windspeedValue)
{
    char stringbuffer[100];
    sprintf(stringbuffer, "%.1f", windspeedValue);
    return String(stringbuffer);
}

void WindSpeed::logWindspeedToSDCard(fs::FS &fs)
{
    if (!fs.exists(getLogFilePath().c_str()))
    {
        writeLineToFile(fs, getLogFilePath().c_str(), getLogFileHeader().c_str());
    }
    appendLineToFile(fs, getLogFilePath().c_str(), getLogCsvRow().c_str());
}

String WindSpeed::getWindspeedEvaluationString()
{
    return "MAX:" + getWindspeedEvaluationSingleString(_windspeedEvaluation.MaxWindspeed) + " MIN:" + getWindspeedEvaluationSingleString(_windspeedEvaluation.MinWindspeed) + " AVG:" + getWindspeedEvaluationSingleString(_windspeedEvaluation.AverageWindspeed);
}

String WindSpeed::getTimestampString()
{
    time_t t = now();
    return getTimestampString(t);
}

String WindSpeed::getTimestampString(time_t time)
{
    char stringbuffer[100];
    sprintf(stringbuffer, "%4u-%02u-%02u %02u:%02u:%02u", year(time), month(time), day(time), hour(time), minute(time), second(time));
    return String(stringbuffer);
}

void WindSpeed::storeCsvSnapshot()
{
    String csvFilePath = getSnapshotFilePath("csv");
    time_t time = now();
    String content;
    content = getSnapshotLogFileHeader() + String("\r\n");
    for (size_t i = 0; i < _evaluationRange; i++)
    {
        content += getSnapshotCsvRow(time - _evaluationRange + 1 + i, _windspeedHistoryArray[_evaluationRange - 1 - i] / 10.0f, ',') + String("\r\n");
    }
    appendFile(SD, csvFilePath.c_str(), content.c_str());
}

void WindSpeed::storeJsonSnapshot()
{
    String jsonFilePath = getSnapshotFilePath("json");
    writeFile(SD, jsonFilePath.c_str(), getWindspeedJson().c_str());
}

void WindSpeed::storeJsonEvaluationSnapshot()
{
    String jsonEvaluationFilePath = getSnapshotBaseFilePath() + "_evaluation.json";
    writeFile(SD, jsonEvaluationFilePath.c_str(), getWindspeedEvaluationJson().c_str());
}

void WindSpeed::storeSnapshot()
{
    storeJsonSnapshot();
    storeJsonEvaluationSnapshot();
    storeCsvSnapshot();
}

String WindSpeed::getWindspeedJson()
{
    JsonDocument jsonDocument;

    for (size_t i = 0; i < _evaluationRange; i++)
    {
        JsonObject arrayDocument = jsonDocument.add<JsonObject>();
        arrayDocument["x"] = i;
        arrayDocument["y"] = _windspeedHistoryArray[_evaluationRange - 1 - i] / 10.0f;
    }

    String jsonString;
    jsonDocument.shrinkToFit();
    serializeJson(jsonDocument, jsonString);

    return jsonString;
}

String WindSpeed::getWindspeedEvaluationJson()
{
    WindspeedEvaluation windspeedEvaluation = getWindspeedEvaluation();

    JsonDocument jsonDocument;

    jsonDocument["Current"] = getCurrentWindspeed();
    jsonDocument["Min"] = windspeedEvaluation.MinWindspeed;
    jsonDocument["Max"] = windspeedEvaluation.MaxWindspeed;
    jsonDocument["Average"] = windspeedEvaluation.AverageWindspeed;

    JsonArray exceededRanges = jsonDocument["ExceededRanges"].to<JsonArray>();
    for (size_t i = 0; i < windspeedEvaluation.NumberOfExceededRanges; i++)
    {
        JsonObject exceedingRange = exceededRanges.add<JsonObject>();
        exceedingRange["RangeIndex"] = i;
        exceedingRange["StartIndex"] = windspeedEvaluation.RangeStartIndex[i];
        exceedingRange["StopIndex"] = windspeedEvaluation.RangeStopIndex[i];
    }

    String jsonString;
    jsonDocument.shrinkToFit();
    serializeJson(jsonDocument, jsonString);
    return jsonString;
}

String WindSpeed::getSnapshotCsvRow(time_t time, float windspeedValue, char separationChar)
{
    return getTimestampString(time) + separationChar + getWindspeedEvaluationSingleString(windspeedValue);
}

String WindSpeed::getLogCsvRow(char separationChar)
{
    return getTimestampString() + separationChar + getWindspeedString() + separationChar + String(M5.Power.getBatteryLevel()) + separationChar + String(M5.Power.getBatteryVoltage());
}

String WindSpeed::getSnapshotBaseFilePath()
{
    time_t t = now();
    char stringbuffer[100];
    sprintf(stringbuffer, "/logs/%4u-%02u-%02u_%02u-%02u-%02u_windspeed_snapshot", year(t), month(t), day(t), hour(t), minute(t), second(t));
    return String(stringbuffer);
}

String WindSpeed::getSnapshotFilePath(String fileType)
{
    return getSnapshotBaseFilePath() + "." + fileType;
}

String WindSpeed::getSnapshotLogFileHeader()
{
    return "Timestamp(UTC), Windspeed[m/s]";
}

String WindSpeed::getLogFilePath()
{
    time_t t = now();
    char stringbuffer[100];
    sprintf(stringbuffer, "/logs/%4u-%02u-%02u_windspeed.csv", year(t), month(t), day(t));
    return String(stringbuffer);
}

String WindSpeed::getLogFileHeader()
{
    return "Timestamp(UTC), Windspeed[m/s], BatteryLevel[%], BatteryVoltage[mV]";
}

void WindSpeed::updateWindspeedArray(float currentWindspeed)
{
    for (size_t i = _evaluationRange; i > 0; --i)
    {
        _windspeedHistoryArray[i] = _windspeedHistoryArray[i - 1];
    }
    int calculatedWindspeed = (int)(currentWindspeed * 10.0f);
    _windspeedHistoryArray[0] = calculatedWindspeed;
}

void WindSpeed::createDir(fs::FS &fs, const char *path)
{
    Serial.printf("Creating Dir: %s\n", path);
    if (fs.mkdir(path))
    {
        Serial.println("Dir created");
    }
    else
    {
        Serial.println("mkdir failed");
    }
}

void WindSpeed::readFile(fs::FS &fs, const char *path)
{
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if (!file)
    {
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.print("Read from file: ");
    while (file.available())
    {
        Serial.write(file.read());
    }
    file.close();
}

void WindSpeed::writeFile(fs::FS &fs, const char *path, const char *message)
{
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if (!file)
    {
        Serial.println("Failed to open file for writing");
        return;
    }
    if (file.print(message))
    {
        Serial.println("File written");
    }
    else
    {
        Serial.println("Write failed");
    }
    file.close();
}

void WindSpeed::writeLineToFile(fs::FS &fs, const char *path, const char *message)
{
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if (!file)
    {
        Serial.println("Failed to open file for writing");
        return;
    }
    if (file.println(message))
    {
        Serial.println("File written");
    }
    else
    {
        Serial.println("Write failed");
    }
    file.close();
}

void WindSpeed::appendFile(fs::FS &fs, const char *path, const char *message)
{
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if (!file)
    {
        Serial.println("Failed to open file for appending");
        return;
    }
    if (file.print(message))
    {
        Serial.println("Message appended");
    }
    else
    {
        Serial.println("Append failed");
    }
    file.close();
}

void WindSpeed::appendLineToFile(fs::FS &fs, const char *path, const char *message)
{

    File file = fs.open(path, FILE_APPEND);
    if (!file)
    {
        Serial.println("Failed to open file for appending");
        return;
    }

    if (!file.println(message))
    {
        Serial.println("Append failed");
    }
    file.close();
}
