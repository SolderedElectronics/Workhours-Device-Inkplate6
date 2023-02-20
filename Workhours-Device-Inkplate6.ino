#include "Inkplate.h" //Include Inkplate library to the sketch
#include "esp_wifi.h"
#include "include/Inter12pt7b.h"
#include "include/Inter16pt7b.h"
#include "include/Inter8pt7b.h"
#include "include/mainUI.h"
#include <ArduinoJson.h>
#include <sys\time.h>
#include <time.h>

#include "defines.h"
#include "helpers.h"
#include "linkedList.h"
#include "logging.h"

Inkplate display(INKPLATE_1BIT); // Create an object on Inkplate library and also set library into 1 Bit mode (BW)
WiFiServer server(80);
LinkedList myList;
Logging logger;

// Change WiFi and IP data to suit your setup.
char ssid[] = "";
char pass[] = "";
// Set your Static IP address
IPAddress localIP(192, 168, 2, 200); // IP address should be set to desired address
// Set your Gateway IP address
// Gateway address (in most cases it's the first address of selected IP addreess subnet)
IPAddress gateway(192, 168, 2, 1);
IPAddress subnet(255, 255, 255, 0);   // Subnet mask
IPAddress primaryDNS(192, 168, 2, 1); // Primary DNS (use router / ISP DNS as primary one)
IPAddress secondaryDNS(8, 8, 4, 4);   // Secondary DNS (use google as secondary one)

// Global variables.
// Used for menu refreshing.
uint32_t refreshes = 0;
bool changeNeeded = 1;
unsigned long periodicRefresh = 0;
unsigned long menuTimeout = 0;

// Used for RTC update.
unsigned long rtcUpdate = 0;

void setup()
{
    Serial.begin(115200);
    Serial2.begin(57600, SERIAL_8N1, 39, -1); // Software Serial for RFID
    display.begin();                          // Init Inkplate library (you should call this function ONLY ONCE)
    display.clearDisplay();                   // Clear frame buffer of display
    display.setTextColor(BLACK, WHITE);
    display.display(); // Put clear image on display
    display.sdCardInit();
    display.setFont(&Inter16pt7b);
    display.setCursor(0, 30);
    display.pinModeIO(BUZZER_PIN, OUTPUT, IO_INT_ADDR);

    display.print("Checking microSD Card");
    display.partialUpdate(0, 1);
    if (!sd.begin(15, SD_SCK_MHZ(10)))
    {
        BUZZ_SYS_ERROR;
        errorDisplay();
        display.partialUpdate(0, 1);
        while (1)
            ;
    }

    display.print("\nConfiguring IP Adresses");
    display.partialUpdate(0, 0);

    // High current consumption due WiFi and epaper activity causes reset of the ESP32. So wait a little bit.
    delay(1000);

    // Confingure IP Adress, subnet, DNS.
    if (!WiFi.config(localIP, gateway, subnet, primaryDNS, secondaryDNS))
    {
        display.println("\nSTA Failed to configure, please reset Inkplate! If error keeps to repeat, try to cnofigure "
                        "STA in different way or use another IP address");
    }

    WiFi.mode(WIFI_STA);
    esp_wifi_set_ps(WIFI_PS_NONE);
    WiFi.begin(ssid, pass);
    if (WiFi.status() != WL_CONNECTED)
    {
        WiFi.reconnect();

        delay(5000);

        int cnt = 0;
        display.print("\nWaiting for WiFi to connect");
        display.partialUpdate(0, 1);
        while ((WiFi.status() != WL_CONNECTED))
        {
            // Prints a dot every second that wifi isn't connected
            display.print(F("."));
            display.partialUpdate(0, 1);
            delay(1000);
            ++cnt;

            if (cnt == 25)
            {
                display.println("Can't connect to WIFI, restart initiated.");
                display.partialUpdate(0, 1);
                delay(100);
                ESP.restart();
            }
        }
    }

    // Start server
    display.print("\nStarting the web server");
    display.partialUpdate();
    server.begin();
    display.print("\nGetting the time from timeAPI...");
    display.partialUpdate();
    if (!fetchTime())
    {
        display.print("Failed! Please reset the device.");
        display.partialUpdate();
        BUZZ_SYS_ERROR;
        while (true)
            ;
    }

    // Initialize library for logging functions. Send address of the SdFat object and Linked List object as parameters
    // (needed by the library).
    display.print("\nReading data from SD card");
    display.partialUpdate();
    logger.begin(&sd, &myList, &display);

    // Be ready for the next daily report calculation
    logger.calculateNextDailyReport();

    Serial.println(WiFi.localIP());

    // Draw the main screen
    display.clearDisplay();
    mainDraw();
    changeNeeded = 1;
}

