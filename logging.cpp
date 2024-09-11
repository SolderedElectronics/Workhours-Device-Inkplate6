#include "logging.h"

// Constructor. Empty...for now.
Logging::Logging()
{
}

int Logging::begin(SdFat *_s, LinkedList *_l, Inkplate *_i)
{
    // Copy objects addreesses into pointers for internal use.
    _sd = _s;
    _link = _l;
    _ink = _i;

    // First it's needed to load all our employees into memory (their Firt Name, Last Name, tag ID and employee image
    // filename).

    // File class.
    SdFile _file;

    // Init uSD card
    if (!_sd->begin(15, SD_SCK_MHZ(10)))
    {
        return 0;
    }

    // Set correct directory. First check if there is folder for this device. If it doesn't exists, make it!
    if (!_sd->exists(DEFAULT_FOLDER_NAME))
    {
        if (!_sd->mkdir(DEFAULT_FOLDER_NAME))
        {
            // Ooops, something went wrong... uSD card is full, or it's corrupted.
            return 0;
        }
    }

    // Now change to default directory / folder for this device. If you can't, card is corrupted.
    if (!_sd->chdir(DEFAULT_FOLDER_NAME))
    {
        return 0;
    }

    // Open a file with all employees.
    if (!_file.open("workers.csv", FILE_READ))
    {
        // Again, this should never happen!
        return 0;
    }

    // Now struct (linked list) with employees data.
    fillEmployees(&_file);

    // Close the file.
    _file.close();

    // Check for employee folders
    checkFolders();

    // Everything went ok? Return 1 as success! :)
    return 1;
}

int Logging::getEnteries(SdFile *_file)
{
    int _r = 0; // Counter for enteries. Acutally it just counts \n symbols.
    char c;

    // Read and count \n  symbols
    while (_file->available())
    {
        c = _file->read();
        if (c == '\n')
            _r++;
    }

    // Rewind file to the start.
    _file->rewind();

    // First line with description is ignored.
    return _r - 1;
}

int Logging::fillEmployees(SdFile *_file)
{
    // Temp arrays for storing data.
    char name[50];
    char lastName[50];
    char id[20];
    char image[200];
    char department[100];

    // Array for storing one line of .csv file.
    char oneLine[400];

    // Get total numbers of registred employees.
    int n = getEnteries(_file);

    // If returns -1 (empty file) or 0 (no registered employees), return error.
    if (n == -1 || n == 0)
        return 0;

    // Go to the end of the line
    while (_file->available() && _file->read() != '\n')
        ;

    // Copy employee data into structs
    for (int i = 0; i < n; i++)
    {
        int k = 0;
        char c;

        // Copy one line into array (copy it until it finds new line char -> \n)
        do
        {
            c = _file->read();
            oneLine[k++] = c;
        } while (_file->available() && c != '\n');

        // Add string terminating char.
        oneLine[k] = '\0';

        // Parse it using sscanf function. %[^';'] means that semicolon will terminate the string. For successful
        // parsing, sscanf() must find 4 variables / arrays.
        if (sscanf(oneLine, "%[^','], %[^','], %[^','], %[^','], %[^',']\r\n", name, lastName, id, department, image) ==
            5)
        {
            _link->addEmployee(name, lastName, atoi(id), image, department);
        }
    }

    // After all, clost the file.
    _file->close();

    // Everything went ok? Return 1.
    return 1;
}

