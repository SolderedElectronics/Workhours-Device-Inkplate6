#ifndef __HELPERS_H__
#define __HELPERS_H__

#include "Arduino.h"
#include "Inkplate.h"
#include "dataTypes.h"
#include <stdio.h>
#include <string.h>
#include <sys\time.h>
#include <time.h>

int mcalculateMonthEpochs(int32_t _currentEpoch, int32_t *_startWeekEpoch, int32_t *_endWeekEpoch);
int calculateWeekdayEpochs(int32_t _currentEpoch, int32_t *_startWeekEpoch, int32_t *_endWeekEpoch);
int calculateDayEpoch(int32_t _currentEpoch, int32_t *_startDayEpoch, int32_t *_endDayEpoch);
void fixHTTPResponseText(char *_c);
char *createImagePath(struct employeeData _e, char *_buffer);
void createTimeStampFromEpoch(char *_str, int32_t _epoch);
int readOneLineFromFile(SdFile *_myFile, char *_buffer, int _n);

#endif