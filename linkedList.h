#ifndef __LINKEDLIST_H__
#define __LINKEDLIST_H__

#include "Arduino.h"
#include "dataTypes.h"

class LinkedList
{
    public:
    LinkedList();
    int addEmployee(char *_firstName, char *_lastName, uint64_t _id, char *_imageFilename);
    int removeEmployee(uint64_t _id);
    struct worker * getEmployee(int i);
    struct worker * getEmployeeByID(uint64_t _id);
    void removeAllEmployees();
    int checkID(uint64_t _id);
    int numberOfElements();
    void printAllData();

    private:
    struct worker *head = NULL;
    struct worker *current = NULL;
};

#endif