void loop()
{
    // Check if connection with WiFi is lost
    if (WiFi.status() == WL_CONNECTED)
    {
        // Check if there are active requests from clients
        WiFiClient client = server.available();
        if (client.available())
        {
            // Call function to do server things
            doServer(&client);
        }
    }
    else
    {
        // If connection to Wi Fi is lost then reconnect
        WiFi.reconnect();
    }

    // Check if tag is scanned.
    if (Serial2.available())
    {
        // Get data from RFID reader and conver it into integer (Tag data is sent as ASCII coded numbers).
        uint64_t tag = logger.getTagID();

        // If the tag is successfully read, check if there is employee with that tag.
        struct employeeData employee;
        int result = logger.addLog(tag, display.rtcGetEpoch(), employee);

        if (result != LOGGING_TAG_ERROR)
        {
            // If everything went ok, read the result.
            // If it's login, show login screen with name, last name, department, tagID etc.
            if (result == LOGGING_TAG_LOGIN)
                login(&employee);

            // If the tag if already logged in the span of 10 minutes, just ingore it and show error message
            if (result == LOGGING_TAG_10MIN)
                alreadyLogged(tag);

            // If it's logout, show logout screen with weekday work hours and daily work hours.
            if (result == LOGGING_TAG_LOGOUT)
            {
                logout(&employee, logger.getEmployeeDailyHours(tag, (int32_t)display.rtcGetEpoch()),
                       logger.getEmployeeWeekHours(tag, (int32_t)display.rtcGetEpoch()));
            }

            // If tag is not in the list, show error message!
            if (result == LOGGING_TAG_NOT_FOUND)
                unknownTag(tag);
        }
        else if (result == LOGGING_TAG_ERROR)
        {
            // Show error message
            tagLoggingError();
        }
    }

    // If menu is active, return to the main screen after some times (defied in LOG_SCREEN_TIME).
    if ((menuTimeout != 0) && ((unsigned long)(millis() - menuTimeout) > LOG_SCREEN_TIME))
    {
        menuTimeout = 0;
        display.clearDisplay();
        mainDraw();
        changeNeeded = 1;
    }
    else if ((unsigned long)(millis() - periodicRefresh) >= 60000UL) // Periodically refresh screen every 60 seconds.
    {
        periodicRefresh = millis();
        display.clearDisplay();
        mainDraw();
        changeNeeded = 1;

        // Update the RTC data.
        display.rtcGetRtcData(); // Get RTC data
    }

    // Get RTC data every second
    if ((unsigned long)(millis() - rtcUpdate) >= 1000UL)
    {
        rtcUpdate = millis();
        display.rtcGetRtcData(); // Get RTC data
    }

    if (changeNeeded) // If change is needed, refresh screen
    {
        if (refreshes > 40000)
        {
            display.display();
            refreshes = 0;
        }
        else
        {
            refreshes += display.partialUpdate();
        }
        changeNeeded = 0;
    }

    // Check if daily report needs to be made.
    if (logger.isDailyReport())
    {
        // Show the screen
        dailyReportScreen();
        display.display();

        // Make a daily report for all employees
        logger.createDailyReport();

        // Calculate the time for the next.
        logger.calculateNextDailyReport();

        // Update the time.
        fetchTime();

        // Wait for 10 seconds (daily report is activated 10 seconds before midnight).
        delay(10000);
    }
}

// Get the time from the web (time.api). If everything went ok, Inkplate RTC will be set to the correct time.
// Otherwise, funciton will return 0 and clock won't be set.
int fetchTime()
{
    WiFiClientSecure client;
    DynamicJsonDocument _doc(500);
    DeserializationError _err;
    char _temp[500];
    struct tm _t;
    int32_t _epoch;
    int i = 0;
    char *_jsonStart = NULL;

    client.setInsecure(); // Skip verification
    if (client.connect("timeapi.io", 443))
    {
        client.println("GET /api/Time/current/zone?timeZone=Europe/Zagreb HTTP/1.1");
        client.println("Host: timeapi.io");
        client.println("Connection: close");
        client.println();

        // Skip header.
        while (client.connected())
        {
            String line = client.readStringUntil('\n');
            if (line == "\r")
            {
                break;
            }
        }

        // Read and store everything after the header
        int i = 0;
        memset(_temp, 0, sizeof(_temp));
        while (client.available())
        {
            _temp[i++] = client.read();
        }
        client.stop();

        // Find the start of the JSON file
        _jsonStart = strchr(_temp, '{');
        if (_jsonStart == NULL)
            return 0;

        // Deserialize the JSON file.
        _err = deserializeJson(_doc, _jsonStart);

        // Deserilization error=? Return error.
        if (_err)
        {
            return 0;
        }

        // No entry in JSON called "seconds"? Return error.
        if (!_doc.containsKey("seconds"))
        {
            return 0;
        }

        // Get the clock!
        _t.tm_year = (int)_doc["year"] - 1900;
        _t.tm_mon = (int)_doc["month"] - 1;
        _t.tm_mday = _doc["day"];
        _t.tm_hour = _doc["hour"];
        _t.tm_min = _doc["minute"];
        _t.tm_sec = _doc["seconds"];
        _t.tm_isdst = 0;
        _epoch = mktime(&_t);

        // Set it on Inkplate RTC.
        display.rtcSetEpoch(_epoch);
        display.rtcGetRtcData();
        return 1;
    }

    // Stop the client (just in case).
    client.stop();

    // If you got this far, that means something is wrong. Return error.
    return 0;
}

// Show login screen.
void login(struct employeeData *_w)
{
    // Code for login screen

    // Make a sound.
    BUZZ_LOG;

    // Array for the image path on the microSD card.
    char _imagePath[200];

    // Write the data on the display.
    display.setTextSize(1);
    display.setFont(&Inter16pt7b);
    display.setCursor(350, 30);
    display.clearDisplay();
    display.print("First name:  ");
    display.print(_w->firstName);
    display.setCursor(350, 60);
    display.print("Last name:  ");
    display.print(_w->lastName);
    display.setCursor(350, 90);
    display.print("ID:  ");
    display.print(_w->ID);
    display.setCursor(350, 120);
    display.print(_w->department);
    display.setCursor(350, 190);
    display.print("Login");

    // Create path to the image on the microSD card
    createImagePath((*_w), _imagePath);

    // Try to display it. If not, use default image.
    if (!(display.drawImage(_imagePath, 5, 5, 1, 0)))
    {
        display.drawImage(DEFAULT_IMAGE_PATH, 5, 5, 1, 0);
    }

    // Set variables for screen update and timeout.
    menuTimeout = millis();
    changeNeeded = 1;
}

// Show logout screen.
void logout(struct employeeData *_w, uint32_t _dailyHours, uint32_t _weekHours)
{
    // Code for logout screen

    // Make a sound.
    BUZZ_LOG;

    // Array for the image path on the microSD card.
    char _imagePath[200];

    // Write the data on the display.
    display.setTextSize(1);
    display.setFont(&Inter16pt7b);
    display.setCursor(350, 30);
    display.clearDisplay();
    display.print("Name:  ");
    display.print(_w->firstName);
    display.setCursor(350, 60);
    display.print("Last name:  ");
    display.print(_w->lastName);
    display.setCursor(350, 90);
    display.print("ID:  ");
    display.print(_w->ID);
    display.setCursor(350, 120);
    display.print(_w->department);
    display.setCursor(350, 150);
    display.printf("Daily: %2d:%02d:%02d", _dailyHours / 3600, _dailyHours / 60 % 60, _dailyHours % 60);
    display.setCursor(350, 180);
    display.printf("Weekly: %2d:%02d:%02d", _weekHours / 3600, _weekHours / 60 % 60, _weekHours % 60);
    display.setCursor(350, 250);
    display.print("Logout");

    // Create path to the image on the microSD card
    createImagePath((*_w), _imagePath);

    // Try to display it. If not, use default image.
    if (!(display.drawImage(_imagePath, 5, 5, 1, 0)))
    {
        display.drawImage(DEFAULT_IMAGE_PATH, 5, 5, 1, 0);
    }

    // Set variables for screen update and timeout.
    menuTimeout = millis();
    changeNeeded = 1;
}

