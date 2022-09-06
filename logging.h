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
    int addLog(uint64_t _tagID, uint32_t _epoch);
    int findLastEntry(SdFile *_f, uint32_t *_epoch, uint8_t *_log);
    uint32_t getEmployeeWeekHours(uint64_t _tagID, uint32_t _epoch);

    private:
    struct worker *employees;
    SdFat *_sd;
    LinkedList *_link;
    Inkplate *_ink;
};

#endif