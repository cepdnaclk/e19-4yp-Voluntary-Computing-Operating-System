#ifndef VOLCOM_EMPLOYEE_H
#define VOLCOM_EMPLOYEE_H

#define _GNU_SOURCE

#define BROADCAST_INTERVAL 5              // seconds
#define RESOURCE_THRESHOLD_PERCENT 80.0   // Max usage before stopping broadcast

void run_employee_mode();

#endif // VOLCOM_EMPLOYEE_H