int Logging::checkFolders()
{
    // Check if all employees have their folders on uSD card.
    // Folder stucture is WorkingHoursDevice\[EMPLOYEE_NAME_&_TAGID]\[YEAR]\logs...

    char _path[128];
    struct employeeData _temp;

    // Get the number of the elements from the linked list
    int _n = _link->numberOfElements();

    // If there are no elements -> No employees -> No folders. Go back!
    if (_n == 0)
        return 0;

    // Get the first employee.
    struct employeeData *_l = _link->getEmployee(0);

    // Initialize uSD card. If it fails, return 0 (fail). Could be usefull for error handling.
    if (!_sd->begin(15, SD_SCK_MHZ(10)))
    {
        return 0;
    }

    // Try to change to the working folder of this device. If can't do that, well... it's not good, this should never
    // happen!
    if (!_sd->chdir(DEFAULT_FOLDER_NAME))
    {
        return 0;
    }

    // Update RTC data (needed for the creating folders).
    _ink->rtcGetRtcData();

    // Go through the linked list (list of all employees).
    for (int i = 0; i < _n; i++)
    {
        // Get first name, last name and tag ID of the employee.
        strcpy(_temp.firstName, _l->firstName);
        strcpy(_temp.lastName, _l->lastName);
        _temp.ID = _l->ID;

        // Make folder name for employee.
        sprintf(_path, "%s%s%llu", _temp.firstName, _temp.lastName, _temp.ID);

        // Create it!
        _sd->mkdir(_path);

        // Go to the next element.
        _l = _l->next;

        // Now make folder in the current directory for current year.
        // First select the folder of the employee.
        _sd->chdir(_path);

        sprintf(_path, "%d", (int)(_ink->rtcGetYear()));

        // Create it!
        _sd->mkdir(_path);

        // Go back to the main working directory
        _sd->chdir();
        _sd->chdir(DEFAULT_FOLDER_NAME);
    }

    return 1;
}

int Logging::updateEmployeeFile()
{
    // First delete old workers.csv file

    // File class.
    SdFile _file;

    // Init uSD card
    if (!sd.begin(15, SD_SCK_MHZ(10)))
    {
        // Init has failed. Wrong connections, bad uSD card, wrong speed, wrong format?
        return 0;
    }

    // Change directory to the working directory of this device.
    if (!_sd->chdir(DEFAULT_FOLDER_NAME))
    {
        // No device working folder? Something is wrong...
        return 0;
    }

    // Try to delete wokreks file. No need for checking if delete was successful. If it is, great, if not, well maybe
    // it's already deleted, or it doesn't exists.
    _sd->remove("workers.csv");

    // Now make a new file with updated list.
    if (!_file.open("workers.csv", FILE_WRITE))
    {
        // File create has failed, return 0.
        return 0;
    }

    // Make a header for the .csv file (so user know what each column represents)
    _file.println("First name,Last Name,Tag ID,Department,Image Filename,");

    // Fill the file with employee data.

    // Variable for counting (going though linked list)
    int i = 0;

    // Try to get first element from linked list.
    struct employeeData *_list = _link->getEmployee(i++);

    // Go trough the list until until there are no more elements in the list.
    while (_list != NULL)
    {
        // Array for storing one row of data.
        char _oneRow[500];

        // Create one line of .csv file filled with data.
        sprintf(_oneRow, "%s,%s,%llu,%s,%s,\r\n", _list->firstName, _list->lastName,
                (unsigned long long)(_list->ID), _list->department, _list->image);

        // Write that line to the file on uSD.
        _file.print(_oneRow);

        // Go to the next element.
        _list = _link->getEmployee(i++);
    }

    // Close the file.
    _file.close();

    // Make a new folder for all new employees.
    checkFolders();

    // Everything went ok? Great! Return 1 for success.
    return 1;
}

bool Logging::getTagID(HardwareSerial *_serial, uint64_t *_tagIdData)
{
    // Try to parse tag ID. It is sent from Serial using ASCII code (decimal value). It needs to be converted int 64 bit
    // integer. Return true if is valid TagID, return false otherwise.
    bool _retValue = false;

    // Set default ID value for TagID to zero.
    *_tagIdData = 0;

    // Variable for serial timeout. The idea is to wait max 10ms from last received char from Serial.
    unsigned long _timer1 = millis();

    // Variable for counting how many char have we received.
    int _n = 0;

    // Array for storing chars from serial. Tags have max 10 digits + enter (LF, CR) and one more for '\0' = 13
    // elements.
    char _buffer[33];

    // Variable for storing one char from serial.
    char _c = 0;

    // Set whole buffer to 0.
    memset(_buffer, 0, sizeof(_buffer));

    // Try to catch all datra from RFID reader.
    while (((unsigned long)(millis() - _timer1) < 100))
    {
        if (_serial->available())
        {
            _c = _serial->read();
            if (_n < 31) _buffer[_n++] = _c;
            _timer1 = millis();
        }
    }

    // Try to catch start of the RFID.
    char* _rfidResponseStart = strchr(_buffer, '$');
    if (_rfidResponseStart != NULL)
    {
        // Add string terminating char at the end of the buffer.
        _buffer[_n] = 0;

        // Convert string to integer.
        sscanf(_rfidResponseStart, "$%llu&", _tagIdData);

        // If the data is vaild, return true.
        if (*_tagIdData != 0) _retValue = true;
    }

    // Return true for vaild TagID, otherwise return false.
    return _retValue;
}

