CC = gcc
CFLAGS = -Wall -Wextra -pthread -I./include -I../volcom_sysinfo
SRC = comm_main.c \
      employee/employee_mode.c \
      utils/net_utils.c \
      ../volcom_sysinfo/volcom_sysinfo.c

OBJ = $(SRC:.c=.o)
TARGET = volcom_comm

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)
