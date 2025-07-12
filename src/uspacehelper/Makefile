#Makefile for volcom main module

CC = gcc
CFLAGS = -Wall -Wextra -std=c99
SOURCES = volcom_main.c
OBJECT = volcom_main.o
LIBRARY = libvolcom_sysinfo.a

all: volcom_main

volcom_main: $(OBJECT) volcom_sysinfo/$(LIBRARY)
	$(CC) $(CFLAGS) $(OBJECT) volcom_sysinfo/$(LIBRARY) -o volcom_main

$(OBJECT): $(SOURCES)
	$(CC) $(CFLAGS) -c $(SOURCES)

volcom_sysinfo/$(LIBRARY):
	$(MAKE) -C volcom_sysinfo $(LIBRARY)

clean:
	rm -f $(OBJECT) volcom_main
	$(MAKE) -C volcom_sysinfo clean
