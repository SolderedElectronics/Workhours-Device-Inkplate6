#include <ArduinoJson.h>
#include <time.h>
#include "Inkplate.h"            //Include Inkplate library to the sketch
#include "SdFat.h"
#include "include/Inter16pt7b.h"
#include "include/Inter12pt7b.h"
#include "include/Inter8pt7b.h"
#include "include/mainUI.h"
#include "esp_wifi.h"
#include "favicon.h"

#define DEBOUNCE_SCAN 1000 // Miliseconds
#define LOG_SCREEN_TIME 10000 // Miliseconds
#define TIMEOUT_AT_LOGGING 60 * 10 // seconds * minutes 
#define MAX_SHIFT_LASTING 60 * 60 * 14 // seconds * minutes * hours

Inkplate display(INKPLATE_1BIT); // Create an object on Inkplate library and also set library into 1 Bit mode (BW)
StaticJsonDocument<30000> doc; // Buffer for JSON document
WiFiServer server(80);

uint32_t refreshes = 0;
time_t read_epoch;
// Set your Static IP address
IPAddress local_IP(192, 168, 71, 152);  // IP address should be set to desired address
// Set your Gateway IP address
IPAddress gateway(192, 168, 71, 1);   // Gateway address should match IP address

IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional

char ssid[] = "";
char pass[] = "";
time_t last_scan = 0, epoch = 0, log_shown = 0;
uint32_t tagID = 0;
uint8_t buffer[16], prev_hours = 0, prev_minutes = 0;
bool change_needed = 1, login_screen_shown = 0;

const char wday_name[7][10] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};
const char mon_name[12][4] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

struct worker {
    char name[20];
    char lname[20];
    char image[128];
    uint32_t ID;
    struct worker *next;
};

struct worker *workers = NULL;
struct worker *curr_worker = NULL;

void setup()
{
    Serial.begin(115200);
    Serial2.begin(57600, SERIAL_8N1, 39, -1); // Software Serial for RFID
    display.begin();        // Init Inkplate library (you should call this function ONLY ONCE)
    display.clearDisplay(); // Clear frame buffer of display
    display.display();      // Put clear image on display
    display.sdCardInit();
    display.rtcReset();
    display.setFont(&Inter16pt7b);
    display.setCursor(10, 30);
    display.pinModeMCP(0, OUTPUT);

    if (!sd.begin(15, SD_SCK_MHZ(10)))
    {
        errorDisplay();
        display.partialUpdate(0, 1);
        while (1);
    }

    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
        display.println("STA Failed to configure, please reset Inkplate! If error keeps to repeat, try to cnofigure STA in different way or use another IP address");
    }

    WiFi.mode(WIFI_STA);
    esp_wifi_set_ps(WIFI_PS_NONE);
    WiFi.begin(ssid, pass);
    if (WiFi.status() != WL_CONNECTED)
    {
        WiFi.reconnect();

        delay(5000);

        int cnt = 0;
        display.println(F("Waiting for WiFi to connect..."));
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

    fetchTime();

    display.setTextColor(BLACK, WHITE);

    loadWorkersFromSD();

    change_needed = 1;
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
        Serial.println("Reconnecting");
    }
    if (Serial2.available()) // Check if tag is scanned
    {
        memset(buffer, 0, 8 * sizeof(uint8_t));
        last_scan = millis();
        uint8_t cnt = 0;
        while (Serial2.available() && Serial2.peek() != '\r')
        {
            buffer[cnt] = Serial2.read();
            cnt++;
        }
        while (Serial2.available())
            Serial2.read();
        tagID = 0;
        for (int i = 0; i < cnt; i++)
        {
            tagID *= 10;
            tagID += (buffer[i] - 48);
        }
        logScan(tagID,0);
        Serial.println(tagID);
    }

    display.rtcGetRtcData(); // Get RTC data

    if (log_shown + LOG_SCREEN_TIME < millis() || millis() < LOG_SCREEN_TIME) //If login/logout screen is shown
    {
        display.clearDisplay(); // Clear screen
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

    prev_hours = display.rtcGetHour();
    prev_minutes = display.rtcGetMinute();

    if (prev_hours == 23 && display.rtcGetHour() == 0) // If midnight, call function checkDaily and get time
    {
        checkDaily();
        fetchTime();
    }

}

