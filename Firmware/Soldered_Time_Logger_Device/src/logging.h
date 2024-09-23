#ifndef __LOGGING_H__
#define __LOGGING_H__

// Include main Arduino Header file.
#include <Arduino.h>

class SdFat;
class LinkedList;
class Inkplate;
class SdFile;

class Logging
{
  public:
    Logging();
    int begin(SdFat *_s, LinkedList *_l, Inkplate *_i);
    int getEnteries(SdFile *_file);
    int fillEmployees(SdFile *_file);
    int checkFolders();
    int updateEmployeeFile();
    bool getTagID(HardwareSerial *_serial, uint64_t *_tagIdData);
    int addLog(uint64_t _tagID, uint32_t _epoch, struct employeeData &_w);
    int findLastEntry(SdFile *_f, int32_t *_epoch, uint8_t *_log);
    int32_t getEmployeeWeekHours(uint64_t _tagID, uint32_t _epoch);
    int32_t getEmployeeDailyHours(uint64_t _tagID, uint32_t _epoch, int32_t *_firstLogin = NULL,
                                  int32_t *_lastLogout = NULL, int *_missedLogutFlag = NULL);
    void calculateNextDailyReport();
    bool isDailyReport();
    int createDailyReport();
    int getEmployeeFile(SdFile *_myFile, struct employeeData *_employee, int _month, int _year, int _rawFlag);
    int findLastLog(struct employeeData *_e, int32_t *_login, int32_t *_logout);
    int getWorkHours(SdFile *_f, int32_t _startEpoch, int32_t _endEpoch, int32_t *_loggedTime);

  private:
    struct employeeData *employees;
    SdFat *_sd;
    LinkedList *_link;
    Inkplate *_ink;

    int32_t _dailyReportEpoch = 0;
};

#endif