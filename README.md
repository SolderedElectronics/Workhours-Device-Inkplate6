# Soldered Time Logger Device
Soldered Inkplate 6 based time logger device for logging work hours data of employees.

![Completed Device](extras/images/completedDevice.jpg)

![Device](extras/images/deviceImage.jpg)

## Features:
- Storing login and logout times of every employee on microSD card in .cvs format
- Allows for multiple login / logout in a one day
- Automatically calculating and creating daily report at 10 seconds before midnight at the and of the day for each employee
- Automatically calculates overtinem and stores them in report
- Forgotten logout indicator in report (if employee forgot to do a logout)
- Displaying employee image while login and logout (**must** be on microSD card, PNG format 300x300 px)
- Showing accumulated week and daily time while logout
- Managing employees through web interface
- Managing employee reports trough web interface
- mDNS support
- Automatic clock adjustment through Web API
- Automatic periodic clock adjustmet
- WiFi status indicator
- [JSON APIs](README.md?tab=readme-ov-file#api)

## Requirements:
- microSD card with loaded data from microSD card folder
- Soldered RFID Reader with multiple tags for each employee
- Magnetic buzzer
- Installed Inkplate Board Definition (v8.1.0)
- Installed Inkplate Library (v9.1.0)
- Installed ArduinoJSON library (v7.0.4)
- 5V USB Supply
- 3D printed Case (optional)
- Stable WiFi connection
- API Key from https://timezonedb.com/api. It's free!

## Getting started
- Download this repo and open -ino file (Arduino, VSCode,...)
- Add timezonedb API key for time in defined.h under `API_KEY_TIME`. Otherwise, device won't work!
- Change settings in src/defines.h (if needed)
  - Web Server password in `SECRET_KEY` as well as length of the password in `SECRET_KEY_LEN`
  - Name of each department in `departments`. Must be at least one department!
  - default folder name on microSD card in `DEFAULT_FOLDER_NAME`
  - default image name (if one does not exists in employee folder) in `DEFAULT_IMAGE_NAME`
  - default image path in `DEFAULT_IMAGE_PATH`
  - Web Server (mDNS) name in `ESP32_MDNS_NAME`
  - Font names and files in `FONT_FILENAME` `FONT_NAME`, `FONT_TYPE` and `FONT_FORMAT`
  - Missed logout tag in login/logout list in `LOGGING_ERROR_STRING`
  - Weekday names in `wdayName`
  - Month names in `monthName`
  - Default work time for every day in a week (in hours) in `defaultWeekWorkHours` for overtime calculation
  - login/logout "cooldown" time in `LOGGING_LOG_TIMEOUT`
- Create folder name same as `DEFAULT_FOLDER_NAME` (watch out, it's case sensitive!) and add all files from microSdCardPrep in this directory.
- In src/defines.h change WiFi SSID and WiFi Password
- Also in src/defines.h change IP address settings. Note: secondary DNS IP Address if Google DNS.

## Connections
Detail assembly manual can be found [here](Assembly/README.md).

Two additional parts must be connected to the logging device; RFID breakout and buzzer.
### RFID
|Soldered Inkplate 6|Soldered 125kHz RFID (UART)|Wire color|
|-------------------|---------------------------|----------|
|ESP32 GPIO39 (S_VN)|TXD                        |Yellow    |
|Inkplate GND Pad   |GND                        |Black     |
|Inkplate VIN Pad   |VCC                        |Red       |

Also, baud rate switches must be set in next position (to set a baud rate of 57600 bauds)
|1    |2    |3    |
|-----|-----|-----|
|ON   |OFF  |ON   |

**NOTE** After setting the switches (baud rate), breakout must be powered off to apply the new baud rate.

### Buzzer
Connect P1-7 of the GPIO EXPANDER 1 to the (+) of the magnetic buzzer. Other end of the buzzer connect to the ground. To protect GPIO expander pin, connect Schottky Diode parallel to the buzezr, with the anode of the diode connected to the GND.

### API
As mentioned, this device has API; for getting week and daily hours of selected employee and to get the list of the currently active employees with it's tagID.

- `getweekhours` API
    - endpoint: `[deviceName|IP address]/api/getweekhours/tagID`
    - example: `timeloggerdevice.local/api/getweekhours/12345678` or `192.168.71.99/api/getweekhours/12345678`
    - response:
       ````
       {
           "firstName": "Borna",
           "lastName": "Biro",
           "tagId": "13500832",
           "department": "R&D",
           "status": "ok",
           "status_desc": "loginOnly",
           "first_login": "08:16:39 18.12.2024.",
           "last_logout": "--:--:-- --.--.----.",
           "daily": "05:04:24",
           "weekly": "23:40:07"
       }
       ````
- `getemployeelist` API
    - endpoint: `[deviceName|IP address]/api/getemployeelist/`
    - example: `timeloggerdevice.local/api/getemployeelist` or `192.168.71.99/api/getemployeelist`
    - response:
       ````
       {
           "status": "ok",
           "timestamp": 1734528352,
           "employeeNumber": 2,
           "employeeList": [
             {
               "tagID": 12345678,
               "firstName": "Borna",
               "lastName": "Biro",
               "department": "R&D"
             },
             {
               "tagID": 87654321,
               "firstName": "Ivan",
               "lastName": "Ivic",
               "department": "Operations"
             }
           ]
       }
       ````

## Notes
- This is updated version, so files from previous version won't work. Main differences are:
  - `;` is replaced by `,` is CVS file
  - Removed unnecessary spaces from CVS files
  - Removed unnecessary `,` or `;` at the end
  - workers.csv is now called employees.csv
  - images are in .png format
- Web pages are cached! If web page is changed, use CTRL+F5 in Chromium Browser to clear the cache. This needs to be done for every web page on server.