void saveWorkersToSD()
{
    Serial.println("Save workers!");
    if (sd.begin(15, SD_SCK_MHZ(10)))
    {
        if (sd.exists("/workers.txt"))
        {
            //Serial.print("Removed");
            Serial.println(sd.remove("/workers.txt"));
        }
        File file;
        Serial.println(file.open("/workers.txt", FILE_WRITE));
        char temp[128];
        curr_worker = workers;
        while (curr_worker != NULL)
        {
            Serial.println("Iterate");
            sprintf(temp, "; %s %s %d %s \r\n", curr_worker->name, curr_worker->lname, curr_worker->ID, curr_worker->image);
            Serial.println(temp);
            file.write(temp);
            Serial.println((int)curr_worker);
            curr_worker = curr_worker->next;
        }
        file.close();
    }
    else
    {
        //Serial.println("SD not inited!");
    }
}

void removeAllWorkers()
{
    Serial.println("Remove workers");
    while (workers != NULL)
    {
        curr_worker = workers;
        workers = curr_worker->next;
        Serial.println((int)curr_worker);
        free(curr_worker);
    }
    workers = NULL;
    curr_worker = NULL;
}

void loadWorkersFromSD()
{
    Serial.println("Load workers");
    removeAllWorkers();
    if ((sd).begin(15, SD_SCK_MHZ(10)))
    {
        File file;
        if (file.open("/workers.txt", FILE_READ));
        {
            if (file.available())
            {
                char *temp = (char*)ps_malloc(10000);
                file.read(temp, 10000);
                uint16_t cnt = 0;
                char *nextWorker = temp, *end = strstr(temp, "\0");
                struct worker *last = NULL;
                while (nextWorker != NULL)
                {
                    last = curr_worker;
                    if (workers == NULL)
                    {
                        workers = (struct worker*)ps_malloc(1 * sizeof(struct worker));
                        curr_worker = workers;
                        curr_worker->next = NULL;
                    }
                    else
                    {
                        curr_worker->next = (struct worker*)ps_malloc(1 * sizeof(struct worker));
                        curr_worker = curr_worker->next;
                    }
                    int i = 0;
                    char l[20], n[20];
                    if (sscanf(nextWorker, "; %s %s %d %s", curr_worker->name, curr_worker->lname, &(curr_worker->ID), curr_worker->image) == 4)
                    {
                        //Serial.print("Worker found with ID");
                        //Serial.println(curr_worker->ID);
                        curr_worker->next = NULL;
                    }
                    else
                    {
                        free(curr_worker); // KRITIK
                        curr_worker = last;
                    }
                    nextWorker = strstr(nextWorker + 1, ";");

                }
                file.close();
                free(temp);
            }
            else
            {
                //Serial.println("File not available!");
            }
            file.close();
        }
        /*  else
            {
            Serial.println("No workers.txt file!");
            }*/
    }
    else
    {
        //Serial.println("No SD Card!");
    }
    curr_worker = workers;
    Serial.println("Print next addresses:");
    while (curr_worker != NULL)
    {
        Serial.println((int)curr_worker->next);
        curr_worker = curr_worker->next;
    }
    Serial.println("Done!");
}

