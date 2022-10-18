#include "Inkplate.h" //Include Inkplate library to the sketch
#include "SdFat.h"
#include "esp_wifi.h"
#include "favicon.h"
#include "include/Inter12pt7b.h"
#include "include/Inter16pt7b.h"
#include "include/Inter8pt7b.h"
#include "include/mainUI.h"
#include <ArduinoJson.h>
#include <sys\time.h>
#include <time.h>

#include "dataTypes.h"
#include "helpers.h"
#include "linkedList.h"
#include "logging.h"

#define DEBOUNCE_SCAN      1000         // Miliseconds
#define LOG_SCREEN_TIME    10000        // Miliseconds
#define TIMEOUT_AT_LOGGING 60 * 10      // seconds * minutes
#define MAX_SHIFT_LASTING  60 * 60 * 14 // seconds * minutes * hours

Inkplate display(INKPLATE_1BIT); // Create an object on Inkplate library and also set library into 1 Bit mode (BW)
StaticJsonDocument<30000> doc;   // Buffer for JSON document
WiFiServer server(80);

LinkedList myList;
Logging logger;

uint32_t refreshes = 0;
time_t read_epoch;
// Set your Static IP address
IPAddress localIP(192, 168, 71, 200); // IP address should be set to desired address
// Set your Gateway IP address
IPAddress gateway(192, 168, 71, 1); // Gateway address should match IP address

IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);   // optional
IPAddress secondaryDNS(8, 8, 4, 4); // optional

const char *DOWStr[] = {"SUN", "MON", "TUE", "WEN", "THU", "FRI", "SAT"};

char ssid[] = "";
char pass[] = "";
time_t last_scan = 0, epoch = 0, log_shown = 0;
uint32_t tagID = 0;
uint8_t buffer[16], prev_hours = 0, prev_minutes = 0;
bool change_needed = 1, login_screen_shown = 0;

