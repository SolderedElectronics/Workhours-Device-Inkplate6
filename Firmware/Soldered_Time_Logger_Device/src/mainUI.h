// Include GUI functions.
#include "gui.h"

// Include system functions.
#include "system.h"

// Include icons.
#include "icons.h"

// Class used in this file (speed up compile process).
class Inkplate;

// Clock variables (generated from Inkplate GUI).
int widget0_h = 0;
int widget0_m = 0;
int widget0_center_x = 400;
int widget0_center_y = 260;
int widget0_size = 450;
int widget0_r0 = (double)widget0_size / 2 * 0.55;
int widget0_r1 = (double)widget0_size / 2 * 0.65;
int widget0_r2 = (double)widget0_size / 2 * 0.9;
int widget0_r3 = (double)widget0_size / 2 * 1.0;
char text1_content[32] = "--[DOW]--, -- --- ----";
int text1_cursor_x = 250;
int text1_cursor_y = 550;

void mainDraw(Inkplate *_display, const char _wdayName[][10], const char _monthName[][4])
{
    // Create a string for date.
    sprintf(text1_content, "%s, %2d %.3s %04d\n", _wdayName[_display->rtcGetWeekday()], _display->rtcGetDay(),
            _monthName[_display->rtcGetMonth() - 1], _display->rtcGetYear());

    // Get the current time.
    widget0_h = _display->rtcGetHour();
    widget0_m = _display->rtcGetMinute();

    // Draw a clock.
    for (int i = 0; i < 60; ++i)
    {
        if (i % 5 == 0)
            _display->drawThickLine(widget0_center_x + widget0_r1 * cos((double)i / 60.0 * 2.0 * 3.14159265),
                                    widget0_center_y + widget0_r1 * sin((double)i / 60.0 * 2.0 * 3.14159265),
                                    widget0_center_x + widget0_r3 * cos((double)i / 60.0 * 2.0 * 3.14159265),
                                    widget0_center_y + widget0_r3 * sin((double)i / 60.0 * 2.0 * 3.14159265), 0, 3);
        else if (widget0_size > 150)
            _display->drawLine(widget0_center_x + widget0_r1 * cos((double)i / 60.0 * 2.0 * 3.14159265),
                               widget0_center_y + widget0_r1 * sin((double)i / 60.0 * 2.0 * 3.14159265),
                               widget0_center_x + widget0_r2 * cos((double)i / 60.0 * 2.0 * 3.14159265),
                               widget0_center_y + widget0_r2 * sin((double)i / 60.0 * 2.0 * 3.14159265), 2);
    }
    _display->drawThickLine(
        widget0_center_x, widget0_center_y,
        widget0_center_x + widget0_r0 * cos((double)(widget0_h - 3.0 + widget0_m / 60.0) / 12.0 * 2.0 * 3.14159265),
        widget0_center_y + widget0_r0 * sin((double)(widget0_h - 3.0 + widget0_m / 60.0) / 12.0 * 2.0 * 3.14159265), 2,
        2);

    _display->drawThickLine(widget0_center_x, widget0_center_y,
                            widget0_center_x + widget0_r2 * cos((double)(widget0_m - 15.0) / 60.0 * 2.0 * 3.14159265),
                            widget0_center_y + widget0_r2 * sin((double)(widget0_m - 15.0) / 60.0 * 2.0 * 3.14159265),
                            2, 2);

    _display->setFont(&SourceSansPro_Regular16pt7b);
    _display->setTextColor(BLACK, WHITE);
    _display->setTextSize(1);
    _display->setCursor(text1_cursor_x, text1_cursor_y);
    _display->print(text1_content);

    // Add soldered logo.
    _display->drawBitmap(bitmap3_x, bitmap3_y, bitmap3_content, bitmap3_w, bitmap3_h, BLACK);

    // Draw the icons for the WiFi
    drawInternetInfo(_display);

    // Draw the IP Adresss at the bottom of the screen.
    drawIPAddress(_display, 0, 590);
}