void logScan(uint32_t _tagID,bool daily)
{
    curr_worker = workers;
    while (curr_worker != NULL)
    {
        if (_tagID != curr_worker->ID)
        {
            curr_worker = curr_worker->next;
        }
        else
        {
            break;
        }
    }

    if (curr_worker != NULL)
    {
        File file;
        display.rtcGetRtcData();
        char fileName[48];
        sprintf(fileName, "%s_%s.txt", curr_worker->name, curr_worker->lname);
        if (sd.begin(15, SD_SCK_MHZ(10)))
        {
            //time_t read_epoch;
            uint8_t log_type;
            if (!sd.exists(fileName))
            {
                file.open(fileName, FILE_WRITE);
                char to_write[48];
                sprintf(to_write, "%d %d ", display.rtcGetEpoch(), 1);
                read_epoch = display.rtcGetEpoch();
                log_type = 1;
                file.print(to_write);
                file.close();
                login();
                return;
            }

            else
            {
                file.open(fileName, FILE_READ);
                char to_write[48];
                file.read(to_write, 48);
                Serial.println(to_write);
                sscanf(to_write, "%d %d ", &read_epoch, &log_type);
                file.close();
            }

            time_t temp_epoch = display.rtcGetEpoch();

            if ((read_epoch + TIMEOUT_AT_LOGGING) < temp_epoch)
            {
                if (log_type)
                {
                    sprintf(fileName, "%c%d%c%d%c%s%c%s%s", '/', display.rtcGetMonth(), '_', display.rtcGetYear() - 2000, '_',
                            curr_worker->lname, '_', curr_worker->name, ".csv");
                    if (!(sd).exists(fileName))
                    {
                        file.open(fileName, FILE_WRITE);
                        char header[] = "Datum;Ulazak;Izlazak;Provedeno\n";
                        file.print(header);
                    }
                    else
                    {
                        file.open(fileName, FILE_WRITE);
                    }

                    char temp[40];
                    struct tm t;
                    gmtime_r(&read_epoch, &t);

                    if (((temp_epoch - read_epoch) > MAX_SHIFT_LASTING) || (display.rtcGetDay() != t.tm_mday))
                    {
                        sprintf(temp, "%.2d%c%.2d%c%.2d%c%.2d%c%c%c%c%c%c%c%c%c", t.tm_mday, '.', t.tm_mon,
                                ';', t.tm_hour, ':', t.tm_min, ';', '?', ':', '?', ';',
                                '?', ':', '?', '\n');

                        file.print(temp);
                        file.close();
                        sprintf(fileName, "%s_%s.txt", curr_worker->name, curr_worker->lname);
                        sd.remove(fileName);
                        file.open(fileName, FILE_WRITE);
                        Serial.println(fileName);
                        char to_write[48];
                        if(daily)
                            sprintf(to_write, "%d %d", display.rtcGetEpoch(), 0);
                        else
                            sprintf(to_write, "%d %d", display.rtcGetEpoch(), 1);
                        Serial.println(to_write);
                        Serial.println(daily);
                        file.print(to_write);
                        file.close();
                        login();
                        return;
                    }

                    else
                    {
                        uint8_t hours, minutes;
                        hours = display.rtcGetHour() - t.tm_hour;

                        if (display.rtcGetMinute() >= t.tm_min)
                        {
                            minutes = display.rtcGetMinute() - t.tm_min;
                        }

                        else
                        {
                            minutes = 60 - t.tm_min + display.rtcGetMinute();
                            hours--;
                        }

                        sprintf(temp, "%.2d%c%.2d%c%.2d%c%.2d%c%.2d%c%.2d%c%.2d%c%.2d%c", display.rtcGetDay(), '.', display.rtcGetMonth(),
                                ';', t.tm_hour, ':', t.tm_min, ';', display.rtcGetHour(), ':', display.rtcGetMinute(), ';',
                                hours, ':', minutes, '\n');
                        logout(hours, minutes);
                    }
                    Serial.println(temp);
                    file.print(temp);
                    file.close();

                    sprintf(fileName, "%s_%s.txt", curr_worker->name, curr_worker->lname);
                    sd.remove(fileName);
                    file.open(fileName, FILE_WRITE);
                    char to_write[48];
                    sprintf(to_write, "%d %d", display.rtcGetEpoch(), 0);
                    file.print(to_write);
                    file.close();
                }
                else
                {
                    sprintf(fileName, "%s_%s.txt", curr_worker->name, curr_worker->lname);
                    sd.remove(fileName);
                    file.open(fileName, FILE_WRITE);
                    char to_write[48];
                    sprintf(to_write, "%d %d", display.rtcGetEpoch(), 1);
                    file.print(to_write);
                    file.close();
                    login();
                }
            }
            else
            {
                Serial.println("Already scanned!");
                alreadyLogged();
            }
        }
    }
    else
    {
        unknownTag(_tagID);
    }
}