int Logging::addLog(uint64_t _tagID, uint32_t _epoch, struct employeeData &_w)
{
    // If employee exists, tag it! If not, show error message.
    if (_tagID != 0)
    {
        if (_link->checkID(_tagID))
        {
            // Before tagging an employee, check if it's passed 10 minutes before last log.
            if (sd.begin(15, SD_SCK_MHZ(10)))
            {
                // Make Time & Date data from epoch.
                struct tm _myTime;
                memcpy(&_myTime, localtime((const time_t *)&_epoch), sizeof(_myTime));

                // Find employee in the linked list.
                struct employeeData *_employee = _link->getEmployeeByID(_tagID);
                SdFile myFile;

                // Now let's do some stuff with uSD card.
                char path[128];
                char filename[128];

                // Make a file path for this exact employee on uSD card. Path is
                // [root]/DEFAULT_FOLDER_NAME/FirstNameLastNameTagID/YEAR. For example:
                // [root]/WorkingHoursDevice/HarryPotter123456/2022/
                sprintf(path, "/%s/%s%s%llu/%d/", DEFAULT_FOLDER_NAME, &(_employee->firstName), &(_employee->lastName),
                        (unsigned long long)(_employee->ID), _myTime.tm_year + 1900);

                // Change this path to be now working directory.
                sd.chdir(path, true);

                // Create filename. File name looks something like this: FirstName_Year_Month_int.
                // For example: Harry_2022_09_int.csv
                sprintf(filename, "%s_%04d_%02d_int.csv", _employee->firstName, _myTime.tm_year + 1900,
                        _myTime.tm_mon + 1);

                // First check if this filename exists already. If not create it and make a header in the file (header
                // just names columns in the .csv file).
                if (!sd.exists(filename))
                {
                    // File doesn't exists, create it and make a header!
                    if (myFile.open(filename, O_CREAT | O_WRITE))
                    {
                        myFile.println(LOGGING_RAW_FILE_HEADER);
                        myFile.close();
                    }
                }
                // If file already exists, open it and make it available for reading as well as for writing.
                if (!myFile.open(filename, O_RDWR))
                    return LOGGING_TAG_ERROR;

                // After uSD init and the file has been opened, try to find last login/logout data on uSD card of this
                // employee.
                int32_t _lastLogEpoch;
                uint8_t _logInOutTag;

                // Read out every line of logged times and determine if the next log will be logout or login.
                if (!findLastEntry(&myFile, &_lastLogEpoch, &_logInOutTag))
                {
                    myFile.close();
                    return LOGGING_TAG_ERROR;
                }

                // Check if 10 min from last log have been passed. If not, return an error.
                if ((_epoch - _lastLogEpoch) < LOGGING_LOG_TIMEOUT)
                {
                    myFile.close();
                    return LOGGING_TAG_10MIN;
                }

                // Go to the end of the file.
                myFile.seekEnd();

                // Now check if the last logged time is valid.
                // For example, someone forgot to logout and now login time would be a logout time.
                // This is all good IF this happend in the same day, but problem is when it's new day.
                // So, first check if there is day difference in last login and current epoch.
                // If not, log is vaild, if not, logout needs to be invalid and now it's actually login time.
                int32_t _dayStartEpoch;
                int32_t _dayEndEpoch;
                calculateDayEpoch(_epoch, &_dayStartEpoch, &_dayEndEpoch);

                DEBUG_PRINT("Epoch: %ld, Start: %ld, End: %ld", _epoch, _dayStartEpoch, _dayEndEpoch);

                if ((_lastLogEpoch < _dayStartEpoch) && (_lastLogEpoch < _dayEndEpoch) && (_lastLogEpoch != 0) &&
                    (_logInOutTag == LOGGING_TAG_LOGOUT))
                {
                    // Add Special string defined in dataTypes.h
                    myFile.print(LOGGING_ERROR_STRING);
                    myFile.println();

                    // Change flag to login
                    _logInOutTag = LOGGING_TAG_LOGIN;
                }

                // Log the data!
                char oneLine[100];
                sprintf(oneLine, "%lu", _epoch);

                // If it's a logout tag, send a new line at the end of the string (log time), otherwise don't.
                if (_logInOutTag == LOGGING_TAG_LOGOUT)
                {
                    myFile.println(oneLine);

                    DEBUG_PRINT("Logout detected");
                }
                else
                {
                    myFile.print(oneLine);
                    myFile.print(',');

                    DEBUG_PRINT("Login detected");
                }

                // Close the file (send data to the uSD card).
                myFile.close();

                // Return to the root of the uSD card (and set it as current working directory)
                sd.chdir(true);

                // Send employee data (so it can be used in the main code)
                _w = *(_employee);

                // Return a flag
                return _logInOutTag;
            }
        }
        else
        {
            // Tag was not found in the database.
            return LOGGING_TAG_NOT_FOUND;
        }
    }
    // Unknown error...Hudston we have a problem!
    return LOGGING_TAG_ERROR;
}

