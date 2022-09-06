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