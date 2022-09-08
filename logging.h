#ifndef __LOGGING_H__
#define __LOGGING_H__

#include "Inkplate.h"
#include "dataTypes.h"
#include "linkedList.h"
#include "helpers.h"

class Logging
{
    public:
    Logging();
    int begin(SdFat *_s, LinkedList *_l, Inkplate *_i);
    int getEnteries(SdFile *_file);
    int fillEmployees(SdFile *_file);
    int checkFolders();
    int updateEmployeeFile();
    uint64_t getTagID();
    int addLog(uint64_t _tagID, uint32_t _epoch, struct employeeData &_w);
    int findLastEntry(SdFile *_f, uint32_t *_epoch, uint8_t *_log);
    int32_t getEmployeeWeekHours(uint64_t _tagID, uint32_t _epoch);
    int32_t getEmployeeDailyHours(uint64_t _tagID, uint32_t _epoch, int32_t *_firstLogin = NULL, int32_t *_lastLogout = NULL);
    void calculateNextDailyReport();
    bool isDailyReport();
    int createDailyReport();

    private:
    struct employeeData *employees;
    SdFat *_sd;
    LinkedList *_link;
    Inkplate *_ink;

    int32_t _dailyReportEpoch = 0;
};

#endif