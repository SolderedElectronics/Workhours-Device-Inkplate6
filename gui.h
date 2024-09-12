// Add a header guard.
#ifndef __DEVICE_GUI_H__
#define __DEVICE_GUI_H__

// Inlcude main Arduino header file.
#include <Arduino.h>

class Inkplate;

// Show login screen.
void login(Inkplate *_display, struct employeeData *_emp, bool *_changeNeeded, unsigned long *_menuTimeout);

// Show logout screen.
void logout(Inkplate *_display, struct employeeData *_emp, uint32_t _dailyHours, uint32_t _weekHours, bool *_changeNeeded, unsigned long *_menuTimeout);

// Show the screen if some one is already logged.
void alreadyLogged(Inkplate *_display, uint64_t _tag, bool *_changeNeeded, unsigned long *_menuTimeout);

// Show error screen for unknown tag.
void unknownTag(Inkplate *_display, unsigned long long _tagID, bool *_changeNeeded, unsigned long *_menuTimeout);

// Show this screen if something weird happen to the logging.
void tagLoggingError(Inkplate *_display, bool *_changeNeeded, unsigned long *_menuTimeout);

// This screen will appear if the microSD card is missing while loging in or loging out.
void errorDisplay(Inkplate *_display, char *_errorString, bool _clear, bool _halt);

// Screen when daily report is active
void dailyReportScreen(Inkplate *_display, bool *_changeNeeded, unsigned long *_menuTimeout);

void drawInternetInfo(Inkplate *_display);
void drawIPAddress(Inkplate *_display, int _x, int _y);

#endif