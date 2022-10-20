#ifndef __LINKEDLIST_H__
#define __LINKEDLIST_H__

#include "Arduino.h"
#include "dataTypes.h"

class LinkedList
{
  public:
    LinkedList();
    int addEmployee(char *_firstName, char *_lastName, uint64_t _id, char *_imageFilename, char *_department);
    int removeEmployee(uint64_t _id);
    struct employeeData *getEmployee(int i);
    struct employeeData *getEmployeeByID(uint64_t _id);
    void removeAllEmployees();
    int checkID(uint64_t _id);
    int numberOfElements();
    void printAllData();

  private:
    struct employeeData *head = NULL;
    struct employeeData *current = NULL;
};

#endif