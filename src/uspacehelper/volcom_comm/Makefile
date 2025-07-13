# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -pthread -I./include -I../volcom_sysinfo -I/usr/include/cjson

# Common source files shared by both modes
COMMON_SRC = utils/net_utils.c \
             ../volcom_sysinfo/volcom_sysinfo.c

# All sources needed for unified binary (employee + employer)
SRC = comm_main.c \
      employee/employee_mode.c \
      employer/employer_mode.c \
      employer/employee_list.c \
      employer/select_employee.c \
      employer/send_task.c\
      employee/task_receiver.c \
      employee/send_result.c \
      $(COMMON_SRC)


# Object files
OBJ = $(SRC:.c=.o)

# Final binary
TARGET = volcom_comm

# Default target
all: $(TARGET)

# Build final executable
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -lcjson -o $(TARGET)

# Clean build artifacts
clean:
	rm -f $(OBJ) $(TARGET)
