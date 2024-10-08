#ifndef __HELPERS_H__
#define __HELPERS_H__

#include <Arduino.h>

class SdFile;
class WiFiClient;
class SdFile;
class SdFat;

int calculateMonthEpochs(int32_t _currentEpoch, int32_t *_startWeekEpoch, int32_t *_endWeekEpoch);
int calculateWeekdayEpochs(int32_t _currentEpoch, int32_t *_startWeekEpoch, int32_t *_endWeekEpoch);
int calculateDayEpoch(int32_t _currentEpoch, int32_t *_startDayEpoch, int32_t *_endDayEpoch);
bool monthChangeDeteced(int32_t _startEpoch, int32_t _endEpoch);
void fixHTTPResponseText(char *_c);
char *createImagePath(struct employeeData _e, char *_buffer);
void createTimeStampFromEpoch(char *_str, int32_t _epoch);
int readOneLineFromFile(SdFile *_myFile, char *_buffer, int _n);
int sendIcon(WiFiClient *_client, SdFat *_sd);
void removeEmployeeData(SdFat *_sd, employeeData *_employee);

#endif