int Logging::findLastEntry(SdFile *_f, int32_t *_epoch, uint8_t *_log)
{
    char _prevLine[250];
    char _currentLine[250];
    char _c;
    int _n = 0;
    int _k = 0;
    int _resultPrev;
    int _resultCurrent;
    unsigned long long _time1 = 0;
    unsigned long long _time2 = 0;
    unsigned long long _time3 = 0;
    unsigned long long _time4 = 0;

    // First count how much entries are in the file.
    _f->rewind();
    int _counter = 0;
    while (_f->available())
    {
        if (_f->read() == '\n')
            _counter++;
    }

    // If for some reason, there are no data in the file, return error.
    if (_counter == 0)
    {
        return LOGGING_TAG_ERROR;
    }

    // Go back to the begining of the file.
    _f->rewind();

    // Go to the line before the last one.
    do
    {
        if (_f->read() == '\n')
            _n++;
    } while (_f->available() && _n != (_counter - 1));

    // Copy one whole line into the buffer
    do
    {
        _c = _f->read();
        _prevLine[_k++] = _c;
    } while (_f->available() && _c != '\n');
    _prevLine[_k] = '\0';

    // Now, copy the last one into the buffer.
    _k = 0;
    do
    {
        _c = _f->read();
        _currentLine[_k++] = _c;
    } while (_f->available());

    // Add null-terminating char at the end of the string.
    _currentLine[_k] = '\0';

    // Try to parse line before the last line in the file.
    _resultPrev = sscanf(_prevLine, "%llu,%llu", &_time1, &_time2);

    DEBUG_PRINT("_resultPrev: %d", _resultPrev);

    // If line cannot be parsed, that means it's the header of the file (so this is the first line with acctual data)
    if (_resultPrev == 0)
    {
        // Now try to see if it's a empty line or it has login data
        _resultCurrent = sscanf(_currentLine, "%llu,%llu", &_time3, &_time4);

        if (_resultCurrent == 0 || _resultCurrent == -1)
        {
            // No log data -> first epoch in this file -> no prev epoch!
            *_log = LOGGING_TAG_LOGIN;
            *_epoch = 0;
            return 1;
        }
        else if (_resultCurrent == 1)
        {
            // Login data -> next log data is the logout data.
            *_log = LOGGING_TAG_LOGOUT;
            *_epoch = _time3;
            return 1;
        }
        else if (_resultCurrent == 2)
        {
            // Login and logout data? Next data is the login data,
            *_log = LOGGING_TAG_LOGIN;
            *_epoch = _time4;
            return 1;
        }
        else
        {
            return 0;
        }
    }

    // Try to parse current line and see what you got.
    _resultCurrent = sscanf(_currentLine, "%llu,%llu", &_time3, &_time4);
    DEBUG_PRINT("_resultCurrent: %d", _resultCurrent);

    if (_resultCurrent == 0)
    {
        // New line -> login data is inserted.
        *_log = LOGGING_TAG_LOGIN;
        *_epoch = _time2;
        return 1;
    }
    else if (_resultCurrent == 1)
    {
        // Login data -> next log data is the logout data.
        *_log = LOGGING_TAG_LOGOUT;
        *_epoch = _time3;
        return 1;
    }
    else if (_resultCurrent == 2)
    {
        // Login and logout data? Next data is the login data.
        *_log = LOGGING_TAG_LOGIN;
        *_epoch = _time4;
        return 1;
    }
    else
    {
        // Hudston, we have a problem...abort, abort!
        return 0;
    }

    // Same as before...abort, abort!
    return 0;
}


