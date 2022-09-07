#include "helpers.h"

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
    
    if (_endWeek > _endMonth)
    {
        _endWeek -= _endWeek - _endMonth;
    }
    
    if (_startWeek < _startMonth)
    {
        _startWeek -= _startWeek - _startMonth;
    }

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

void fixHTTPResponseText(char *_c)
{
    // Try to remove escape chars from http get request and replace it with normal characters.
    // For example, R&D will be sent with HTTP GET like R%26D. %26 means 0x026, which is & char if you look at the ASCII table.
    // But there is one special escape char for space and that is '+', so this also needs to be converted.

    // First find the length of the string.
    int _n = strlen(_c);
    
    // Now go through the string and try to find '%' or '+' sign.
    for (int i = 0; i < _n; i++)
    {
        // Found '+' char? Great! Replace it with space.
        if (_c[i] == '+') _c[i] = ' ';

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