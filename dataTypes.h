#ifndef __DATATYPES_H__
#define __DATATYPES_H__

#include "stdlib.h"

// Folder names and structure on micro SD Card
#define DEFAULT_FOLDER_NAME "WorkingHoursDevice"
#define DEFAULT_IMAGE_NAME  "defaultImage.bmp"
#define DEFAULT_IMAGE_PATH  DEFAULT_FOLDER_NAME "/" DEFAULT_IMAGE_NAME

// Tag for missed logout in the list.
#define LOGGING_ERROR_STRING "??????????"

// Login / Logout error tags
#define LOGGING_TAG_NOT_FOUND   0
#define LOGGING_TAG_LOGIN       1
#define LOGGING_TAG_LOGOUT      2
#define LOGGING_TAG_10MIN       3
#define LOGGING_TAG_ERROR       -1

// Chars for Month and weekday names
const char wdayName[7][10] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char monthName[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// Maximal worktime in the week. First element is sunday.
// For example, first element in the array represents sunday and in sunday every working hours is overtime.
// But for monday (second element in the list) employee can work 9 hours, anything above that is overtime.
const int overtimeHours[] = {0, 9, 9, 9, 9, 4, 0};

// Timeout for logging - it nedds to pass as leasts this defined seconds from last loggeg time to be able to log the tag.
#define LOGGING_LOG_TIMEOUT     10

// Struct that holds data of the every employee. Can be changed if needed, but the whole code must be updated.
struct employeeData {
    char firstName[50];
    char lastName[50];
    char image[128];
    char department[100];
    uint64_t ID;
    struct employeeData *next;
};

#endif