int32_t Logging::getEmployeeWeekHours(uint64_t _tagID, uint32_t _epoch)
{
    // First init uSD card. If uSD card can't be initialized, return -1 (error).
    if (!sd.begin(15, SD_SCK_MHZ(10)))
    {
        return 0;
    }

    // Change the current working directory to be root of the uSD card.
    sd.chdir(true);

    // Now try to get every login and logout data of this specific employee.
    int32_t _startWeekEpoch;
    int32_t _endWeekEpoch;
    int32_t _weekHours = 0;

    SdFile _fileCurrentMonth;
    SdFile _filePrevMonth;
    const time_t _epochInternal = _epoch;

    // Get all employee data (First name, last name, tagID, etc...)
    struct employeeData *_employee = _link->getEmployeeByID(_tagID);

    // Didn't find it? Return -1 indicating an error.
    if (_employee == NULL)
        return -1;

    // Get the start and end epoch of the current week.
    // Now, tricky stuff. But this function can save us! It calculates the epoch for first day of the week and the last
    // day of the week where current epoch lands. For example if epoch of 07.09.2022 15:05:14 is send it will return
    // epoch for 05.09.2022. 0:00:00 and 11.09.2022. 23:59:59. It will NOT handle situation where end of the month is in
    // the middle of the week.
    calculateWeekdayEpochs(_epoch, &_startWeekEpoch, &_endWeekEpoch);

    // Check if month change has occured in one week. If so, two files needs to be opened.
    if (monthChangeDeteced(_startWeekEpoch, _endWeekEpoch))
    {
        // Variables to store human readable time and date data.
        struct tm _prevMonthTd;
        struct tm _currentMonthTd;

        // Init the structs (needed for correct conversion).
        memset(&_prevMonthTd, 0, sizeof(_prevMonthTd));
        memset(&_currentMonthTd, 0, sizeof(_currentMonthTd));

        // Convert epoch to the human readable time and date.
        // Use start of the week as "prev. month" and end of the week as a "curent month"
        memcpy(&_prevMonthTd, localtime((const time_t*)(&_startWeekEpoch)), sizeof(_prevMonthTd));
        memcpy(&_currentMonthTd, localtime((const time_t*)(&_endWeekEpoch)), sizeof(_currentMonthTd));

        // Try to open file for the current month. Check only for prev. month as this time and date data are from start of the week epoch.
        // If we cannot opet first file, we def. cannot open a second (and also there is a situation where there is a month change in a week
        // but it's still not a "next month", so there is no file for it.)
        if (getEmployeeFile(&_filePrevMonth, _employee, _prevMonthTd.tm_mon + 1, _prevMonthTd.tm_year + 1900, 1))
        {
            // Try to open and get employee file for the "current month" (or next month).
            getEmployeeFile(&_fileCurrentMonth, _employee, _currentMonthTd.tm_mon + 1, _currentMonthTd.tm_year + 1900, 1);

            // Variables for logged time for both months.
            int32_t _loggedTimePrevMonth = 0;
            int32_t _loggedTimeCurrMonth = 0;

            // Get the week hours of both months (but for the same reason as mentioned above, open only "prev. month")
            if (getWorkHours(&_filePrevMonth, _startWeekEpoch, _endWeekEpoch, &_loggedTimePrevMonth))
            {
                getWorkHours(&_fileCurrentMonth, _startWeekEpoch, _endWeekEpoch, &_loggedTimeCurrMonth);

                // Sum both of the logged times (from prev and current month).
                _weekHours = _loggedTimeCurrMonth + _loggedTimePrevMonth;

                // Close the files.
                _filePrevMonth.close();
                _fileCurrentMonth.close();
            }
        }
        else
        {
            return -1;
        }
    }
    else
    {
        // Calculate the human time and date.
        struct tm _currentMonth;

        // Init the struct.
        memset(&_currentMonth, 0, sizeof(_currentMonth));

        // Calculate the time.
        memcpy(&_currentMonth, localtime(&_epochInternal), sizeof(_currentMonth));

        // If no month change is detected, normally open and read only one file (file for the current month).
        if (getEmployeeFile(&_fileCurrentMonth, _employee, _currentMonth.tm_mon + 1, _currentMonth.tm_year + 1900, 1))
        {
            int32_t _loggedTime = 0;
            // Get the logged hours.
            if (getWorkHours(&_fileCurrentMonth, _startWeekEpoch, _endWeekEpoch, &_loggedTime))
            {
                _weekHours = _loggedTime;
            }

            // Close the file.
            _fileCurrentMonth.close();
        }
    }

    // Return how much employee has wokred.
    return _weekHours;
}