void checkDaily()
{
    curr_worker = workers;
    while (curr_worker != NULL)
    {
        char temp[32];
        sprintf(temp, "%s_%s.txt", curr_worker->name, curr_worker->lname);
        File tempFile;
        uint8_t log_type = 0;
        if (tempFile.open(temp, O_RDONLY))
        {
            char to_write[48];
            tempFile.read(to_write, 48);
            sscanf(to_write, "%d %d ", &read_epoch, &log_type);
            tempFile.close();
            if (log_type)
            {
                logScan(curr_worker->ID,1);
                Serial.println("Dislogged!");
            }
        }
        curr_worker = curr_worker->next;
    }
}

// Time fetching

void fetchTime()
{
    HTTPClient http;
    http.getStream().setTimeout(10);
    http.getStream().flush();
    http.begin("https://www.timeapi.io/api/Time/current/zone?timeZone=Europe/Zagreb");
    int httpCode;
    do
    {
        httpCode = http.GET();
    } while (httpCode != 200);
    display.setCursor(10, 90);
    display.print("Getting time...");
    display.partialUpdate(0, 1);
    if (httpCode == 200)
    {
        while (http.getStream().available() && http.getStream().peek() != '{')
            (void)http.getStream().read();
        DeserializationError error = deserializeJson(doc, http.getStream());
        if (error)
        {
            //Serial.print(F("deserializeJson() failed: "));
            //Serial.println(error.c_str());
        }
        else
        {
            struct tm t;
            t.tm_year = (int)doc["year"] - 1900;
            t.tm_mon = (int)doc["month"] - 1;
            t.tm_mday = doc["day"];
            t.tm_hour = doc["hour"];
            t.tm_min = doc["minute"];
            t.tm_sec = doc["seconds"];
            const char *weekday_string = doc["dayOfWeek"];
            for (int i = 0; i < 7 ; i++)
            {
                if (strstr(wday_name[i], weekday_string))
                {
                    t.tm_wday = i;
                }
            }
            t.tm_isdst = 0;
            display.rtcSetTime(t.tm_hour, t.tm_min, t.tm_sec);
            display.rtcSetDate(t.tm_wday, t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
            display.rtcGetRtcData();
        }
    }
    else if (httpCode == 404)
    {
        //Serial.println(F("Time has not been fetched!"));
    }
    doc.clear();
    http.end();
}

//Display screens

void login()
{
    // Code for login screen
    buzz(1);
    display.setTextSize(1);
    display.setFont(&Inter16pt7b);
    display.setCursor(250, 360);
    display.clearDisplay();
    display.print("First name:  ");
    display.print(curr_worker->name);
    display.setCursor(250, 400);
    display.print("Last name:  ");
    display.print(curr_worker->lname);
    display.setCursor(250, 440);
    display.print("ID:  ");
    display.print(curr_worker->ID);
    display.setCursor(250, 530);
    display.print("LOGIN");
    log_shown = millis();
    change_needed = 1;
    login_screen_shown = 1;
    Serial.print("Image drawn: ");
    if (!(display.drawImage(curr_worker->image, 250, 20, 1, 0)))
    {
        display.drawImage("Normalna_slika.bmp", 250, 20, 1, 0);
    }
}

void logout(uint8_t hours, uint8_t minutes)
{
    // Code for logout screen
    buzz(1);
    display.setTextSize(1);
    display.setFont(&Inter16pt7b);
    display.setCursor(250, 360);
    display.clearDisplay();
    display.print("Name:  ");
    display.print(curr_worker->name);
    display.setCursor(250, 400);
    display.print("Last name:  ");
    display.print(curr_worker->lname);
    display.setCursor(250, 440);
    display.print("ID:  ");
    display.print(curr_worker->ID);
    display.setCursor(250, 530);
    display.print("LOGOUT");
    display.setCursor(250, 570);
    display.print("Worked: ");
    hours < 10 ? display.print("0") : 0;
    display.print(hours);
    display.print(":");
    minutes < 10 ? display.print("0") : 0;
    display.print(minutes);
    log_shown = millis();
    change_needed = 1;
    login_screen_shown = 1;
    if (!(display.drawImage(curr_worker->image, 250, 20, 1, 0)))
    {
        display.drawImage("Normalna_slika.bmp", 250, 20, 1, 0);
    }
}

void alreadyLogged()
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

void unknownTag(uint32_t _tagID)
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
    display.print("Error occured. No SD card inserted. Please insert SD Card. If you don't have SD card, please contact gazda.");
    display.display();
    log_shown = millis();
}

