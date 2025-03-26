#include "WindSpeed.h"

WindSpeed::WindSpeed(uint8_t sensorPin, uint16_t evaluationRange, uint16_t windspeedThreshold, uint16_t windspeedDurationRange)
{
    _sensorPin = sensorPin;
    pinMode(_sensorPin, INPUT_PULLUP);
    _evaluationRange = evaluationRange;
    _windspeedThreshold = windspeedThreshold;
    _windspeedDurationRange = windspeedDurationRange;
}

void WindSpeed::setup() 
{
    setupSDCard();
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
    float windspeed = ((float)(_counter-_lastCounter) / 20.0f * 1.75f * 1000 / _sampleRate);
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
    return (float)_windspeedHistoryArray[0]/10.0f;
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
    int exceededRangesIndex[15];

    for (size_t i = 0; i < 15; i++)
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

        if (_windspeedHistoryArray[i] > _windspeedThreshold * 10)
        {
            rangeCounter++;
        }

        if (rangeCounter == _windspeedDurationRange)
        {
            exceededRangesIndex[exceededRangesCounter] = i;
            exceededRangesCounter++;
            rangeCounter = 0;
        }

        if (_windspeedHistoryArray[i] <= _windspeedThreshold * 10)
        {
            rangeCounter = 0;
        }
    }

    _windspeedEvaluation.NumberOfExceededRanges = exceededRangesCounter;
    _windspeedEvaluation.MaxWindspeed = (float)maxWindspeed / 10.0f;
    _windspeedEvaluation.MinWindspeed = (float)minWindspeed / 10.0f;
    _windspeedEvaluation.AverageWindspeed = (float)(sumWindspeed / _evaluationRange) / 10.0f;
    for (size_t i = 0; i < 15; i++)
    {
        _windspeedEvaluation.RangeStartIndex[i] = exceededRangesIndex[i];
        _windspeedEvaluation.RangeStopIndex[i] = exceededRangesIndex[i] + _windspeedDurationRange;
    }

    if (exceededRangesCounter < _numberOfRangesThreshold) {
        //Serial.println("Reset isCallbackAlreadySent");
        _isCallbackAlreadySent = false;
    }

        if (exceededRangesCounter >= _numberOfRangesThreshold && !_isCallbackAlreadySent && _evaluationCallback != nullptr)
        {
            _isCallbackAlreadySent = true;
            _evaluationCallback();
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
    appendLineToFile(fs, getLogFilePath().c_str(), getLogCsvRow(getCurrentWindspeed()).c_str());
}

String WindSpeed::getWindspeedEvaluationString()
{
    return "MAX:" + getWindspeedEvaluationSingleString(_windspeedEvaluation.MaxWindspeed) + " MIN:" + getWindspeedEvaluationSingleString(_windspeedEvaluation.MinWindspeed) + " AVG:" + getWindspeedEvaluationSingleString(_windspeedEvaluation.AverageWindspeed);
}

String WindSpeed::getTimestampString()
{
    time_t t = now();
    char stringbuffer[100];
    sprintf(stringbuffer, "%4u-%02u-%02u %02u:%02u:%02u", year(t), month(t), day(t), hour(t), minute(t), second(t));
    return String(stringbuffer);
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

    jsonDocument["currentWindspeed"] = getCurrentWindspeed();
    jsonDocument["minWindspeed"] = windspeedEvaluation.MinWindspeed;
    jsonDocument["maxWindspeed"] = windspeedEvaluation.MaxWindspeed;
    jsonDocument["averageWindspeed"] = windspeedEvaluation.AverageWindspeed;
    jsonDocument["numberOfExceededRanges"] = windspeedEvaluation.NumberOfExceededRanges;

    for (size_t i = 0; i < _evaluationRange; i++)
    {
        JsonObject dataArray = jsonDocument["data"].createNestedObject();
        dataArray["x"] = i;
        dataArray["y"] = _windspeedHistoryArray[_evaluationRange - 1 - i] / 10.0f;
    }

    if (windspeedEvaluation.NumberOfExceededRanges == 0) {
        JsonObject exceededRangeObject = jsonDocument["exceededRanges"].createNestedObject();
    }

    for (size_t i = 0; i < windspeedEvaluation.NumberOfExceededRanges; i++)
    {
        JsonObject exceededRangeObject = jsonDocument["exceededRanges"].createNestedObject();
        exceededRangeObject["RangeIndex"] = i;
        exceededRangeObject["StartIndex"] = windspeedEvaluation.RangeStartIndex[i];
        exceededRangeObject["StopIndex"] = windspeedEvaluation.RangeStopIndex[i];
    }

    String jsonString;
    jsonDocument.shrinkToFit();
    serializeJson(jsonDocument, jsonString);
    return jsonString;
}

String WindSpeed::getLogCsvRow(float windspeedValue, char separationChar)
{
    return getTimestampString() + separationChar + getWindspeedString(windspeedValue);
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
    return "Timestamp, Windspeed[m/s]";
}


void WindSpeed::updateWindspeedArray(float currentWindspeed)
{
    for (size_t i = _evaluationRange; i > 0; --i)
    {
        _windspeedHistoryArray[i] = _windspeedHistoryArray[i - 1];
    }
    _windspeedHistoryArray[0] = (int)(currentWindspeed * 10.0f);
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
