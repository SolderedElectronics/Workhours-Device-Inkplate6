#ifndef __DATATYPES_H__
#define __DATATYPES_H__

#include "stdlib.h"

#define DEFAULT_FOLDER_NAME "WorkingHoursDevice"
#define DEFAULT_IMAGE_NAME  "defaultImage.bmp"
#define DEFAULT_IMAGE_PATH  DEFAULT_FOLDER_NAME "/" DEFAULT_IMAGE_NAME
#define LOGGING_ERROR_STRING "??????????"

#define LOGGING_TAG_NOT_FOUND   0
#define LOGGING_TAG_LOGIN       1
#define LOGGING_TAG_LOGOUT      2
#define LOGGING_TAG_10MIN       3
#define LOGGING_TAG_ERROR       -1

// Timeout for logging - it nedds to pass as leasts this defined seconds from last loggeg time to be able to log the tag.
#define LOGGING_LOG_TIMEOUT     10

struct employeeData {
    char firstName[50];
    char lastName[50];
    char image[128];
    char department[100];
    uint64_t ID;
    struct employeeData *next;
};

#endif