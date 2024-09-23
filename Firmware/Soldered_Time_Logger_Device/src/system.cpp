// Include main header file.
#include "system.h"
#include "linkedList.h"
#include "logging.h"
#include "Inkplate.h"
#include "defines.h"
#include "gui.h"
#include <esp_wifi.h>
#include "ArduinoJson.h"
#include <ESPmDNS.h>

// Funcion for making a sound. Buzzer must be connected on the IO expaned GPIO15 pin with the MOSFET)
// Do not connect buzzer directly to the IO expander!
void buzzer(Inkplate *_display, uint8_t n, int _ms)
{
    for (int i = 0; i < n; i++)
    {
        unsigned long _timer1 = millis();
        while ((unsigned long)(millis() - _timer1) < _ms)
        {
            _display->digitalWriteIO(BUZZER_PIN, HIGH, IO_INT_ADDR);
            _display->digitalWriteIO(BUZZER_PIN, LOW, IO_INT_ADDR);
        }
        _timer1 = millis();
        while ((unsigned long)(millis() - _timer1) < _ms)
            ;
    }
}

// Device system initialization. Connects to the WiFi, starts the server, starts mDNS, gets the time from API, initializes logger library.
int deviceInit(Inkplate *_display, char *_ssid, char *_password, WiFiServer *_server, LinkedList *_myList, Logging *_logger, IPAddress _localIP, IPAddress _gateway, IPAddress _subnet, IPAddress _primaryDNS, IPAddress _secondaryDNS)
{
    // Initialize serial communication for debug.
    Serial.begin(115200);

    // Initialuie second HW Serial for RFID.
    Serial2.begin(57600, SERIAL_8N1, 39, -1);

    // Set GPIO on I/O expander for the buuzer.
    _display->pinModeIO(BUZZER_PIN, OUTPUT, IO_INT_ADDR);

    // Check the microSD card. Stop if failed (no data storage - no point of going any further).
    _display->print("Checking microSD Card");
    _display->partialUpdate(0, 1);
    if (!sd.begin(15, SD_SCK_MHZ(10)))
        errorDisplay(_display, "\nError occured. No SD card inserted. Please insert SD Card.\nIf you don't have SD card, please contact gazda.", false, true);

    // Configure IP address - use static IP.
    _display->print("\nConfiguring IP Adresses");
    _display->partialUpdate(0, 0);

    // High current consumption due WiFi and epaper activity causes reset of the ESP32. So wait a little bit.
    delay(1000);

    // Confingure IP Adress, subnet, DNS.
    if (!WiFi.config(_localIP, _gateway, _subnet, _primaryDNS, _secondaryDNS))
    {
        _display->println("\nSTA Failed to configure, please reset Inkplate! If error keeps to repeat, try to cnofigure "
                        "STA in different way or use another IP address");
    }

    // Connect to the WiFi. Set WiFi in station mode only.
    WiFi.mode(WIFI_STA);

    // Disable any power saving, this device must be on network 24/7.
    esp_wifi_set_ps(WIFI_PS_NONE);

    // Try to connect to the network.
    WiFi.begin(_ssid, _password);

    // Wait until WiFi connection is established.
    if (WiFi.status() != WL_CONNECTED)
    {
        int cnt = 0;
        _display->print("\nWaiting for WiFi to connect");
        _display->partialUpdate(0, 1);
        while ((WiFi.status() != WL_CONNECTED))
        {
            // Prints a dot every second that wifi isn't connected
            _display->print(F("."));
            _display->partialUpdate(0, 1);
            delay(1000);
            ++cnt;

            // If after 50 secnods there is still no WiFi connection, show error code and halt the device!
            if (cnt == 50)
                errorDisplay(_display, "WiFi Connection failed, device stopped!\nCheck the WiFi network and reset the device.", false, true);
        }
    }

    // Create mDNS for easier URL.
    _display->print("\nCreating mDNS service ");
    _display->partialUpdate(0, 1);
    if (!MDNS.begin(ESP32_MDNS_NAME))
        errorDisplay(_display, "mDNS set failed. Device halted. Please check the network and reset the device.", false, true);

    // Start server.
    _display->print("\nStarting the web server");
    _display->partialUpdate();
    _server->begin();
    _display->print("\nGetting the time from API service ");
    _display->partialUpdate();
    if (!updateTime(_display, 10, 500ULL))
        errorDisplay(_display, "Failed!", false, true);

    // Initialize library for logging functions. Send address of the SdFat object and Linked List object as parameters
    // (needed by the library).
    _display->print("\nReading data from SD card ");
    _display->partialUpdate();
    if (_logger->begin(&sd, _myList, _display) != 1)
        errorDisplay(_display, "Failed. Check microSD card (files, folders etc)", false, true);

    return 1;
}

// Get the time from the web (wolrdTimeApi). If everything went ok, Inkplate RTC will be set to the correct time.
// Otherwise, funciton will return 0 and clock won't be set.
int fetchTime(Inkplate *_display)
{
    HTTPClient _http;
    DynamicJsonDocument _doc(500);
    DeserializationError _err;
    char _temp[500];
    char _apiUrl[100];

    // Create URL for the API Call.
    sprintf(_apiUrl, "https://worldtimeapi.org/api/timezone/%s/%s", API_CLOCK_CONTINENT, API_CLOCK_REGION);

    // Try to get the JSON.
    _http.begin(_apiUrl);

    // Send a request.
    int _httpCode = _http.GET();

    // If the result is 200 OK, read asnd parse the JSON with clock data.
    if (_httpCode == HTTP_CODE_OK)
    {
        // Capture the data.
        strncpy(_temp, _http.getString().c_str(), sizeof(_temp) - 1);

        // Deserialize the JSON file.
        _err = deserializeJson(_doc, _temp);

        // Deserilization error=? Return error.
        if (_err)
            return 0;

        // No entry in JSON called "unixtime"? Return error.
        if (!_doc.containsKey("unixtime"))
            return 0;

        // Get the clock!
        int32_t _epoch = (int32_t)_doc["unixtime"];
        int32_t _epochTimezoneOffset = (int32_t)_doc["raw_offset"];
        int32_t _epochDstOffset = (int32_t)_doc["dst_offset"];

        // Set it on Inkplate RTC.
        _display->rtcSetEpoch(_epoch + _epochTimezoneOffset + _epochDstOffset);
        _display->rtcGetRtcData();
    }
    else
    {
        // No HTTP OK? Return error.
        return 0;
    }

    // Stop the client (just in case).
    _http.end();

    // If you got this far, that means clock is set, return success.
    return 1;
}

// Try to get time from API in multiple attempts.
bool updateTime(Inkplate *_display, uint8_t _retries, uint16_t _delay)
{
    // Try to get the time. If failed, try again multiple times.
    for (int i = 0; i < _retries; i++)
    {
        // Try to get the time from the API.
        if (!fetchTime(_display))
        {
            // If failed, wait a little bit before next attempt.
            delay(_delay);
        }
        else
        {
            // Got the time? Return success!
            return true;
        }
    }

    // Still no time? Return false!
    return false;
}