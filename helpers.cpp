#include "helpers.h"

// Calculates the start and stop epochs of the current month
int calculateMonthEpochs(int32_t _currentEpoch, int32_t *_startWeekEpoch, int32_t *_endWeekEpoch)
{
    struct tm _myTime;
    int32_t _start;
    int32_t _end;
    const time_t myEpoch = _currentEpoch;

    memcpy(&_myTime, localtime(&myEpoch), sizeof(_myTime));

    _myTime.tm_hour = 0;
    _myTime.tm_min = 0;
    _myTime.tm_sec = 0;
    _myTime.tm_mday = 1;
    _myTime.tm_isdst = 0;

    _start = mktime(&_myTime);

    _myTime.tm_mon++;

    _end = mktime(&_myTime);
    _end--;

    *_startWeekEpoch = _start;
    *_endWeekEpoch = _end;

    return 1;
}

// NOTE: This function does not anymore handles situation when month change is in the middle of the month!
int calculateWeekdayEpochs(int32_t _currentEpoch, int32_t *_startWeekEpoch, int32_t *_endWeekEpoch)
{
    struct tm _myTime;
    int32_t _startMonth;
    int32_t _endMonth;
    const time_t myEpoch = _currentEpoch;

    memcpy(&_myTime, localtime(&myEpoch), sizeof(_myTime));

    calculateMonthEpochs(_currentEpoch, &_startMonth, &_endMonth);
    double x = (_currentEpoch - 345600) % 604800 / 86400.0;

    int32_t _startWeek = _currentEpoch - (x * 86400);
    int32_t _endWeek = _currentEpoch + ((6 - x + 1) * 86400) - 1;

    *_startWeekEpoch = _startWeek;
    *_endWeekEpoch = _endWeek;

    return 1;
}

int calculateDayEpoch(int32_t _currentEpoch, int32_t *_startDayEpoch, int32_t *_endDayEpoch)
{
    struct tm _myTime;
    int32_t _startDay;
    int32_t _endDay;
    const time_t myEpoch = _currentEpoch;

    memcpy(&_myTime, localtime(&myEpoch), sizeof(_myTime));

    _myTime.tm_hour = 0;
    _myTime.tm_min = 0;
    _myTime.tm_sec = 0;
    _myTime.tm_isdst = 0;

    _startDay = mktime(&_myTime);

    _myTime.tm_mday++;

    _endDay = mktime(&_myTime) - 1;

    *_startDayEpoch = _startDay;
    *_endDayEpoch = _endDay;

    return 1;
}


// Check if there is a month has changed in a period between two epochs.
bool monthChangeDeteced(int32_t _startEpoch, int32_t _endEpoch)
{
    // Variables for storing month of both epochs.
    int _m1, _m2;

    // Create struct for time and date.
    struct tm _t;

    // Clear the whole struct for time and date.
    memset(&_t, 0, sizeof(_t));

    // Save the epochs in time_t variable.
    const time_t _time1 = _startEpoch;
    const time_t _time2 = _endEpoch;

    // Convert epoch timestamp of the start of the week into human readable time and date.
    memcpy(&_t, localtime(&_time1), sizeof(_t));

    // Save the month
    _m1 = _t.tm_mon;

    // Do the same, but now for the _endEpoch.
    memset(&_t, 0, sizeof(_t));
    memcpy(&_t, localtime(&_time2), sizeof(_t));
    _m2 = _t.tm_mon;

    // Retrun if the month has changed (true if it is, false if not).
    return (_m1 != _m2 ? true : false);
}

void fixHTTPResponseText(char *_c)
{
    // Try to remove escape chars from http get request and replace it with normal characters.
    // For example, R&D will be sent with HTTP GET like R%26D. %26 means 0x026, which is '&' char if you look at the
    // ASCII table. But there is one special escape char for space and that is '+', so this also needs to be converted.

    // First find the length of the string.
    int _n = strlen(_c);

    // Now go through the string and try to find '%' or '+' sign.
    for (int i = 0; i < _n; i++)
    {
        // Found '+' char? Great! Replace it with space.
        if (_c[i] == '+')
            _c[i] = ' ';

        // Things are little bit more complicated with the '%' sign...
        if (_c[i] == '%')
        {
            // If you find the '%' sign, try to get the HEX value of it.
            int _escapeChar = 0;

            // If everything went ok, now move two chars to the left.
            if (sscanf(_c + i, "%%%2x", &_escapeChar))
            {
                memmove(&_c[i], &_c[i + 2], _n - i);

                // Add decoded char back to the string.
                _c[i] = _escapeChar;

                // Done! Easy right? :)
            }
        }
    }
}

char *createImagePath(struct employeeData _e, char *_buffer)
{
    // Check if the buffer address if valid, if not, return error (NULL).
    if (_buffer == NULL)
        return NULL;

    // Create path with sprintf.
    sprintf(_buffer, "/%s/%s%s%llu/%s", DEFAULT_FOLDER_NAME, _e.firstName, _e.lastName, (unsigned long long)_e.ID,
            _e.image);

    // Return the buffer address.
    return _buffer;
}

