#Makefile for volcom main module

CC = gcc
CFLAGS = -Wall -Wextra -std=c99
SOURCES = volcom_main.c
OBJECT = volcom_main.o
SYSINFO_OBJECT = libvolcom_sysinfo.a
SYSINFO_LIBRARY = volcom_sysinfo
RCSMNGR_OBJECT = libvolcom_rcsmngr.a
RCSMNGR_LIBRARY = volcom_rcsmngr

all: volcom_main

volcom_main: $(OBJECT) $(SYSINFO_OBJECT) $(RCSMNGR_OBJECT)
	$(CC) $(CFLAGS) $(OBJECT) $(SYSINFO_LIBRARY)/$(SYSINFO_OBJECT) $(RCSMNGR_LIBRARY)/$(RCSMNGR_OBJECT) -o volcom_main

$(OBJECT): $(SOURCES)
	$(CC) $(CFLAGS) -c $(SOURCES)

$(SYSINFO_OBJECT):
	$(MAKE) -C volcom_sysinfo $(SYSINFO_OBJECT)

$(RCSMNGR_OBJECT):
	$(MAKE) -C volcom_rcsmngr $(RCSMNGR_OBJECT)

clean:
	rm -f $(OBJECT) volcom_main
	$(MAKE) -C volcom_sysinfo clean
	$(MAKE) -C volcom_rcsmngr clean