// Show the screen if some one is already logged.
void alreadyLogged(uint64_t _tag)
{
    BUZZ_LOG_ERROR;
    display.clearDisplay();
    display.setTextSize(3);
    display.setFont(&Inter16pt7b);
    display.setCursor(280, 100);
    display.print("No can't do");
    display.setTextSize(1);
    display.setCursor(50, 500);
    display.print("You already scanned your tag in last 10 minutes!");
    menuTimeout = millis();
    changeNeeded = 1;
}

// Show error screen for unknown tag.
void unknownTag(unsigned long long _tagID)
{
    BUZZ_LOG_ERROR;
    display.clearDisplay();
    display.setTextSize(1);
    display.setFont(&Inter16pt7b);
    display.setCursor(50, 50);
    display.print("Tag ID ");
    display.print(_tagID);
    display.print(" is not assigned to any worker");
    menuTimeout = millis();
    changeNeeded = 1;
}

// Show this screen if something weird happen to the logging.
void tagLoggingError()
{
    BUZZ_LOG_ERROR;
    display.clearDisplay();
    display.setTextSize(1);
    display.setFont(&Inter16pt7b);
    display.setCursor(50, 50);
    display.print("Logging failed! Check with the gazda for more info");
    menuTimeout = millis();
    changeNeeded = 1;
}

// This screen will appear if the microSD card is missing while loging in or loging out.
void errorDisplay()
{
    BUZZ_SYS_ERROR;
    display.setTextSize(1);
    display.setFont(&Inter16pt7b);
    display.setCursor(0, 30);
    display.clearDisplay();
    display.print("\nError occured. No SD card inserted. Please insert SD Card. If you don't have SD card, please "
                  "contact gazda.");
    menuTimeout = millis();
    changeNeeded = 1;
}

// Screen when daily report is active
void dailyReportScreen()
{
    display.setTextSize(1);
    display.setFont(&Inter16pt7b);
    display.setCursor(0, 30);
    display.clearDisplay();
    display.print("Creating daily report, please wait!");
    menuTimeout = millis();
}

// Funcion for making a sound. Buzzer must be connected on the IO expaned GPIO15 pin with the MOSFET)
// Do not connect buzzer directly to the IO expander!
void buzzer(uint8_t n, int _ms)
{
    for (int i = 0; i < n; i++)
    {
        unsigned long _timer1 = millis();
        while ((unsigned long)(millis() - _timer1) < _ms)
        {
            display.digitalWriteIO(BUZZER_PIN, HIGH, IO_INT_ADDR);
            display.digitalWriteIO(BUZZER_PIN, LOW, IO_INT_ADDR);
        }
        _timer1 = millis();
        while ((unsigned long)(millis() - _timer1) < _ms)
            ;
    }
}

