#ifndef TASK_RECEIVER_H
#define TASK_RECEIVER_H

// Receives a file and stores it; returns malloc'ed path to saved file (must be freed)
char* handle_task_receive(int client_fd, const char *employer_ip);


#endif
