#ifndef SELECT_EMPLOYEE_H
#define SELECT_EMPLOYEE_H

#include "employee_list.h"

// Prompts user to select an employee by IP
// Returns a strdup'ed string containing the selected IP
char *select_employee_ip(EmployeeNodeWrapper *list);

#endif
