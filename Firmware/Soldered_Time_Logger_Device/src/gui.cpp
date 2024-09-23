// Add main header file.
#include "gui.h"

#include "Inkplate.h"
#include "SourceSansPro_Regular16pt7b.h"
#include "defines.h"
#include "system.h"
#include "helpers.h"
#include "icons.h"

class Inkplate;

// Show login screen.
void login(Inkplate *_display, struct employeeData *_emp, bool *_changeNeeded, unsigned long *_menuTimeout)
{
    // Code for login screen
    // Make a sound.
    BUZZ_LOG(_display);

    // Array for the image path on the microSD card.
    char _imagePath[200];

    // Write the data on the display.
    _display->setTextSize(1);
    _display->setFont(&SourceSansPro_Regular16pt7b);
    _display->setCursor(350, 30);
    _display->clearDisplay();
    _display->print("First name:  ");
    _display->print(_emp->firstName);
    _display->setCursor(350, 60);
    _display->print("Last name:  ");
    _display->print(_emp->lastName);
    _display->setCursor(350, 90);
    _display->print("ID:  ");
    _display->print(_emp->ID);
    _display->setCursor(350, 120);
    _display->print(_emp->department);
    _display->setCursor(350, 190);
    _display->print("Login");

    // Create path to the image on the microSD card
    createImagePath((*_emp), _imagePath);

    // Try to display it. If not, use default image.
    if (!(_display->drawImage(_imagePath, 5, 5, 1, 0)))
    {
        _display->drawImage(DEFAULT_IMAGE_PATH, 5, 5, 1, 0);
    }

    // Set variables for screen update and timeout.
    *_menuTimeout = millis();
    *_changeNeeded = 1;
}

// Show logout screen.
void logout(Inkplate *_display, struct employeeData *_emp, uint32_t _dailyHours, uint32_t _weekHours , bool *_changeNeeded, unsigned long *_menuTimeout)
{
    // Code for logout screen
    // Make a sound.
    BUZZ_LOG(_display);

    // Array for the image path on the microSD card.
    char _imagePath[200];

    // Write the data on the display.
    _display->setTextSize(1);
    _display->setFont(&SourceSansPro_Regular16pt7b);
    _display->setCursor(350, 30);
    _display->clearDisplay();
    _display->print("Name:  ");
    _display->print(_emp->firstName);
    _display->setCursor(350, 60);
    _display->print("Last name:  ");
    _display->print(_emp->lastName);
    _display->setCursor(350, 90);
    _display->print("ID:  ");
    _display->print(_emp->ID);
    _display->setCursor(350, 120);
    _display->print(_emp->department);
    _display->setCursor(350, 150);
    _display->printf("Daily: %2d:%02d:%02d", _dailyHours / 3600, _dailyHours / 60 % 60, _dailyHours % 60);
    _display->setCursor(350, 180);
    _display->printf("Weekly: %2d:%02d:%02d", _weekHours / 3600, _weekHours / 60 % 60, _weekHours % 60);
    _display->setCursor(350, 250);
    _display->print("Logout");

    // Create path to the image on the microSD card
    createImagePath((*_emp), _imagePath);

    // Try to display it. If not, use default image.
    if (!(_display->drawImage(_imagePath, 5, 5, 1, 0)))
    {
        _display->drawImage(DEFAULT_IMAGE_PATH, 5, 5, 1, 0);
    }

    // Set variables for screen update and timeout.
    *_menuTimeout = millis();
    *_changeNeeded = 1;
}

// Show the screen if some one is already logged.
void alreadyLogged(Inkplate *_display, uint64_t _tag, bool *_changeNeeded, unsigned long *_menuTimeout)
{
    BUZZ_LOG_ERROR(_display);
    _display->clearDisplay();
    _display->setTextSize(3);
    _display->setFont(&SourceSansPro_Regular16pt7b);
    _display->setCursor(280, 100);
    _display->print("No can't do");
    _display->setTextSize(1);
    _display->setCursor(50, 500);
    _display->print("You already scanned your tag in last 10 minutes!");
    *_menuTimeout = millis();
    *_changeNeeded = 1;
}

// Show error screen for unknown tag.
void unknownTag(Inkplate *_display, unsigned long long _tagID, bool *_changeNeeded, unsigned long *_menuTimeout)
{
    BUZZ_LOG_ERROR(_display);
    _display->clearDisplay();
    _display->setTextSize(1);
    _display->setFont(&SourceSansPro_Regular16pt7b);
    _display->setCursor(50, 50);
    _display->print("Tag ID ");
    _display->print(_tagID);
    _display->print(" is not assigned to any employee");
    *_menuTimeout = millis();
    *_changeNeeded = 1;
}

// Show this screen if something weird happen to the logging.
void tagLoggingError(Inkplate *_display, bool *_changeNeeded, unsigned long *_menuTimeout)
{
    BUZZ_LOG_ERROR(_display);
    _display->clearDisplay();
    _display->setTextSize(1);
    _display->setFont(&SourceSansPro_Regular16pt7b);
    _display->setCursor(50, 50);
    _display->print("Logging failed! Check with the gazda for more info");
    *_menuTimeout = millis();
    *_changeNeeded = 1;
}

// This screen will appear if the microSD card is missing while loging in or loging out.
void errorDisplay(Inkplate *_display, char *_errorString, bool _clear, bool _halt)
{
    // Set font and position settings.
    _display->setTextSize(1);
    _display->setFont(&SourceSansPro_Regular16pt7b);
    if (_clear)
    {
        _display->setCursor(0, 30);
        _display->clearDisplay();
    }

    // Print error message.
    _display->print(_errorString);
    _display->partialUpdate();
    BUZZ_SYS_ERROR(_display);

    // Halt if needed.
    if (_halt) while (1);
}

// Screen when daily report is active
void dailyReportScreen(Inkplate *_display, bool *_changeNeeded, unsigned long *_menuTimeout)
{
    _display->setTextSize(1);
    _display->setFont(&SourceSansPro_Regular16pt7b);
    _display->setCursor(0, 30);
    _display->clearDisplay();
    _display->print("Creating daily report, please wait!");
    *_menuTimeout = millis();
    *_changeNeeded = 1;
}

void drawInternetInfo(Inkplate *_display)
{
    // Clear the part of the screen for the icon.
    _display->fillRect(0, 0, wifiSignal_w, wifiSignal_h, WHITE);

    if (WiFi.status() == WL_CONNECTED)
    {
        // Draw the symbol icon
        int _mappedSignal = map(WiFi.RSSI(), -127, -30, 0, 5);

        // Check bounaries.
        if (_mappedSignal < 0) _mappedSignal = 0;
        if (_mappedSignal > 5) _mappedSignal = 5;

        // Dispaly the icon.
        _display->drawBitmap(0, 0, wifiSignal[_mappedSignal], wifiSignal_w, wifiSignal_h, BLACK);
    }
    else
    {
        // Draw the icon for no wifi.
        _display->drawBitmap(0, 0, wifiSignalX, wifiSignal_w, wifiSignal_h, BLACK);
    }
}

void drawIPAddress(Inkplate *_display, int _x, int _y)
{
    // Set normal 5x7 font
    _display->setFont();

    // Set the cursor
    _display->setCursor(_x, _y);

    // Printout the IP address
    _display->print(WiFi.localIP());

    // Print also mDNS.
    _display->printf(" or %s.local/", ESP32_MDNS_NAME);
}