int32_t Logging::getEmployeeDailyHours(uint64_t _tagID, uint32_t _epoch, int32_t *_firstLogin, int32_t *_lastLogout,
                                       int *_missedLogutFlag)
{
    // First init uSD card. If uSD card can't be initialized, return -1 (error).
    if (!sd.begin(15, SD_SCK_MHZ(10)))
    {
        return -1;
    }

    // Change the current working directory to be root of the uSD card.
    sd.chdir(true);

    // Now try to get every login and logout data of this specific employee.
    int32_t _startDayEpoch;
    int32_t _endDayEpoch;
    uint32_t _dailyHours = 0;
    int32_t _firstTimeLogin = -1;
    int32_t _lastTimeLogout = -1;

    // Set missed logouts to false.
    if (_missedLogutFlag != NULL)
        *_missedLogutFlag = 0;

    // For file handling.
    SdFile _myFile;

    // Get all employee data (First name, last name, tagID, etc...)
    struct employeeData *_employee = _link->getEmployeeByID(_tagID);
    if (_employee == NULL)
        return -1;

    // Create human readable time and date from epoch.
    struct tm _myTime;
    memcpy(&_myTime, localtime((const time_t *)&_epoch), sizeof(_myTime));

    // Open a directory and file of this specific employee for month and year defined in the function argument.
    char _path[250];
    sprintf(_path, "/%s/%s%s%llu/%d/%s_%04d_%02d_int.csv", DEFAULT_FOLDER_NAME, &(_employee->firstName),
            &(_employee->lastName), (unsigned long long)(_employee->ID), _myTime.tm_year + 1900, _employee->firstName,
            _myTime.tm_year + 1900, _myTime.tm_mon + 1);

    // Try to open this file (if it even exists). Can't do that? Throw an error (return -1)!
    if (!_myFile.open(_path))
    {
        return -1;
    }

    // Now, tricky stuff. But this function can save us! It calculates epoch time of start and end of the selected day.
    // Easy!
    calculateDayEpoch(_epoch, &_startDayEpoch, &_endDayEpoch);

    // Now get one line from the file with login and logut epoch times and parse it using sscanf function.
    // Function for logging (addLog) and this function MUST have same structure!!!
    char _oneLine[50];
    while (_myFile.available())
    {
        unsigned long _login = 0;
        unsigned long _logout = 0;
        readOneLineFromFile(&_myFile, _oneLine, 49);

        // If function finds both times (login and logout), it can calculate work hours between logs.
        int _dataFound = 0;
        _dataFound = sscanf(_oneLine, "%lu,%lu", &_login, &_logout);
        if (_dataFound > 0)
        {
            // Just use times that matches start and end of the current day, ingore anything else.
            if ((_login >= _startDayEpoch) && (_login <= _endDayEpoch) && (_logout <= _endDayEpoch) &&
                (_logout >= _startDayEpoch) && (_dataFound == 2))
            {
                _dailyHours += (_logout - _login);
                _lastTimeLogout = _logout;
                if (_firstTimeLogin == -1)
                    _firstTimeLogin = _login;
            }
            else if ((_login >= _startDayEpoch) && (_login <= _endDayEpoch) && (_dataFound == 1) && (_logout == 0))
            {
                // If someone forgot to logout, there will be only one enrty and that's for login, logut will be missing
                if (_missedLogutFlag != NULL)
                    *_missedLogutFlag = 1;
                if (_firstTimeLogin == -1)
                    _firstTimeLogin = _login;
            }
        }
    }
    if (_firstLogin != NULL)
        (*_firstLogin) = _firstTimeLogin;
    if (_lastLogout != NULL)
        (*_lastLogout) = _lastTimeLogout;
    _myFile.close();
    return _dailyHours;
}