void buzz(uint8_t n)
{
    Serial.println("Buzz");
    for (int i = 0; i < n; i++)
    {
        Serial.println("For");
        display.digitalWriteMCP(0, HIGH);
        delay(150);
        display.digitalWriteMCP(0, LOW);
        delay(150);
    }
}

void doServer(WiFiClient * client)
{
    char buffer[256];
    client->read((uint8_t*)buffer, 256);
    Serial.println(buffer);
    Serial.println("\n\n");
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
        sd.begin(15, SD_SCK_MHZ(25));
        char *ps = (char*)ps_malloc(50000);
        File file;
        Serial.println(file.open("/inter.txt", FILE_READ));
        int size_of_file = file.size();
        memset(ps, 0, size_of_file * sizeof(char));
        file.read(ps, size_of_file);
        file.close();
        client->write(ps, size_of_file);
        free(ps);
    }

    else if (strstr(buffer, "addworker/?"))
    {
        client->println("HTTP/1.1 200 OK");
        client->println("Content-type:text/html");
        client->println("Connection: close");
        client->println();
        bool flag_exist = 0;
        int temp_id = 0;
        char temp_name[20], temp_lname[20], temp_image[128];
        char *temp = strstr(buffer, "/?") + 2;
        uint16_t cnt = 0;
        loadWorkersFromSD();
        while (temp[cnt] != '\0')
        {
            if (temp[cnt] == '&')
                temp[cnt] = ' ';
            cnt++;
        }
        if (strstr(temp, "name") && strstr(temp, "lname") && strstr(temp, "tagID") && strstr(temp, "image"))
        {
            sscanf(temp, "name=%s lname=%s tagID=%d image=%s", temp_name, temp_lname, &(temp_id), temp_image);
            Serial.println(temp_name);
            Serial.println(temp_lname);
            Serial.println(temp_id);
            Serial.println(temp_image);
            if (workers == NULL)
            {
                workers = (struct worker*)ps_malloc(1 * sizeof(struct worker));
                curr_worker = workers;
                strcpy(curr_worker->name, temp_name);
                strcpy(curr_worker->lname, temp_lname);
                curr_worker->ID = temp_id;
                strcpy(curr_worker->image, temp_image);
                curr_worker->next = NULL;
            }

            else
            {
                curr_worker = workers;
                while (curr_worker != NULL)
                {
                    if (curr_worker->ID == temp_id)
                    {
                        flag_exist = 1;
                        break;
                    }
                    curr_worker = curr_worker->next;
                }
                if (!flag_exist)
                {
                    curr_worker = workers;
                    while (curr_worker->next != NULL)
                        curr_worker = curr_worker->next;
                    curr_worker->next = (struct worker*)ps_malloc(1 * sizeof(struct worker));
                    if (curr_worker->next)
                    {
                        Serial.println("Zauzet!");
                        curr_worker = curr_worker->next;
                        strcpy(curr_worker->name, temp_name);
                        strcpy(curr_worker->lname, temp_lname);
                        curr_worker->ID = temp_id;
                        strcpy(curr_worker->image, temp_image);
                        curr_worker->next = NULL;
                    }
                }
            }
            Serial.print("0: ");
            Serial.println(curr_worker->name);
            Serial.print("1: ");
            Serial.println(curr_worker->lname);
            Serial.print("2: ");
            Serial.println(curr_worker->ID);
            Serial.print("3: ");
            Serial.println(curr_worker->image);
            Serial.println("Temp:");
            Serial.println(temp_name);
            Serial.println(temp_lname);
            Serial.println(temp_id);
            Serial.println(temp_image);
            Serial.println("End temp!");
            saveWorkersToSD();
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
        if (flag_exist)
        {
            client->println("        <p>Worker with ID already exist!</p>");
        }
        else
        {
            client->println("        <p>Worker added successfully!</p>");
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
        char *temp = strstr(buffer, "/?remove") + 9;
        uint32_t id_to_remove = 0;
        //Serial.println("");
        //Serial.println(temp);
        sscanf(temp, "%d ", &id_to_remove);
        struct worker *prev_worker = NULL;
        curr_worker = workers;
        while (/*curr_worker->ID != id_to_remove && */curr_worker != NULL)
        {
            //Serial.print("Checking user with ID: ");
            //Serial.println(curr_worker->ID);
            //Serial.print("ID to compare: ");
            //Serial.println(id_to_remove);
            if (curr_worker->ID == id_to_remove)
            {
                if (prev_worker == NULL)
                {
                    workers = curr_worker->next;
                }
                else
                {
                    prev_worker->next = curr_worker->next;
                }
                free(curr_worker);
                curr_worker = workers;
                break;
            }
            prev_worker = curr_worker;
            curr_worker = curr_worker->next;
        }
        saveWorkersToSD();
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
                while (tempFile.openNext(&dir, O_RDONLY)) {
                    memset(strTemp, 0, 56 * sizeof(char));
                    tempFile.getName(strTemp, 56);
                    Serial.println(strTemp);
                    if (strstr(strTemp, ".csv") && strstr(strTemp, wname)) {
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
            client->println("               <option value=\"0\" selected=\"selected\" hidden=\"hidden\">No workers yet!</option>");
            client->println("            </select>");
            client->println("            <br>");
            client->println("        </form>");
        }
        else
        {
            while (curr_worker != NULL)
            {
                char tmp[86];
                sprintf(tmp, "               <option value=\"%s_%s\">%s %s</option>", curr_worker->lname, curr_worker->name, curr_worker->name, curr_worker->lname);
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
        Serial.println(wanted);
        if ((sd).begin(15, SD_SCK_MHZ(25)))
        {
            File dir;
            File tempFile;
            File file;
            char strTemp[64];
            dir.open("/");
            while (tempFile.openNext(&dir, O_RDONLY)) {
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
        Serial.print("File name: ");
        Serial.println(fileName);
        char *ps = (char*)ps_malloc(10000);
        memset(ps, 0, 10000 * sizeof(char));
        Serial.print("File opened: ");
        File file;
        Serial.println(file.open(fileName, FILE_READ));
        file.read(ps, 10000);
        Serial.print("PS: ");
        Serial.println(ps);
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
        curr_worker = workers;
        while (curr_worker != NULL)
        {
            client->print("            <input type =\"radio\" name=\"remove\" value =\"");
            //Serial.print("ID: ");
            //Serial.println(curr_worker->ID);
            client->print(curr_worker->ID); //KRITIK
            client->println("\">");
            client->print("            <label for=\"javascript\">");
            client->print(curr_worker->name);
            client->print(" ");
            client->print(curr_worker->lname);
            client->print(" (");
            client->print(curr_worker->ID);
            client->print(")");
            client->println("</label><br>");
            curr_worker = curr_worker->next;
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