// Function that handles all requests for the client.
void doServer(WiFiClient *client)
{
    // Update the RTC data (needed by the code below)
    display.rtcGetRtcData();

    char buffer[256];
    client->read((uint8_t *)buffer, 256);
    if (strstr(buffer, "style.css"))
    {
        client->println("HTTP/1.1 200 OK");
        client->println("Content-type:text/css");
        client->println("Cache-Control: public, max-age=2678400");
        client->println("Connection: close");
        client->println();
        client->println("@font-face {");
        client->printf("  font-family: '%s';\r\n", FONT_NAME);
        client->printf("  src: url('%s') format('%s');\r\n", FONT_FILENAME, FONT_FORMAT);
        client->println("}\n");
        client->println("html {");
        client->printf("  font-family: %s, %s, %s; \r\n", FONT_NAME, FONT_NAME, FONT_TYPE);
        client->println("  display: inline-block; ");
        client->println("  text-align: center;");
        client->println("}");
        client->println("h1 {");
        client->printf("  font-family: %s, %s, %s; \r\n", FONT_NAME, FONT_NAME, FONT_TYPE);
        client->println("  font-size: 1.8rem; ");
        client->println("  color: #FEFCFB;");
        client->println("}");
        client->println("p { ");
        client->printf("  font-family: %s, %s, %s; \r\n", FONT_NAME, FONT_NAME, FONT_TYPE);
        client->println("  font-size: 1.4rem;");
        client->println("}");
        client->println(".topnav { ");
        client->println("  overflow: hidden; ");
        client->println("  background-color: #5B2379;");
        client->println("}");
        client->println("body {  ");
        client->println("  margin: 0;");
        client->println("}");
        client->println(".content { ");
        client->println("  padding: 5%;");
        client->println("}");
        client->println(".card-grid { ");
        client->println("  max-width: 800px; ");
        client->println("  margin: 0 auto; ");
        client->println("  display: grid; ");
        client->println("  grid-gap: 2rem; ");
        client->println("  grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));");
        client->println("}");
        client->println(".card ");
        client->println("{ ");
        client->println("  background-color: white; ");
        client->println("  box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);");
        client->println("    padding-top: 30px;");
        client->println("    padding-bottom: 30px;");
        client->println("}");
        client->println(".card-title { ");
        client->println("  font-size: 1.2rem;");
        client->println("  font-weight: bold;");
        client->println("  color: #23B9D6");
        client->println("}");
        client->println("input[type=submit] ");
        client->println("{");
        client->println("  border: none;");
        client->println("  color: #FEFCFB;");
        client->println("  background-color: #23B9D6;");
        client->println("  padding: 15px 15px;");
        client->println("  text-align: center;");
        client->println("  text-decoration: none;");
        client->println("  display: inline-block;");
        client->println("  font-size: 16px;");
        client->println("  width: 150px;");
        client->println("  margin-right: 10px;");
        client->println("  border-radius: 4px;");
        client->println("  transition-duration: 0.4s;");
        client->println("  }");
        client->println("a {");
        client->printf("  font-family: %s, %s, %s; \r\n", FONT_NAME, FONT_NAME, FONT_TYPE);
        client->println(" text-decoration: none;");
        client->println(" width: 150px;");
        client->println("}");
        client->println("button {");
        client->printf("  font-family: %s, %s, %s; \r\n", FONT_NAME, FONT_NAME, FONT_TYPE);
        client->println("  border: none;");
        client->println("  color: #FEFCFB;");
        client->println("  background-color: #23B9D6;");
        client->println("  padding: 15px 15px;");
        client->println("  text-align: center;");
        client->println("  text-decoration: none;");
        client->println("  display: inline-block;");
        client->println("  font-size: 16px;");
        client->println("  width: 150px;");
        client->println("  margin-right: 10px;");
        client->println("  border-radius: 4px;");
        client->println("  transition-duration: 0.4s;");
        client->println("  }");
        client->println("input[type=radio]:hover {");
        client->printf("  font-family: %s, %s, %s; \r\n", FONT_NAME, FONT_NAME, FONT_TYPE);
        client->println(" margin : 18px;");
        client->println("  background-color: #1282A2;");
        client->println("}");
        client->println("input[type=submit]:hover {");
        client->printf("  font-family: %s, %s, %s; \r\n", FONT_NAME, FONT_NAME, FONT_TYPE);
        client->println(" margin : 18px;");
        client->println("  background-color: #1282A2;");
        client->println("}");
        client->println("input[type=radio] {");
        client->printf("  font-family: %s, %s, %s; \r\n", FONT_NAME, FONT_NAME, FONT_TYPE);
        client->println("  margin : 18px;");
        client->println("}");
        client->println("input[type=submit] {");
        client->printf("  font-family: %s, %s, %s; \r\n", FONT_NAME, FONT_NAME, FONT_TYPE);
        client->println(" margin : 18px;");
        client->println("}");
        client->println("button:hover {");
        client->printf("  font-family: %s, %s, %s; \r\n", FONT_NAME, FONT_NAME, FONT_TYPE);
        client->println("  background-color: #1282A2;");
        client->println("}");
        client->println("input[type=text], input[type=number], select {");
        client->printf("  font-family: %s, %s, %s; \r\n", FONT_NAME, FONT_NAME, FONT_TYPE);
        client->println("  width: 50%;");
        client->println("  padding: 12px 20px;");
        client->println("  margin: 18px;");
        client->println("  display: inline-block;");
        client->println("  border: 1px solid #ccc;");
        client->println("  border-radius: 4px;");
        client->println("  box-sizing: border-box;");
        client->println("}");
        client->println("label {");
        client->printf("  font-family: %s, %s, %s; \r\n", FONT_NAME, FONT_NAME, FONT_TYPE);
        client->println("  font-size: 1.2rem; ");
        client->println("}");
        client->println(".value{");
        client->printf("  font-family: %s, %s, %s; \r\n", FONT_NAME, FONT_NAME, FONT_TYPE);
        client->println("  font-size: 1.2rem;");
        client->println("  color: #1282A2;  ");
        client->println("}");
        client->println(".state {");
        client->printf("  font-family: %s, %s, %s; \r\n", FONT_NAME, FONT_NAME, FONT_TYPE);
        client->println("  font-size: 1.2rem;");
        client->println("  color: #1282A2;");
        client->println("}");
        client->println(".button-on {");
        client->printf("  font-family: %s, %s, %s; \r\n", FONT_NAME, FONT_NAME, FONT_TYPE);
        client->println("  background-color: #23B9D6;");
        client->println("}");
        client->println(".button-on:hover {");
        client->printf("  font-family: %s, %s, %s; \r\n", FONT_NAME, FONT_NAME, FONT_TYPE);
        client->println("  background-color: #1282A2;");
        client->println("}");
        client->println(".button-off {");
        client->printf("  font-family: %s, %s, %s; \r\n", FONT_NAME, FONT_NAME, FONT_TYPE);
        client->println("  background-color: #858585;");
        client->println("}");
        client->println(".button-off:hover {");
        client->printf("  font-family: %s, %s, %s; \r\n", FONT_NAME, FONT_NAME, FONT_TYPE);
        client->println("  background-color: #252524;");
        client->println("} ");
    }
    else if (strstr(buffer, FONT_FILENAME))
    {
        // Init micro SD Card
        sd.begin(15, SD_SCK_MHZ(10));

        // Set root as current working directory
        sd.chdir(true);

        // Allocate memory for the font file
        char *ps = (char *)ps_malloc(500000);

        // Allocation ok? Get the file from the micro SD
        if (ps != NULL)
        {
            File file;
            char _filePath[250];

            // Make path to the font file
            sprintf(_filePath, "%s/%s", DEFAULT_FOLDER_NAME, FONT_FILENAME);

            // Try to open the file
            if (file.open(_filePath, FILE_READ))
            {
                // Send response (200 OK)
                client->println("HTTP/1.1 200 OK");
                client->println("Content-type:file/woff");
                client->println("Cache-Control: public, max-age=2678400");
                client->println("Connection: close");
                client->println();

                // Open successfully. Get the bytes and send it to the web.
                int size_of_file = file.size();
                memset(ps, 0, size_of_file * sizeof(char));
                file.read(ps, size_of_file);
                file.close();
                client->write(ps, size_of_file);
            }

            // Free the memory.
            free(ps);
        }
    }
    else if (strstr(buffer, "addworker/?"))
    {
        client->println("HTTP/1.1 200 OK");
        client->println("Content-type:text/html");
        client->println("Connection: close");
        client->println();
        unsigned long long tempID = 0;
        char tempFName[50];
        char tempLName[50];
        char tempImage[128];
        char tempDepartment[100];
        char tempKey[51];
        char *temp = strstr(buffer, "addworker/?");

        client->println("<!DOCTYPE html>");
        client->println(" <html>");
        client->println(" <head>");
        client->println("   <title>Worktime</title>");
        client->println("   <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
        client->println("   <link rel=\"icon\" href=\"favicon.ico\" type=\"image/ico\">");
        client->println("   <link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">");
        client->println(" </head>");
        client->println(" <body>");
        client->println("  <div class=\"topnav\">");
        client->println("    <h1>Worktime</h1>");
        client->println("  </div>");
        client->println("  <div class=\"content\">");
        client->println("    <div class=\"card-grid\">");
        client->println("      <div class=\"card\">");

        // Parse the input data. There should be 6 items parsed.
        int _res =
            sscanf(temp, "addworker/?name=%[^'&']&lname=%[^'&']&tagID=%llu&department=%[^'&']&image=%[^'&']&key=%s",
                   tempFName, tempLName, &(tempID), tempDepartment, tempImage, tempKey);

        // Fix http text for all string inputs (covert HEX into ASCII, for example '# sign is in HTTp response %23 -
        // 0x23 is in ASCII '#').
        fixHTTPResponseText(tempFName);
        fixHTTPResponseText(tempLName);
        fixHTTPResponseText(tempDepartment);
        fixHTTPResponseText(tempImage);
        fixHTTPResponseText(tempKey);

        Serial.println(temp);
        Serial.printf("Add worker: Name: %s Last name: %s ID; %llu Depart: %s Image: %s\r\n", tempFName, tempLName,
                      tempID, tempDepartment, tempImage);

        // If the data is ok and key is vaild, add the employee.
        if (_res == 6 && strstr(tempKey, SECRET_KEY) != NULL)
        {
            if (!myList.checkID(tempID))
            {
                myList.addEmployee(tempFName, tempLName, tempID, tempImage, tempDepartment);
                logger.updateEmployeeFile();
                client->println("        <p>Employee added successfully!</p>");
            }
            else
            {
                client->println("        <p>Employee with ID already exist!</p>");
            }
        }
        else
        {
            client->println("<p>ERROR! Wrong password or input!</p>");
        }

        client->println("        <a href=\"/\"> <button>Home</button> </a>");
        client->println("        <a href=\"/addworker\"> <button>Add another Employee</button> </a>");
        client->println("      </div>");
        client->println("    </div>");
        client->println("  </div>");
        client->println(" </body>");
        client->println("</html>");
    }
    else if (strstr(buffer, "/addworker"))
    {
        client->println("HTTP/1.1 200 OK");
        client->println("Content-type:text/html");
        client->println("Cache-Control: public, max-age=2678400");
        client->println("Connection: close");
        client->println();
        client->println("<!DOCTYPE html>");
        client->println(" <html>");
        client->println(" <head>");
        client->println("   <title>Worktime</title>");
        client->println("   <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
        client->println("   <link rel=\"icon\" href=\"favicon.ico\" type=\"image/ico\">");
        client->println("   <link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">");
        client->println(" </head>");
        client->println(" <body>");
        client->println("  <div class=\"topnav\">");
        client->println("    <h1>Worktime</h1>");
        client->println("  </div>");
        client->println("  <div class=\"content\">");
        client->println("    <div class=\"card-grid\">");
        client->println("      <div class=\"card\">");
        client->println("        <form action=\"/addworker/\" method=\"GET\">");
        client->println("          <p>");
        client->println("            <label for=\"name\">First name</label>");
        client->println("            <input type=\"text\" id =\"name\" name=\"name\"><br>");
        client->println("            <label for=\"lname\">Last Name</label>");
        client->println("            <input type=\"text\" id =\"lname\" name=\"lname\"><br>");
        client->println("            <label for=\"tagID\">ID of RFID tag</label>");
        client->println("            <input type=\"number\" id =\"tagID\" name=\"tagID\"><br>");
        client->println("            <label for=\"department\">Department</label>");
        client->println("            <input type=\"text\" id =\"department\" name=\"department\"><br>");
        client->println("            <label for=\"image\">Image name</label>");
        client->println("            <input type=\"text\" id =\"image\" name=\"image\"><br>");
        client->println("            <label for=\"password\">Password</label>");
        client->println("            <input type=\"password\" id =\"key\" name=\"key\"><br>");
        client->println("            <input type =\"submit\" value =\"Add worker\">");
        client->println("        </form>");
        client->println("        <a href=\"/\"> <button>Back</button> </a>");
        client->println("      </div>");
        client->println("    </div>");
        client->println("  </div>");
        client->println(" </body>");
        client->println("</html>");
    }
    else if (strstr(buffer, "/favicon.ico"))
    {
        client->println("HTTP/1.1 200 OK");
        client->println("Content-type:image/ico");
        client->println("Cache-Control: public, max-age=2678400");
        client->println("Connection: close");
        client->println();
        sendIcon(client, &sd);
    }
    else if (strstr(buffer, "remove/?remove"))
    {
        // Send HHTTP response
        client->println("HTTP/1.1 200 OK");
        client->println("Content-type:text/html");
        client->println("Connection: close");
        client->println();
        client->println("<!DOCTYPE html>");
        client->println(" <html>");
        client->println(" <head>");
        client->println("   <title>Worktime</title>");
        client->println("   <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
        client->println("   <link rel=\"icon\" href=\"favicon.ico\" type=\"image/ico\">");
        client->println("   <link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">");
        client->println(" </head>");
        client->println(" <body>");
        client->println("  <div class=\"topnav\">");
        client->println("    <h1>Worktime</h1>");
        client->println("  </div>");
        client->println("  <div class=\"content\">");
        client->println("    <div class=\"card-grid\">");
        client->println("      <div class=\"card\">");

        uint64_t _idToRemove;
        char _tempKey[51];
        int _removeEmployeeDataFlag = 0;

        // Try to find the /?remove substring. If it's found, try to parse the data (employee ID and secretKey)
        char *_subString = strstr(buffer, "/?remove");
        if (_subString != NULL)
        {
            // Try to get the data from the string
            int _res = sscanf(_subString, "/?remove=%lld&dataRemoval=%d&key=%s", (unsigned long long *)(&_idToRemove),
                              &_removeEmployeeDataFlag, _tempKey);

            // Try to fix HTTP response (convert HTTP HEX into ASCII)
            fixHTTPResponseText(_tempKey);

            if (_res >= 2 && strstr(_tempKey, SECRET_KEY))
            {
                if (_removeEmployeeDataFlag)
                {
                    struct employeeData *_employee;
                    _employee = myList.getEmployeeByID(_idToRemove);
                    removeEmployeeData(&sd, _employee);
                }
                myList.removeEmployee(_idToRemove);
                logger.updateEmployeeFile();
                client->println("        <p>Worker removed successfully!</p>");
                client->println("        <a href=\"/\"> <button>Home</button> </a>");
                client->println("        <a href=\"/remove\"> <button>Remove another worker</button> </a>");
            }
            else
            {
                client->println("        <p>ERROR! Wrong password or input!</p>");
            }
        }

        client->println("      </div>");
        client->println("    </div>");
        client->println("  </div>");
        client->println(" </body>");
        client->println("</html>");
    }
    else if (strstr(buffer, "/monthly/?"))
    {
        // This block of code will send the needed file to the client.
        // It will send report file of a worker from the microSD card

        // Try to find the start of the request
        char *startStr = strstr(buffer, "/monthly/?");

        // Found a start of the HTTP GET request with "/monthly/?" string in it? Good, now get the data from it.
        if (startStr != NULL)
        {
            // Variables for storing the data from the HTTP GET req.
            struct employeeData employee;
            int month = 0;
            int year = 0;
            int rawDataFlag = 0;

            // Send a response
            client->println("HTTP/1.1 200 OK");

            // HTTP GET req. will look something like this:
            // /monthly/?worker=[FirstName]_[LastName]_[ID]&month=[Year]-[Month]&rawData=1
            // There should me minimum 5 arguments (first name, last name, ID, Year and month, rawDataFlag is optional)
            if (sscanf(startStr, "/monthly/?worker=%[^'_']_%[^'_']_%llu&month=%d-%d&rawData=%d", employee.firstName,
                       employee.lastName, (unsigned long long *)&(employee.ID), &year, &month, &rawDataFlag) >= 5)
            {
                SdFile myFile;

                // Try to find needed file on the micro SD Card.
                if (logger.getEmployeeFile(&myFile, &employee, month, year, rawDataFlag))
                {
                    // We need to send the file and for client to know that, we need to set proper Content-Type.
                    client->println("Content-Type: application/octet-stream");

                    // File name will be [FirstName]_[LastName]_[ID]_[year]_[month].csv
                    client->printf("Content-Disposition: attachment; filename=%s_%s_%llu_%d_%d.csv\r\n",
                                   employee.firstName, employee.lastName, (unsigned long long)(employee.ID), year,
                                   month);
                    client->println("Connection: close");
                    client->println();

                    // If you are trying to read all login / logout data, convert it to human readable T&D instead of
                    // EPOCH
                    if (rawDataFlag)
                    {
                        // Send out the header
                        client->println(LOGGING_RAW_FILE_HEADER);

                        while (myFile.available())
                        {
                            // Buffer for one line from file
                            char oneLine[50];

                            // Get the one line from the file
                            if (readOneLineFromFile(&myFile, oneLine, 49))
                            {
                                unsigned long long loginEpoch = 0;
                                unsigned long long logoutEpoch = 0;

                                // Parse one line from the file
                                if (sscanf(oneLine, "%llu; %llu", &loginEpoch, &logoutEpoch) != 0)
                                {
                                    char loginEpochStr[30];
                                    char logoutEpochStr[30];

                                    // Covert epoch to timestamp
                                    createTimeStampFromEpoch(loginEpochStr, loginEpoch);
                                    createTimeStampFromEpoch(logoutEpochStr, logoutEpoch);

                                    // Send one line to the client.
                                    client->printf("%s; %s;\r\n", loginEpochStr,
                                                   logoutEpoch != 0 ? logoutEpochStr : LOGGING_ERROR_STRING);
                                }
                            }
                        }
                    }
                    else
                    {
                        // If normal report is needed just read the file until you hit the end.
                        while (myFile.available())
                        {
                            client->write(myFile.read());
                        }
                    }

                    // Close the file -> Very importnant!
                    myFile.close();
                }
                else
                {
                    // Send error message if the file is not found.
                    client->println("Content-type:text/html");
                    client->println("Connection: close");
                    client->println();
                    client->println("Such file / entry does not exists on micro SD Card!");
                }
            }
            else
            {
                // Send an error message if there is a problem with the data inserted.
                client->println("Content-type:text/html");
                client->println("Connection: close");
                client->println();
                client->println("Wrong / unvalid data is inserted!");
            }
        }
    }
    else if (strstr(buffer, "/monthly"))
    {
        client->println("HTTP/1.1 200 OK");
        client->println("Content-type:text/html");
        client->println("Connection: close");
        client->println();
        client->println("<!DOCTYPE html>");
        client->println(" <html>");
        client->println(" <head>");
        client->println("   <title>Monthly review</title>");
        client->println("   <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
        client->println("   <link rel=\"icon\" href=\"favicon.ico\" type=\"image/ico\">");
        client->println("   <link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">");
        client->println(" </head>");
        client->println(" <body>");
        client->println("  <div class=\"topnav\">");
        client->println("    <h1>Worktime</h1>");
        client->println("  </div>");
        client->println("  <div class=\"content\">");
        client->println("    <div class=\"card-grid\">");
        client->println("      <div class=\"card\">");
        client->println("        <form action=\"/monthly/\" method=\"GET\">");
        client->println("            <label for=\"worker\">Choose worker:</label>");
        client->println("            <select name=\"worker\" id=\"worker\">");

        // Get the number of the employees.
        int numberOfEmloyees = myList.numberOfElements();

        // No employees? Send error message.
        if (numberOfEmloyees == 0)
        {
            client->println("               <option value=\"0\" selected=\"selected\" hidden=\"hidden\">No "
                            "employees in the list</option>");
            client->println("            </select>");
            client->println("            <br>");
            client->println("        </form>");
        }
        else
        {
            // If there are some employees, list them.
            for (int i = 0; i < numberOfEmloyees; i++)
            {
                // Struct for storing employee address in the linked list.
                struct employeeData *currentEmployee;

                // Get data of specific employee.
                currentEmployee = myList.getEmployee(i);

                // Add entry to the drop down list.
                client->printf("               <option value=\"%s_%s_%llu\">%s %s (%llu)</option>",
                               currentEmployee->firstName, currentEmployee->lastName,
                               (unsigned long long)(currentEmployee->ID), currentEmployee->firstName,
                               currentEmployee->lastName, (unsigned long long)(currentEmployee->ID));
            }
            client->println("            </select>");
            client->println("            <br>");
            client->println("<label for=\"month\">Choose month and year </label>");
            client->printf("<input type=\"month\" id=\"month\" name=\"month\" value=\"%04d-%02d\">\r\n",
                           display.rtcGetYear(), display.rtcGetMonth());
            client->println("<br><br>");
            client->println("<label for=\"rawData\">Export RAW Login data </label>");
            client->println("<input type=\"checkbox\" id=\"rawData\" name=\"rawData\" value=\"1\">");
            client->println("            <br>");
            client->println("            <input type =\"submit\" value =\"Select\">");
            client->println("        </form>");
        }
        client->println("        <a href=\"/\"> <button>Back</button> </a>");
        client->println("      </div>");
        client->println("    </div>");
        client->println("  </div>");
        client->println(" </body>");
        client->println("</html>");
    }
    else if (strstr(buffer, "/byworker/?"))
    {
        // If download of the file is requested, first send 200 OK.
        client->println("HTTP/1.1 200 OK");

        // First check if the microSD card is OK
        if (sd.begin(15, SD_SCK_MHZ(10)))
        {
            // Get the start of the response of the HTTP GET
            char *startStr = strstr(buffer, "/byworker/?");

            // If is found, start parsing the data
            if (startStr != NULL)
            {
                // Temp variables for year and month from the HTTP GET request
                int year;
                int month;

                // For successful parsing, two arguments must be provided in the HTTP GET request (year and month)
                if (sscanf(startStr, "/byworker/?date=%d-%d", &year, &month) == 2)
                {
                    // Now get whole daily from every employee
                    // But first get how much employees there are
                    int numberOfemployees = myList.numberOfElements();

                    // Check if there are no employees
                    if (numberOfemployees != 0)
                    {
                        struct employeeData *employee;

                        // Tell the client that we are going to send a lot of data, and we do not know how much (so no
                        // Content-Length argument now)
                        client->println("Content-Type: application/octet-stream");

                        // File name will be Full_Monthly_Report_[year]_[month].csv
                        client->printf("Content-Disposition: attachment; filename=Full_Monthy_Report_%d_%d.csv\r\n",
                                       year, month);
                        client->println("Connection: close");
                        client->println();

                        // Now we need to send the data of each employee
                        for (int i = 0; i < numberOfemployees; i++)
                        {
                            SdFile myFile;

                            // Get the employee data
                            employee = myList.getEmployee(i);

                            // First make a header for each employee (basic employee data)
                            client->printf("First Name:%s  Last Name:%s  TAG ID:%llu  Department:%s\r\n",
                                           employee->firstName, employee->lastName, (unsigned long long)(employee->ID),
                                           employee->department);

                            // Try to open a file with reports fot this month
                            if (logger.getEmployeeFile(&myFile, employee, month, year, 0))
                            {
                                // Read the whole file and send it to the client byte-by-byte.
                                while (myFile.available())
                                {
                                    client->write(myFile.read());
                                }
                            }
                            else
                            {
                                // If there is no file for this specific employee, write an error.
                                client->println("[No data for this employee]");
                            }

                            // Make a few new lines between employees
                            client->println("\r\n\r\n");

                            // Close the file
                            myFile.close();
                        }

                        // Print out report data
                        client->printf(
                            "Report was created at %02d:%02d:%02d on %02d.%02d.%04d.\r\nBy Soldered Electronics",
                            display.rtcGetHour(), display.rtcGetMinute(), display.rtcGetSecond(), display.rtcGetDay(),
                            display.rtcGetMonth(), display.rtcGetYear());
                    }
                    else
                    {
                        // No employees in the list? Send an error message
                        client->println("Content-type:text/html");
                        client->println("Connection: close");
                        client->println();
                        client->println("No employess in the list!");
                    }
                }
                else
                {
                    // If parsing has failed, show error message (this should not happen!)
                    client->println("Content-type:text/html");
                    client->println("Connection: close");
                    client->println();
                    client->println("Wrong data / parameters for the date!");
                }
            }
        }
        else
        {
            // Send error message if the microSD card can't be found.
            client->println("Content-type:text/html");
            client->println("Connection: close");
            client->println();
            client->println("Wrong data / parameters for the date!");
            client->println("microSD Card access error!");
        }
    }
    else if (strstr(buffer, "/byworker"))
    {
        client->println("HTTP/1.1 200 OK");
        client->println("Content-type:text/html");
        client->println("Connection: close");
        client->println();
        client->println("<!DOCTYPE html>");
        client->println("<html>");
        client->println("<head>");
        client->println("<title>Worktime</title>");
        client->println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
        client->println("<link rel=\"icon\" href=\"favicon.ico\" type=\"image/ico\">");
        client->println("<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">");
        client->println("</head>");
        client->println("<body>");
        client->println("<div class=\"topnav\">");
        client->println("    <h1>Worktime</h1>");
        client->println("</div>");
        client->println("<div class=\"content\">");
        client->println("    <div class=\"card-grid\">");
        client->println("    <div class=\"card\">");
        client->println("        <form action=\"/byworker/\" method=\"GET\">");
        client->println("        <p>");
        client->println("            <label for=\"calendar\">Select month and year for report </label>");
        client->printf("            <input type=\"month\" id=\"date\" name=\"date\" value=\"%04d-%02d\">\r\n",
                       display.rtcGetYear(), display.rtcGetMonth());
        client->println("            <br>");
        client->println("            <input type =\"submit\" value =\"Get\">");
        client->println("        </form>");
        client->println("        <a href=\"/\"> <button>Back</button> </a>");
        client->println("    </div>");
        client->println("    </div>");
        client->println("</div>");
        client->println("</body>");
        client->println("</html>");
    }
    else if (strstr(buffer, "/remove"))
    {
        client->println("HTTP/1.1 200 OK");
        client->println("Content-type:text/html");
        client->println("Connection: close");
        client->println();
        client->println("<!DOCTYPE html>");
        client->println(" <html>");
        client->println(" <head>");
        client->println("   <title>Worktime</title>");
        client->println("   <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
        client->println("   <link rel=\"icon\" href=\"favicon.ico\" type=\"image/ico\">");
        client->println("   <link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">");
        client->println(" </head>");
        client->println(" <body>");
        client->println("  <div class=\"topnav\">");
        client->println("    <h1>Worktime</h1>");
        client->println("  </div>");
        client->println("  <div class=\"content\">");
        client->println("    <div class=\"card-grid\">");
        client->println("      <div class=\"card\">");
        client->println("        <form action=\"/remove/\" method=\"GET\">");

        int i = 0;
        struct employeeData *list = myList.getEmployee(i++);
        if (list != NULL)
        {
            while (list != NULL)
            {
                client->print("            <input type =\"radio\" name=\"remove\" value =\"");
                client->print(list->ID);
                client->println("\">");
                client->print("            <label for=\"javascript\">");
                client->print(list->firstName);
                client->print(" ");
                client->print(list->lastName);
                client->print(" (");
                client->print(list->ID);
                client->print(")");
                client->println("</label><br>");
                list = myList.getEmployee(i++);
            }
        }
        else
        {
            client->print("No workers in the database!");
            client->println("</label><br>");
        }
        client->println("            <label for=\"dataRemoval\">Remove all employee data from the storage? </label>");
        client->println(
            "            <input type=\"checkbox\" id=\"dataRemoval\" name=\"dataRemoval\" value=\"1\"><br>");
        client->println("            <label for=\"password\">Password</label>");
        client->println("            <input type=\"password\" id =\"key\" name=\"key\"><br>");
        client->println("            <input type =\"submit\" value =\"Remove\">");
        client->println("        </form>");
        client->println("        <a href=\"/\"> <button>Back</button> </a>");
        client->println("      </div>");
        client->println("    </div>");
        client->println("  </div>");
        client->println(" </body>");
        client->println("</html>");
    }
    else if (strstr(buffer, "/api"))
    {
        // API link is [PI Address]/api/getWeekHours/[TAGID]
        // Send response (200 OK)
        client->println("HTTP/1.1 200 OK");
        client->println("Content-type:text/plain");
        client->println("Connection: close");
        client->println();

        // This is block of code for the API interface
        // Check what kind of data is needed (added for future add-ons)
        if (strstr(buffer, "/getWeekHours"))
        {
            // Get the start of the HTTP GET Request
            char *startStr = strstr(buffer, "/getWeekHours");

            // If the start is found, get the TAG ID
            if (startStr != NULL)
            {
                unsigned long long tagID;

                if (sscanf(startStr, "/getWeekHours/%llu", &tagID) == 1)
                {
                    // Get employee data by the TAG ID
                    struct employeeData *employee;
                    employee = myList.getEmployeeByID(tagID);

                    if (employee != NULL)
                    {
                        int32_t weekHours = 0;
                        int32_t dayHours = 0;
                        int32_t lastLogin = 0;
                        int32_t lastLogout = 0;
                        int32_t lastLoginInList = 0;
                        int32_t lastLogoutInList = 0;

                        // Arrays for timestamp
                        char loginTimestampStr[30];
                        char logoutTimestampStr[30];

                        // Get weekly working hours

                        // First update the RTC
                        display.rtcGetRtcData();

                        // Get employee workhours (search employee by ID).
                        weekHours = logger.getEmployeeWeekHours(tagID, display.rtcGetEpoch());
                        dayHours = logger.getEmployeeDailyHours(tagID, display.rtcGetEpoch(), &lastLogin, &lastLogout);

                        // Get the last log data (to check if the logout is done)
                        if (logger.findLastLog(employee, &lastLoginInList, &lastLogoutInList))
                        {
                            // Send data to the client. Note that is different calculation if logout is done.
                            if (lastLogoutInList == 0 && lastLoginInList != 0)
                            {
                                // Add current time to the dayhours and weekhours (because logout is not done yet)
                                dayHours += ((int32_t)(display.rtcGetEpoch()) - lastLoginInList);
                                weekHours += ((int32_t)(display.rtcGetEpoch()) - lastLoginInList);

                                // Make string timestamp
                                createTimeStampFromEpoch(loginTimestampStr, lastLogin);

                                // Send the data to the client
                                client->printf("Daily: %02d:%02d:%02d\r\nWeekly: %02d:%02d:%02d\r\nLogin: "
                                               "%s\r\nLogout: Not Done Yet\r\n",
                                               dayHours / 3600, dayHours / 60 % 60, dayHours % 60, weekHours / 3600,
                                               weekHours / 60 % 60, weekHours % 60, loginTimestampStr);
                            }
                            else
                            {
                                // Make string timestamp
                                createTimeStampFromEpoch(loginTimestampStr, lastLogin);
                                createTimeStampFromEpoch(logoutTimestampStr, lastLogout);
                                client->printf(
                                    "Daily: %02d:%02d:%02d\r\nWeekly: %02d:%02d:%02d\r\nLogin: %s\r\nLogout: %s\r",
                                    dayHours / 3600, dayHours / 60 % 60, dayHours % 60, weekHours / 3600,
                                    weekHours / 60 % 60, weekHours % 60, lastLogin != -1 ? loginTimestampStr : "----",
                                    lastLogout != -1 ? logoutTimestampStr : "----");
                            }
                        }
                        else
                        {
                            // Send error if microSD card can't be found.
                            client->println("microSD Card access error!");
                        }
                    }
                    else
                    {
                        // Send error if the wrong ID is sent.
                        client->println("Wrong API ID!");
                    }
                }
                else
                {
                    // Send error message for the wrong API parameter
                    client->println("Wrong API parameter!");
                }
            }
            else
            {
                // Send error message for the wrong API request
                client->println("Wrong API request!");
            }
        }
        else
        {
            // Send error message for the wrong API request
            client->println("Wrong API request!");
        }
    }
    else
    {
        client->println("HTTP/1.1 200 OK");
        client->println("Content-type:text/html");
        client->println("Cache-Control: public, max-age=2678400");
        client->println("Connection: close");
        client->println();
        client->println("<!DOCTYPE html>");
        client->println(" <html>");
        client->println(" <head>");
        client->println("   <title>Worktime</title>");
        client->println("   <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
        client->println("   <link rel=\"icon\" href=\"favicon.ico\" type=\"image/ico\">");
        client->println("   <link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">");
        client->println(" </head>");
        client->println(" <body>");
        client->println("  <div class=\"topnav\">");
        client->println("    <h1>Worktime</h1>");
        client->println("  </div>");
        client->println("  <div class=\"content\">");
        client->println("    <div class=\"card-grid\">");
        client->println("      <div class=\"card\">");
        client->println("        <a href=\"/addworker\"> <button>Add worker</button> </a>");
        client->println("        <a href=\"/remove\"> <button>Remove worker</button> </a>");
        client->println("        <a href=\"/monthly\"> <button>Monthly review by worker</button></a>");
        client->println("        <a href=\"/byworker\"> <button>Workers review by month</button> </a>");
        client->println("      </div>");
        client->println("    </div>");
        client->println("  </div>");
        client->println(" </body>");
        client->println("</html>");
    }
    client->stop();
}
