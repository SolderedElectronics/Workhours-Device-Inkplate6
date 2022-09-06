#ifndef __HELPERS_H__
#define __HELPERS_H__

#include "dataTypes.h"
#include <time.h>
#include <sys\time.h>
#include <string.h>


int mcalculateMonthEpochs(int32_t _currentEpoch, int32_t *_startWeekEpoch, int32_t *_endWeekEpoch);
int calculateWeekdayEpochs(int32_t _currentEpoch, int32_t *_startWeekEpoch, int32_t *_endWeekEpoch);

#endif