void Logging::calculateNextDailyReport()
{
    // Calculate the next epoch for making a report for the current day (calculating first login time, last login time
    // and work time)

    // First we need current date and time. We can get it from the RTC.
    _ink->rtcGetRtcData();
    int32_t _epoch = _ink->rtcGetEpoch();

    // Add minute on that reading to aviod false calulating (for example, daily report is being made at
    // 23:59:50 1.1.2022. and it's done at 23:59:52 1.1.2022.,so "next" calculation will be at 23:59:50 1.1.2022. that
    // is acctually wrong)
    _epoch += 60;

    // Now get the end of the day epoch
    int32_t _startDayEpoch;
    int32_t _endDayEpoch;
    calculateDayEpoch(_epoch, &_startDayEpoch, &_endDayEpoch);

    // Stat calculating 10 seconds before midnight
    _dailyReportEpoch = _endDayEpoch - 9;
}

bool Logging::isDailyReport()
{
    // Check if the daily report for employees needs to be created
    _ink->rtcGetRtcData();
    int32_t _epoch = _ink->rtcGetEpoch();

    if (_epoch >= _dailyReportEpoch)
    {
        return true;
    }
    else
    {
        return false;
    }
}

int Logging::createDailyReport()
{
    // For file managment
    SdFile _myFile;

    // For filename and path
    char _pathStr[250];

    // Struct for time and date
    struct tm _myTime;

    // For epoch from the RTC
    uint32_t _epoch;

    // Try to get how much employees we have in the list
    int _n = _link->numberOfElements();

    // Empty list? Return error.
    if (_n == 0)
        return 0;

    // Try to init micro SD card. If failed, return 0 (error).
    if (!sd.begin(15, SD_SCK_MHZ(10)))
        return 0;

    // Go to the root of the micro SD card. If failed, return error.
    if (!sd.chdir(true))
        return 0;

    // Get time and date
    _ink->rtcGetRtcData();
    _epoch = _ink->rtcGetEpoch();
    memcpy(&_myTime, localtime((const time_t *)&_epoch), sizeof(_myTime));

    for (int i = 0; i < _n; i++)
    {
        // Get the employee data.
        struct employeeData *_e = _link->getEmployee(i);

        int32_t _firstLoginEpoch = 0;
        int32_t _lastLogoutEpoch = 0;
        int32_t _workHours = 0;
        int32_t _overtime = 0;
        int _missedLogout = 0;
        char _timestampLoginStr[30];
        char _timestampLogoutStr[30];

        // Get working hours data from the specific employee for this specific time.
        _workHours =
            getEmployeeDailyHours(_e->ID, _dailyReportEpoch, &_firstLoginEpoch, &_lastLogoutEpoch, &_missedLogout);

        // Check if the data for this employee is avaialbe. If not, ignore it.
        if (_workHours > 0 && (_firstLoginEpoch != -1 || _firstLoginEpoch != 0))
        {
            // Create timestamp strings for login and logout times
            createTimeStampFromEpoch(_timestampLoginStr, _firstLoginEpoch);
            createTimeStampFromEpoch(_timestampLogoutStr, _lastLogoutEpoch);

            // Create a path and a filename for the report
            sprintf(_pathStr, "/%s/%s%s%llu/%d/%s_%04d_%02d_report.csv", DEFAULT_FOLDER_NAME, &(_e->firstName),
                    &(_e->lastName), (unsigned long long)(_e->ID), _myTime.tm_year + 1900, _e->firstName,
                    _myTime.tm_year + 1900, _myTime.tm_mon + 1);

            // Check if the file even exists. If not, create it and add a header.
            if (!sd.exists(_pathStr))
            {
                // Try to create the file, if file create failed, something is wrong, abort, abort!
                if (_myFile.open(_pathStr, O_CREAT | O_RDWR))
                {
                    _myFile.println("DOW, Time, Date, First Login, Last Logout, Work Time, Overtime, Missed logout");
                    _myFile.close();
                }
            }

            // If file already exists, open it and make it available for reading as well as for writing.
            if (!_myFile.open(_pathStr, O_RDWR))
                return 0;

            // Go to the end of the file.
            _myFile.seekEnd();

            // Calculate overtime hours
            // overtimeHours defines how much workhours are allowed in the specific workday. Defined in dataTypes.h
            _overtime = _workHours - overtimeHours[_myTime.tm_wday] * 60 * 60;

            // No negative overtime.
            if (_overtime < 0)
                _overtime = 0;

            // Write one line of the data into the file.
            sprintf(_pathStr, "%s,%02d:%02d:%02d,%02d/%02d/%04d,%s,%s,%02d:%02d:%02d,%02d:%02d:%02d,%c",
                    wdayName[_myTime.tm_wday], _myTime.tm_hour, _myTime.tm_min, _myTime.tm_sec, _myTime.tm_mday,
                    _myTime.tm_mon + 1, _myTime.tm_year + 1900, _timestampLoginStr,
                    _lastLogoutEpoch != -1 ? _timestampLogoutStr : LOGGING_ERROR_STRING, _workHours / 3600,
                    _workHours / 60 % 60, _workHours % 60, _overtime / 3600, _overtime / 60 % 60, _overtime % 60,
                    _missedLogout == 1 ? 'Y' : ' ');
            _myFile.println(_pathStr);

            // Close the file (avoid memory leak at all cost!)
            _myFile.close();
        }
    }

    return 1;
}


