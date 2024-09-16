#ifndef __DEVICE_DEFINES_H__
#define __DEVICE_DEFINES_H__

#include "stdlib.h"

// Secret key used for adding or deleting employees
#define SECRET_KEY "password12"
// Maximum is 49 chars long...just in case! :)
#define SECRET_KEY_LEN 10

// How long to keep menu active
#define LOG_SCREEN_TIME 10000UL

// Folder names and structure on micro SD Card
#define DEFAULT_FOLDER_NAME "SolderedTimeLoggerDevice"
#define DEFAULT_IMAGE_NAME  "defaultImage.bmp"
#define DEFAULT_IMAGE_PATH  DEFAULT_FOLDER_NAME "/" DEFAULT_IMAGE_NAME

// All departments. Add if needed. Do not modify alreday added.
static const char *departments[] = {"Engineering", "Manufacturing", "Operations", "Sales & Marketing", "Warehouse & Purchasing", "Finance"};
static const int numberOfDepartments = sizeof(departments) / sizeof(departments[0]);

// mDNS - Name of the ESP32 on the localhost.
#define ESP32_MDNS_NAME "timeloggerdevice"

// Change WiFi and IP data to suit your setup.
static char ssid[] = "Soldered-testingPurposes";
static char pass[] = "Testing443";
// Set your Static IP address
static IPAddress localIP(192, 168, 71, 98); // IP address should be set to desired address
// Set your Gateway IP address
// Gateway address (in most cases it's the first address of selected IP addreess subnet)
static IPAddress gateway(192, 168, 71, 1);
static IPAddress subnet(255, 255, 255, 0);   // Subnet mask
static IPAddress primaryDNS(192, 168, 71, 1); // Primary DNS (use router / ISP DNS as primary one)
static IPAddress secondaryDNS(8, 8, 8, 8);   // Secondary DNS (use google as secondary one)

// Font. Change if needed (used by the web interface).
#define FONT_FILENAME "SourceSansPro-Regular.ttf"
#define FONT_NAME     "SourceSansPro"
#define FONT_TYPE     "Regular"
#define FONT_FORMAT   "opentype"

// API for the clock adjustment.
// Full list see here: https://worldtimeapi.org/timezones.
#define API_CLOCK_CONTINENT     "Europe"
#define API_CLOCK_REGION        "Zagreb"

// Tag for missed logout in the list.
#define LOGGING_ERROR_STRING "??????????"

// String for the header in the RAW login and logout list
#define LOGGING_RAW_FILE_HEADER "LOGIN EPOCH,LOGOUT EPOCH"

// Login / Logout error tags
#define LOGGING_TAG_NOT_FOUND 0
#define LOGGING_TAG_LOGIN     1
#define LOGGING_TAG_LOGOUT    2
#define LOGGING_TAG_10MIN     3
#define LOGGING_TAG_ERROR     -1

// Chars for Month and weekday names
const char wdayName[7][10] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char monthName[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// Maximal worktime in the week. First element is sunday.
// For example, first element in the array represents sunday and in sunday every working hours is overtime.
// But for monday (second element in the list) employee can work 9 hours, anything above that is overtime.
const int defaultWeekWorkHours[] = {0, 9, 9, 9, 9, 4, 0};

// Timeout for logging - it nedds to pass as leasts this defined seconds from last loggeg time to be able to log the
// tag.
#define LOGGING_LOG_TIMEOUT 600

// Define different types of buzzing sounds
#define BUZZ_ERROR_QUICK(x) buzzer(x, 2, 150);
#define BUZZ_SYS_ERROR(x)   buzzer(x, 4, 500);
#define BUZZ_LOG(x)         buzzer(x, 1, 500);
#define BUZZ_LOG_ERROR(x)   buzzer(x, 3, 50);

// Define buzzer pin on the I/O expander
#define BUZZER_PIN IO_PIN_B7


// Struct that holds data of the every employee. Can be changed if needed, but the whole code must be updated.
struct employeeData
{
    char firstName[50];
    char lastName[50];
    char image[128];
    char department[100];
    uint64_t ID;
    struct employeeData *next;
};

// Use this for debuging.
#define DEBUG_PRINT(...) { Serial.printf("[DEBUG from %s() ]", __func__); Serial.printf(__VA_ARGS__); Serial.println(); }

#endif