const char wday_name[7][10] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char mon_name[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

struct employeeData *workers = NULL;
struct employeeData *curr_worker = NULL;

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
    display.setCursor(10, 30);
    //display.pinModeMCP(0, OUTPUT);

    if (!sd.begin(15, SD_SCK_MHZ(10)))
    {
        errorDisplay();
        display.partialUpdate(0, 1);
        while (1)
            ;
    }

    if (!WiFi.config(localIP, gateway, subnet, primaryDNS, secondaryDNS))
    {
         display.println("STA Failed to configure, please reset Inkplate! If error keeps to repeat, try to cnofigure "
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
        display.print(F("Waiting for WiFi to connect.."));
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
    Serial.println(WiFi.localIP());

    // Start server
    server.begin();

    if (!fetchTime())
    {
        display.print("Can't get the time and date!");
        display.partialUpdate(0, 1);
        while (true)
            ;
    }

    // Initialize library for logging functions. Send address of the SdFat object and Linked List object as parameters
    // (needed by the library).
    logger.begin(&sd, &myList, &display);
    change_needed = 1;

    // Be ready for the next daily report calculation
    logger.calculateNextDailyReport();
}

void loop()
{
    if (WiFi.status() == WL_CONNECTED) // Check if connection with WiFi is lost
    {
        WiFiClient client = server.available(); // Check if there are active requests from clients
        if (client.available())
        {
            doServer(&client); // Call function to do server things
        }
    }
    else
    {
        WiFi.reconnect(); // If connection to Wi Fi is lost then reconnect
        // Serial.println("Reconnecting");
    }
    if (Serial2.available()) // Check if tag is scanned
    {
        // Get data from RFID reader and conver it into integer (Tag data is sent as ASCII coded numbers).
        uint64_t tag = logger.getTagID();


        // If the tag is successfully read, check if there is employee with that tag.
        display.rtcGetRtcData();
        struct employeeData employee;
        int result = logger.addLog(tag, display.rtcGetEpoch(), employee);

        if (result != LOGGING_TAG_ERROR)
        {
            // If eeverything went ok, read the result.

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
    }

    display.rtcGetRtcData(); // Get RTC data

    if (log_shown + LOG_SCREEN_TIME < millis() || millis() < LOG_SCREEN_TIME) // If login/logout screen is shown
    {
        display.clearDisplay();                     // Clear screen
        if (prev_minutes != display.rtcGetMinute()) // If minutes changed, change is needed
        {
            change_needed = 1;
            mainDraw(); // Draw main screen
        }
        else if (login_screen_shown) // If login screen is shown but should not be shown change is needed
        {
            login_screen_shown = 0;
            change_needed = 1;
            mainDraw(); // Draw main screen
        }
    }

    if (change_needed) // If change is needed, refresh screen
    {
        if (refreshes > 40000)
        {
            display.display();
            refreshes = 0;
        }
        else
        {
            refreshes += display.partialUpdate(0, 1);
        }
        change_needed = 0;
    }

    // Check if daily report needs to be made.
    if (logger.isDailyReport())
    {
        // Make a daily report for all employees
        logger.createDailyReport();

        // Calculate the time for the next.
        logger.calculateNextDailyReport();
    }
}

// Time fetching

int fetchTime()
{
    WiFiClientSecure client;
    DynamicJsonDocument _doc(500);
    DeserializationError _err;
    char _temp[500];
    struct tm _t;
    int32_t _epoch;

    client.setInsecure(); // Skip verification
    if (client.connect("timeapi.io", 443))
    {
        Serial.println("Connected!");
        client.println("GET /api/Time/current/zone?timeZone=Europe/Zagreb HTTP/1.0");
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
        // if there are incoming bytes available
        // from the server, read them and print them:
        int i = 0;
        memset(_temp, 0, sizeof(_temp));
        while (client.available())
        {
            _temp[i++] = client.read();
        }
        client.stop();

        _err = deserializeJson(_doc, _temp);

        if (_err)
        {
            return 0;
        }

        if (!_doc.containsKey("seconds"))
        {
            return 0;
        }

        _t.tm_year = (int)_doc["year"] - 1900;
        _t.tm_mon = (int)_doc["month"] - 1;
        _t.tm_mday = _doc["day"];
        _t.tm_hour = _doc["hour"];
        _t.tm_min = _doc["minute"];
        _t.tm_sec = _doc["seconds"];
        _t.tm_isdst = 0;
        _epoch = mktime(&_t);

        display.rtcSetEpoch(_epoch);
        display.rtcGetRtcData();
        return 1;
    }
    client.stop();
    return 0;
}

// Display screens
void login(struct employeeData *_w)
{
    // Code for login screen
    char _imagePath[200];

    display.setTextSize(1);
    display.setFont(&Inter16pt7b);
    display.setCursor(250, 360);
    display.clearDisplay();
    display.print("First name:  ");
    display.print(_w->firstName);
    display.setCursor(250, 400);
    display.print("Last name:  ");
    display.print(_w->lastName);
    display.setCursor(250, 440);
    display.print("ID:  ");
    display.print(_w->ID);
    display.printf(" [%s]", _w->department);
    display.setCursor(250, 530);
    display.print("Login");
    createImagePath((*_w), _imagePath);
    if (!(display.drawImage(_imagePath, 250, 20, 1, 0)))
    {
        display.drawImage(DEFAULT_IMAGE_PATH, 250, 20, 1, 0);
    }
    buzz(1);
    log_shown = millis();
    change_needed = 1;
    login_screen_shown = 1;
}

void logout(struct employeeData *_w, uint32_t _dailyHours, uint32_t _weekHours)
{
    // Code for logout screen
    char _imagePath[200];

    display.setTextSize(1);
    display.setFont(&Inter16pt7b);
    display.setCursor(250, 360);
    display.clearDisplay();
    display.print("Name:  ");
    display.print(_w->firstName);
    display.setCursor(250, 400);
    display.print("Last name:  ");
    display.print(_w->lastName);
    display.setCursor(250, 440);
    display.print("ID:  ");
    display.print(_w->ID);
    display.printf(" [%s]", _w->department);
    display.setCursor(250, 530);
    display.print("Logout");
    display.setCursor(250, 470);
    display.printf("Daily: %2d:%02d:%02d", _dailyHours / 3600, _dailyHours / 60 % 60, _dailyHours % 60);
    display.setCursor(250, 570);
    display.printf("Weekly: %2d:%02d:%02d", _weekHours / 3600, _weekHours / 60 % 60, _weekHours % 60);
    createImagePath((*_w), _imagePath);
    if (!(display.drawImage(_imagePath, 250, 20, 1, 0)))
    {
        display.drawImage(DEFAULT_IMAGE_PATH, 250, 20, 1, 0);
    }
    buzz(1);
    log_shown = millis();
    change_needed = 1;
    login_screen_shown = 1;
}

void alreadyLogged(uint64_t _tag)
{
    buzz(1);
    display.clearDisplay();
    display.setTextSize(3);
    display.setFont(&Inter16pt7b);
    display.setCursor(280, 100);
    display.print("No can't do");
    display.setTextSize(1);
    display.setCursor(50, 500);
    display.print("You already scanned your tag in last 10 minutes!");
    change_needed = 1;
    login_screen_shown = 1;
    log_shown = millis();
}

void unknownTag(unsigned long long _tagID)
{
    buzz(2);
    display.clearDisplay();
    display.setTextSize(1);
    display.setFont(&Inter16pt7b);
    display.setCursor(50, 50);
    display.print("Tag ID ");
    display.print(_tagID);
    display.print(" is not assigned to any worker");
    change_needed = 1;
    login_screen_shown = 1;
    log_shown = millis();
}

void errorDisplay()
{
    display.setTextSize(1);
    display.setFont(&Inter16pt7b);
    display.setCursor(0, 30);
    display.clearDisplay();
    display.print(
        "Error occured. No SD card inserted. Please insert SD Card. If you don't have SD card, please contact gazda.");
    display.display();
    log_shown = millis();
}

void buzz(uint8_t n)
{
    for (int i = 0; i < n; i++)
    {
        //display.digitalWriteMCP(0, HIGH);
        delay(150);
        //display.digitalWriteMCP(0, LOW);
        delay(150);
    }
}

void doServer(WiFiClient *client)
{
    char buffer[256];
    client->read((uint8_t *)buffer, 256);
    // Serial.println(buffer);
    // Serial.println("\n\n");
    if (strstr(buffer, "style.css"))
    {
        client->println("HTTP/1.1 200 OK");
        client->println("Content-type:text/css");
        client->println("Cache-Control: public, max-age=2678400");
        client->println("Connection: close");
        client->println();
        client->println("@font-face {");
        client->println("  font-family: 'Inter';");
        client->println("  src: url('/inter.woff') format('woff');");
        client->println("}\n");
        client->println("html {");
        client->println("  font-family: Inter, Inter, Regular; ");
        client->println("  display: inline-block; ");
        client->println("  text-align: center;");
        client->println("}");
        client->println("h1 {");
        client->println("  font-family: Inter, Inter, Regular; ");
        client->println("  font-size: 1.8rem; ");
        client->println("  color: #BCA876;");
        client->println("}");
        client->println("p { ");
        client->println("  font-family: Inter, Inter, Regular; ");
        client->println("  font-size: 1.4rem;");
        client->println("}");
        client->println(".topnav { ");
        client->println("  overflow: hidden; ");
        client->println("  background-color: #582C83;");
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
        client->println("  color: #25BAA8");
        client->println("}");
        client->println("input[type=submit] ");
        client->println("{");
        client->println("  border: none;");
        client->println("  color: #FEFCFB;");
        client->println("  background-color: #25BAA8;");
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
        client->println("  font-family: Inter, Inter, Regular; ");
        client->println(" text-decoration: none;");
        client->println(" width: 150px;");
        client->println("}");
        client->println("button {");
        client->println("  font-family: Inter, Inter, Regular; ");
        client->println("  border: none;");
        client->println("  color: #FEFCFB;");
        client->println("  background-color: #25BAA8;");
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
        client->println("  font-family: Inter, Inter, Regular; ");
        client->println(" margin : 18px;");
        client->println("  background-color: #1282A2;");
        client->println("}");
        client->println("input[type=submit]:hover {");
        client->println("  font-family: Inter, Inter, Regular; ");
        client->println(" margin : 18px;");
        client->println("  background-color: #1282A2;");
        client->println("}");
        client->println("input[type=radio] {");
        client->println("  font-family: Inter, Inter, Regular; ");
        client->println("  margin : 18px;");
        client->println("}");
        client->println("input[type=submit] {");
        client->println("  font-family: Inter, Inter, Regular; ");
        client->println(" margin : 18px;");
        client->println("}");
        client->println("button:hover {");
        client->println("  font-family: Inter, Inter, Regular; ");
        client->println("  background-color: #1282A2;");
        client->println("}");
        client->println("input[type=text], input[type=number], select {");
        client->println("  font-family: Inter, Inter, Regular; ");
        client->println("  width: 50%;");
        client->println("  padding: 12px 20px;");
        client->println("  margin: 18px;");
        client->println("  display: inline-block;");
        client->println("  border: 1px solid #ccc;");
        client->println("  border-radius: 4px;");
        client->println("  box-sizing: border-box;");
        client->println("}");
        client->println("label {");
        client->println("  font-family: Inter, Inter, Regular; ");
        client->println("  font-size: 1.2rem; ");
        client->println("}");
        client->println(".value{");
        client->println("  font-family: Inter, Inter, Regular; ");
        client->println("  font-size: 1.2rem;");
        client->println("  color: #1282A2;  ");
        client->println("}");
        client->println(".state {");
        client->println("  font-family: Inter, Inter, Regular; ");
        client->println("  font-size: 1.2rem;");
        client->println("  color: #1282A2;");
        client->println("}");
        client->println(".button-on {");
        client->println("  font-family: Inter, Inter, Regular; ");
        client->println("  background-color: #25BAA8;");
        client->println("}");
        client->println(".button-on:hover {");
        client->println("  font-family: Inter, Inter, Regular; ");
        client->println("  background-color: #1282A2;");
        client->println("}");
        client->println(".button-off {");
        client->println("  font-family: Inter, Inter, Regular; ");
        client->println("  background-color: #858585;");
        client->println("}");
        client->println(".button-off:hover {");
        client->println("  font-family: Inter, Inter, Regular; ");
        client->println("  background-color: #252524;");
        client->println("} ");
    }
    else if (strstr(buffer, "/inter.woff"))
    {
        client->println("HTTP/1.1 200 OK");
        client->println("Content-type:file/woff");
        client->println("Cache-Control: public, max-age=2678400");
        client->println("Connection: close");
        client->println();

        // Init micro SD Card
        sd.begin(15, SD_SCK_MHZ(10));

        // Set root as current working directory
        sd.chdir(true);

        // Allocate memory for the font file
        char *ps = (char *)ps_malloc(50000);

        // Allocation ok? Get the file from the micro SD
        if (ps != NULL)
        {
            File file;
            char _filePath[250];

            // Make path to the font file
            sprintf(_filePath, "%s/%s", DEFAULT_FOLDER_NAME, "webFontInter.dat");

            // Try to open the file
            if (file.open(_filePath, FILE_READ))
            {
                // Opened successfully. Get the bytes and send it to the web.
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
        char tempName[50];
        char tempLName[50];
        char tempImage[128];
        char tempDepartment[100];
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

        if (sscanf(temp, "addworker/?name=%[^'&']&lname=%[^'&']&tagID=%llu&department=%[^'&']&image=%s;", tempName,
                   tempLName, &(tempID), tempDepartment, tempImage) == 5)
        {
            if (!myList.checkID(tempID))
            {
                fixHTTPResponseText(tempDepartment);
                myList.addEmployee(tempName, tempLName, tempID, tempImage, tempDepartment);
                logger.updateEmployeeFile();
                client->println("        <p>Worker added successfully!</p>");
            }
            else
            {
                client->println("        <p>Worker with ID already exist!</p>");
            }
        }
        else
        {
            client->println("ERROR!");
        }

        client->println("        <a href=\"/\"> <button>Home</button> </a>");
        client->println("        <a href=\"/addworker\"> <button>Add another worker</button> </a>");
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
        client->println("            <label for=\"name\">Fist name</label>");
        client->println("            <input type=\"text\" id =\"name\" name=\"name\"><br>");
        client->println("            <label for=\"lname\">Last Name</label>");
        client->println("            <input type=\"text\" id =\"lname\" name=\"lname\"><br>");
        client->println("            <label for=\"tagID\">ID of RFID tag</label>");
        client->println("            <input type=\"text\" id =\"tagID\" name=\"tagID\"><br>");
        client->println("            <label for=\"department\">Department</label>");
        client->println("            <input type=\"text\" id =\"department\" name=\"department\"><br>");
        client->println("            <label for=\"image\">Image name</label>");
        client->println("            <input type=\"text\" id =\"image\" name=\"image\"><br>");
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
        client->write(favicon, 318);
    }
    else if (strstr(buffer, "remove/?remove"))
    {
        client->println("HTTP/1.1 200 OK");
        client->println("Content-type:text/html");
        client->println("Connection: close");
        client->println();

        uint64_t idToRemove;

        // Serial.println(buffer);
        char *_subString = strstr(buffer, "/?remove");
        if (_subString != NULL)
        {
            if (sscanf(_subString, "/?remove=%lld", (unsigned long long *)(&idToRemove)) == 1)
            {
                myList.removeEmployee(idToRemove);
                logger.updateEmployeeFile();
            }
        }
        else
        {
            // Serial.println("remove failed");
        }

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
        client->println("        <p>Worker removed successfully!</p>");
        client->println("        <a href=\"/\"> <button>Home</button> </a>");
        client->println("        <a href=\"/remove\"> <button>Remove another worker</button> </a>");
        client->println("      </div>");
        client->println("    </div>");
        client->println("  </div>");
        client->println(" </body>");
        client->println("</html>");
    }
    else if (strstr(buffer, "/monthly/"))
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
        if (strstr(buffer, "="))
        {
            char *wname = strstr(buffer, "=") + 1;
            strstr(wname, " ") != 0 ? *(strstr(wname, " ")) = '\0' : 0;

            if ((sd).begin(15, SD_SCK_MHZ(25)))
            {
                File dir;
                File tempFile;
                char strTemp[56];
                dir.open("/");
                while (tempFile.openNext(&dir, O_RDONLY))
                {
                    memset(strTemp, 0, 56 * sizeof(char));
                    tempFile.getName(strTemp, 56);
                    // Serial.println(strTemp);
                    if (strstr(strTemp, ".csv") && strstr(strTemp, wname))
                    {
                        client->print(F("        <a href='/download/"));
                        client->print(strTemp);
                        client->print(F("' style='color:#582C83;margin-bottom:30px;'>"));
                        client->print(strTemp);
                        client->println(F("</a></br>"));
                    }
                    tempFile.close();
                }
                dir.close();
            }
        }

        client->println("        <a href=\"/\"> <button>Home</button> </a>");
        client->println("        <a href=\"/monthly\"> <button>Back</button> </a>");
        client->println("      </div>");
        client->println("    </div>");
        client->println("  </div>");
        client->println(" </body>");
        client->println("</html>");
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
        curr_worker = workers;
        if (curr_worker == NULL)
        {
            client->println(
                "               <option value=\"0\" selected=\"selected\" hidden=\"hidden\">No workers yet!</option>");
            client->println("            </select>");
            client->println("            <br>");
            client->println("        </form>");
        }
        else
        {
            while (curr_worker != NULL)
            {
                char tmp[86];
                sprintf(tmp, "               <option value=\"%s_%s\">%s %s</option>", curr_worker->lastName,
                        curr_worker->firstName, curr_worker->firstName, curr_worker->lastName);
                client->println(tmp);
                curr_worker = curr_worker->next;
            }
            client->println("            </select>");
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
        client->println("HTTP/1.1 200 OK");
        client->println("Content-type:text/html");
        client->println("Connection: close");
        client->println();
        char *temp;
        temp = strstr(buffer, "&");
        *temp = ' ';
        temp = strstr(buffer, "=");
        *temp = ' ';
        temp = strstr(buffer, "=");
        *temp = ' ';
        *(temp + 1) = ' ';
        *(temp + 2) = ' ';

        temp = strstr(buffer, "/?") + 7;
        int mon, year;
        sscanf(temp, "%d year   %d", &mon, &year);
        char wanted[15];
        sprintf(wanted, "%d_%d.csv", mon, year);

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

        client->println("        <form action=\"/byworker/\" method=\"GET\">");
        client->println("          <p>");
        client->println("            <label for=\"month\">Choose month:</label>");
        client->print("            <select name=\"month\" id=\"month\" value=\"");
        client->print(display.rtcGetMonth());
        client->println("\">");
        client->println("               <option value=\"1\">1</option>");
        client->println("               <option value=\"2\">2</option>");
        client->println("               <option value=\"3\">3</option>");
        client->println("               <option value=\"4\">4</option>");
        client->println("               <option value=\"5\">5</option>");
        client->println("               <option value=\"6\">6</option>");
        client->println("               <option value=\"7\">7</option>");
        client->println("               <option value=\"8\">8</option>");
        client->println("               <option value=\"9\">9</option>");
        client->println("               <option value=\"10\">10</option>");
        client->println("               <option value=\"11\">11</option>");
        client->println("               <option value=\"12\">12</option>");
        client->println("            </select>");
        client->println("            <br>");
        client->println("            <label for=\"year\">Year: </label>");
        client->println("            <input type=\"text\" id=\"year\" name=\"year\" value=\"2022\">");
        client->println("            <br>");
        client->println("            <input type =\"submit\" value =\"Get\">");
        client->println("        </form>");
        client->println("        <a href=\"/\"> <button>Back</button> </a>");
        client->println("        <br>");
        client->println("        <a href=\"/byworker/");
        client->println(wanted);
        client->println("\">");
        client->println(wanted);
        client->println("</a>");
        client->println("      </div>");
        client->println("    </div>");
        client->println("  </div>");
        client->println(" </body>");
        client->println("</html>");
        /*  if ((sd).begin(15, SD_SCK_MHZ(25)))
            {
            File dir;
            File tempFile;
            char strTemp[64];
            Serial.println(dir.open("/"));
            while (tempFile.openNext(&dir, O_RDONLY)) {
            memset(strTemp, 0, 64 * sizeof(char));
            tempFile.getName(strTemp, 64);
            if (strstr(strTemp, wanted))
            {
            file.open(strTemp, O_RDONLY);
             strstr(strTemp, wanted) = '\0';
             strstr(strTemp, "_") = ' ';
             strstr(strTemp, "_") = ' ';
            client->write(strTemp, strlen(strTemp));
            client->write('\n');
            char *ps = (char*)ps_malloc(10000);
            memset(ps, 0, 10000 * sizeof(char));
            file.read(ps, 10000);
            //Serial.println(ps);
            file.close();
            client->write(ps, strlen(ps));
            free(ps);
            client->write('\n');
            client->write('\n');
            }
            tempFile.close();
            }
            dir.close();
            }*/
    }
    else if (strstr(buffer, "/byworker/"))
    {
        client->println("HTTP/1.1 200 OK");
        client->println("Content-type:file/csv");
        client->println("Connection: close");
        client->println();
        char *temp;
        temp = strstr(buffer, "/byworker/") + 10;
        char wanted[8];
        int i = 0;
        while (temp[i] != '.' && i < 8)
        {
            wanted[i] = temp[i];
            i++;
        }
        wanted[i] = '\0';
        // Serial.println(wanted);
        if ((sd).begin(15, SD_SCK_MHZ(25)))
        {
            File dir;
            File tempFile;
            File file;
            char strTemp[64];
            dir.open("/");
            while (tempFile.openNext(&dir, O_RDONLY))
            {
                memset(strTemp, 0, 64 * sizeof(char));
                tempFile.getName(strTemp, 64);
                if (strstr(strTemp, wanted))
                {
                    char *pointerName = strTemp + 5;
                    file.open(strTemp, O_RDONLY);
                    *strstr(pointerName, "_") = ' ';
                    *strstr(pointerName, ".") = '\0';
                    client->write(pointerName, strlen(pointerName));
                    client->write('\n');
                    char *ps = (char *)ps_malloc(10000);
                    memset(ps, 0, 10000 * sizeof(char));
                    file.read(ps, 10000);
                    // Serial.println(ps);
                    file.close();
                    client->write(ps, strlen(ps));
                    free(ps);
                    client->write('\n');
                    client->write('\n');
                }
                tempFile.close();
            }
            dir.close();
        }
    }

    else if (strstr(buffer, "/byworker"))
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
        client->println("        <form action=\"/byworker/\" method=\"GET\">");
        client->println("          <p>");
        client->println("            <label for=\"month\">Choose month:</label>");
        client->print("            <select name=\"month\" id=\"month\" value=\"");
        client->print(display.rtcGetMonth());
        client->println("\">");
        client->println("               <option value=\"1\">1</option>");
        client->println("               <option value=\"2\">2</option>");
        client->println("               <option value=\"3\">3</option>");
        client->println("               <option value=\"4\">4</option>");
        client->println("               <option value=\"5\">5</option>");
        client->println("               <option value=\"6\">6</option>");
        client->println("               <option value=\"7\">7</option>");
        client->println("               <option value=\"8\">8</option>");
        client->println("               <option value=\"9\">9</option>");
        client->println("               <option value=\"10\">10</option>");
        client->println("               <option value=\"11\">11</option>");
        client->println("               <option value=\"12\">12</option>");
        client->println("            </select>");
        client->println("            <br>");
        client->println("            <label for=\"year\">Year: </label>");
        client->println("            <input type=\"text\" id=\"year\" name=\"year\" value=\"2022\">");
        client->println("            <br>");
        client->println("            <input type =\"submit\" value =\"Get\">");
        client->println("        </form>");
        client->println("        <a href=\"/\"> <button>Back</button> </a>");
        client->println("      </div>");
        client->println("    </div>");
        client->println("  </div>");
        client->println(" </body>");
        client->println("</html>");
    }

    else if (strstr(buffer, "/download"))
    {
        client->println("HTTP/1.1 200 OK");
        client->println("Content-type:file/csv");
        client->println("Connection: close");
        client->println();
        sd.begin(15, SD_SCK_MHZ(25));
        char *temp = strstr(buffer, "/download/") + 9;
        *(strstr(temp, ".") + 4) = '\0';
        char fileName[48];
        strcpy(fileName, temp);
        // Serial.print("File name: ");
        // Serial.println(fileName);
        char *ps = (char *)ps_malloc(10000);
        memset(ps, 0, 10000 * sizeof(char));
        // Serial.print("File opened: ");
        File file;
        // Serial.println(file.open(fileName, FILE_READ));
        file.read(ps, 10000);
        // Serial.print("PS: ");
        // Serial.println(ps);
        file.close();
        client->write(ps, strlen(ps));
        free(ps);
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

        client->println("            <input type =\"submit\" value =\"Remove\">");
        client->println("        </form>");
        client->println("        <a href=\"/\"> <button>Back</button> </a>");
        client->println("      </div>");
        client->println("    </div>");
        client->println("  </div>");
        client->println(" </body>");
        client->println("</html>");
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