void createTimeStampFromEpoch(char *_str, int32_t _epoch)
{
    // Check if the pointer is valid, if not, return error.
    if (_str == NULL)
        return;

    // Struct for human human readable time and date
    struct tm _myTime;

    // Convert epoch to human time and date
    const time_t _time = _epoch;
    memcpy(&_myTime, localtime(&_time), sizeof(_myTime));

    // Make an timestamp in format HH:MM:SS DD.MM.YYYYY.
    sprintf(_str, "%02d:%02d:%02d %02d.%02d.%04d.", _myTime.tm_hour, _myTime.tm_min, _myTime.tm_sec, _myTime.tm_mday,
            _myTime.tm_mon + 1, _myTime.tm_year + 1900);
}

int readOneLineFromFile(SdFile *_myFile, char *_buffer, int _n)
{
    // Check if the buffer address is a vaild address. If not, return 0.
    if (_buffer == NULL)
        return 0;

    // Check if the address for the file is vaild. Again, if not, retrun an error.
    if (_myFile == NULL)
        return 0;

    // Check if the max size of the one line valid. If not, retrun an error.
    if (_n <= 0)
        return 0;

    // Go trough the file and try to find new line or EOF and copy every char into buffer while doing that.
    char c = 0;
    int k = 0;
    while ((c != '\n') && (_myFile->available()) && (k < _n))
    {
        c = _myFile->read();
        _buffer[k++] = c;
    }

    // Add terminating char at the end of the string.
    _buffer[k] = 0;

    return 1;
}

int sendIcon(WiFiClient *_client, SdFat *_sd)
{
    SdFile _myFile;
    uint8_t *_buffer;

    // Try to init the microSD card. If failed, return 0.
    if (!_sd->begin(15, SD_SCK_MHZ(10)))
    {
        return 0;
    }

    // Change dir to working directory of this device
    if (!_sd->chdir(DEFAULT_FOLDER_NAME))
    {
        return 0;
    }

    // Now try to open favicon.ico file
    if (!_myFile.open("favicon.ico"))
    {
        return 0;
    }

    // Allocate  memory for the favicon file.
    _buffer = (uint8_t *)ps_malloc(_myFile.fileSize());
    if (_buffer == NULL)
        return 0;

    // Read it!
    _myFile.read(_buffer, _myFile.fileSize());

    // Send it to the client
    _client->write(_buffer, _myFile.fileSize());

    // Close the file
    _myFile.close();

    // Free the memory
    free(_buffer);

    // Go back to the root dir
    _sd->chdir(true);

    // Return succ
    return 1;
}

void removeEmployeeData(SdFat *_sd, employeeData *_employee)
{
    SdFile _myFile;
    uint8_t *_buffer;

    // Try to init the microSD card. If failed, return 0.
    if (!_sd->begin(15, SD_SCK_MHZ(10)))
    {
        return;
    }

    // Go back to the root
    _sd->chdir(true);

    // Open working folder of this device
    _sd->chdir(DEFAULT_FOLDER_NAME, true);

    // Make folder name
    char _folderName[250];
    snprintf(_folderName, sizeof(_folderName), "%s%s%llu", _employee->firstName, _employee->lastName,
             (unsigned long long)(_employee->ID));

    Serial.print(_folderName);

    // Remove the folder
    _myFile.open(_folderName);
    if (!_myFile.rmRfStar())
    {
        Serial.println("Could not remove the folder!");
    }

    // Close the file.
    _myFile.close();

    // Go back to the root
    _sd->chdir(true);

    return;
}

void drawInternetInfo(Inkplate *_ink)
{
    // Clear the part of the screen for the icon.
    _ink->fillRect(0, 0, noWifiSymbol_w, noWifiSymbol_h, WHITE);
    _ink->fillRect(60, 0, wifiSignal_w, wifiSignal_h, WHITE);

    if (WiFi.status() == WL_CONNECTED)
    {
        // Draw the icon for wifi.
        _ink->drawBitmap(0, 0, wifiSymbol, wifiSymbol_w, wifiSymbol_h, BLACK);

        // Draw the symbol icon
        int _mappedSignal = map(WiFi.RSSI(), -127, 0, 0, 5);
        _ink->drawBitmap(60, 0, wifiSignal[_mappedSignal], wifiSignal_w, wifiSignal_h, BLACK);
    }
    else
    {
        // Draw the icon for no wifi.
        _ink->drawBitmap(0, 0, noWifiSymbol, noWifiSymbol_w, noWifiSymbol_h, BLACK);
    }
}

void drawIPAddress(Inkplate *_ink, int _x, int _y)
{
    // Set normal 5x7 font
    _ink->setFont();

    // Set the cursor
    _ink->setCursor(_x, _y);

    // Printout the IP address
    _ink->print(WiFi.localIP());
}