int Logging::getEmployeeFile(SdFile *_myFile, struct employeeData *_employee, int _month, int _year, int _rawFlag)
{
    // Array for path to the file
    char _path[250];

    // First check if the micro SD card can be initialized
    if (!sd.begin(15, SD_SCK_MHZ(10)))
        return 0;

    // If micro SD card init is ok, go to the root of the micro SD card
    if (!sd.chdir(true))
        return 0;

    // Now make a path to the file
    // Folder structure is like this:
    // [DEFAULT_FOLDER_NAME]\[FirstName][LastName][ID]\[YEAR]\[FirstName]_[Year]_[Month]_[int or report].csv
    sprintf(_path, "%s/%s%s%llu/%04d/%s_%04d_%02d_%s.csv", DEFAULT_FOLDER_NAME, _employee->firstName,
            _employee->lastName, (unsigned long long)(_employee->ID), _year, _employee->firstName, _year, _month,
            _rawFlag ? "int" : "report");

    // Now try to open a file
    return (_myFile->open(_path, O_RDONLY));
}

int Logging::findLastLog(struct employeeData *_e, int32_t *_login, int32_t *_logout)
{
    // Array for one line in file
    char _oneLine[550];

    // Needed for file handling
    SdFile _myFile;

    // Temp variables for last login / logout times
    unsigned long _log1 = 0;
    unsigned long _log2 = 0;

    // First check if the micro SD card can be initialized
    if (!sd.begin(15, SD_SCK_MHZ(10)))
        return 0;

    // If micro SD card init is ok, go to the root of the micro SD card
    if (!sd.chdir(true))
        return 0;

    // Find the file
    if (getEmployeeFile(&_myFile, _e, _ink->rtcGetMonth(), _ink->rtcGetYear(), 1))
    {
        // Go to the last row in the file
        char _oneLine[50];
        while (_myFile.available())
        {
            readOneLineFromFile(&_myFile, _oneLine, 49);
        }

        // Close the file
        _myFile.close();

        // Parse the data
        sscanf(_oneLine, "%llu,%llu", &_log1, &_log2);

        // Save the data
        if (_login != NULL)
            *_login = _log1;
        if (_logout != NULL)
            *_logout = _log2;

        // Retrun 1 for success
        return 1;
    }

    return 0;
}

int Logging::getWorkHours(SdFile *_f, int32_t _startEpoch, int32_t _endEpoch, int32_t *_loggedTime)
{
    char _oneLine[250];
    if (_f == NULL || _loggedTime == NULL)
        return 0;

    if (!_f->available())
        return 0;

    *_loggedTime = 0;

    while (_f->available())
    {
        char c = 0;
        int k = 0;
        unsigned long _login = 0;
        unsigned long _logout = 0;
        while (c != '\n' && _f->available() && c != 255)
        {
            c = _f->read();
            _oneLine[k++] = c;
        }
        _oneLine[k] = 0;

        // If function finds both times (login and logout), it can calculate work hours between logs.
        if (sscanf(_oneLine, "%lu,%lu", &_login, &_logout) == 2)
        {
            // Just use times that matches start and end of the current week, ingore anything else.
            if ((_login >= _startEpoch) && (_login <= _endEpoch) && (_logout <= _endEpoch) && (_logout >= _startEpoch))
            {
                *_loggedTime += (_logout - _login);
            }
        }
    }

    return 1;
}