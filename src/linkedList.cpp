#include "linkedList.h"
#include "defines.h"

LinkedList::LinkedList()
{
}

int LinkedList::addEmployee(char *_firstName, char *_lastName, uint64_t _id, char *_imageFilename, char *_department)
{
    // If the head if at null pointer, that means there is no entery yet, we have to make first one.
    if (head == NULL)
    {
        // Allocate memory for new entry.
        head = (struct employeeData *)ps_malloc(sizeof(struct employeeData));

        // Pointer is still NULL? Allocation is failed (no memory, PS RAM disabled?, cosmic ray hit ESP32??)
        if (head == NULL)
            return 0;

        // If allocation is successfull, copy data to the struct.
        strlcpy(head->firstName, _firstName, sizeof(head->firstName));
        strlcpy(head->lastName, _lastName, sizeof(head->lastName));
        strlcpy(head->image, _imageFilename, sizeof(head->image));
        strlcpy(head->department, _department, sizeof(head->department));
        head->ID = _id;
        head->next = NULL;

        // Set pointer of current data to the new element.
        current = head;
    }
    else
    {
        // There is already some entery in the linked list? Add new pointer to the current data, "next" element.
        // Allocate memory for new entry.
        current->next = (struct employeeData *)ps_malloc(sizeof(struct employeeData));

        // Pointer is still NULL? Allocation is failed (no memory, PS RAM disabled?, cosmic ray hit ESP32??)
        if (head == NULL)
            return 0;

        // Move the pointer to the current element
        current = current->next;

        // If allocation is successfull, copy data to the struct.
        strlcpy(current->firstName, _firstName, sizeof(current->firstName));
        strlcpy(current->lastName, _lastName, sizeof(current->lastName));
        strlcpy(current->image, _imageFilename, sizeof(current->image));
        strlcpy(current->department, _department, sizeof(current->department));
        current->ID = _id;
        current->next = NULL;
    }

    return 1;
}

int LinkedList::removeEmployee(uint64_t _id)
{
    // Variavble return flag. Set it to 0 (fail).
    int _ret = 0;

    // Check for empty list
    if (head == NULL || current == NULL)
        return 0;

    // Set up pointers. Because we have single linked list, it's needed to keep track of address of prev. element.
    struct employeeData *_prev = NULL;
    struct employeeData *_curr = head;
    struct employeeData *_next = head->next;

    // Find element with needed ID tag.
    while (_curr != NULL && _curr->ID != _id)
    {
        _prev = _curr;
        _curr = _curr->next;
        if (_curr != NULL)
            _next = _curr->next;
    }

    // Check if we didn't go to the end of the list. If we did, employee if not found.
    if (_curr == NULL)
        return 0;

    // We found employee with that specific ID? Cool! Make change on linked list!
    if (_curr->ID == _id)
    {
        // Set return flag on 1 (success).
        _ret = 1;
        // Check if we need to delete first element (first element is also stored in the head pointer)
        if (_curr == head)
        {
            head = _curr->next;
            _ret = 1;
        }
        // Check if we need to remove last element (last element has NULL pointer for next element)
        else if (_curr->next == NULL)
        {
            _prev->next = NULL;
            current = _prev;
            _ret = 1;
        }
        // In the middle, just connect prev and next element.
        else
        {
            // Connect prev. and next element
            _prev->next = _next;
            _ret = 1;
        }

        // If element is found, free up memory (do not forget that, otherwise it will create memory leak!)
        free(_curr);
    }

    // Return 1 for successfully deleted element, 0 for fail (element not found in linked list).
    return _ret;
}

struct employeeData *LinkedList::getEmployee(int i)
{
    // First check for empty linked list
    if (head == NULL)
        return NULL;

    // Set pointer to the first element of the linked ist (head).
    struct employeeData *_current = head;

    // Variable for counting.
    int k = 0;

    // Try to find element with that index.
    while ((k < i) && _current != NULL)
    {
        _current = _current->next;
        k++;
    }

    // If elemnt with index number "i" is found, return address of that element.
    if (k == i)
        return _current;

    // Otherwise return NULL pointer.
    return NULL;
}

struct employeeData *LinkedList::getEmployeeByID(uint64_t _id)
{
    // First check for empty linked list
    if (head == NULL)
        return NULL;

    // Set pointer to the first element of the linked ist (head).
    struct employeeData *_current = head;

    // Try to find element with that index. Go until you find end of the list (current elements points to the NULL).
    while (_current != NULL)
    {
        if (_current->ID == _id)
        {
            return _current;
        }
        else
        {
            _current = _current->next;
        }
    }

    // If no element is found with that specific ID, return NULL.
    return NULL;
}

void LinkedList::removeAllEmployees()
{
    // Temp pointer for next element in the linked list.
    struct employeeData *_next;

    // Set current pointer to the head of the linked list.
    current = head;

    // Delete one by one and free memory as doing so.
    do
    {
        _next = current->next;
        free(current);
        current = _next;
    } while (current->next != NULL);

    // Now set both head and current to NULL, indicating empty linked list.
    current = NULL;
    head = NULL;
}

int LinkedList::checkID(uint64_t _id)
{
    // Variable for return flag.
    int _ret = 0;

    // Variable for counting (going though linked list)
    int i = 0;

    // Try to get first element from linked list.
    struct employeeData *_list = getEmployee(i++);

    // Go trough the list until until there are no more elements in the list.
    while (_list != NULL)
    {
        if (_list->ID == _id)
            _ret = 1;

        // Go to the next element.
        _list = getEmployee(i++);
    }

    // Return status of the return flag (1 if ID is found, 0 if ID is not found).
    return _ret;
}

int LinkedList::numberOfElements()
{
    // Variable for counting elements in the linkend list. Initialize it on zero.
    int _n = 0;

    struct employeeData *_list = getEmployee(_n);

    // Run a while loop until you got to the end of the list (end of the list is indicated by returning NULL pointer).
    while (_list != NULL)
    {
        // Get next element in the linked list (and also increment counter).
        _list = getEmployee(++_n);
    }

    // Return number of counted elements.
    return _n;
}

void LinkedList::printAllData()
{
    struct employeeData *_myList = head;

    while (_myList != NULL)
    {
        Serial.println(_myList->firstName);
        Serial.println(_myList->lastName);
        Serial.println(_myList->image);
        Serial.println(_myList->department);
        Serial.println(_myList->ID);
        Serial.println("-----------------");
        _myList = _myList->next;
    }
}