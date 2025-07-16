# Makefile for volcom_rcsmngr module

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -fPIC
SOURCES = volcom_rcsmngr.c
HEADERS = volcom_rcsmngr.h
OBJECT = volcom_rcsmngr.o
LIBRARY = libvolcom_rcsmngr.a

# Dependencies
SYSINFO_LIB = ../volcom_sysinfo/libvolcom_sysinfo.a

# Default target
all: $(LIBRARY) test_rcsmngr test_basic test_cgroups

# Create object file
$(OBJECT): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -c $(SOURCES) -o $(OBJECT)

# Create static library
$(LIBRARY): $(OBJECT)
	ar rcs $(LIBRARY) $(OBJECT)

# Build test program
test_rcsmngr: test_rcsmngr.c $(LIBRARY) $(SYSINFO_LIB)
	$(CC) $(CFLAGS) test_rcsmngr.c -L. -L../volcom_sysinfo -lvolcom_rcsmngr -lvolcom_sysinfo -o test_rcsmngr

# Build basic test program
test_basic: test_basic.c $(LIBRARY) $(SYSINFO_LIB)
	$(CC) $(CFLAGS) test_basic.c -L. -L../volcom_sysinfo -lvolcom_rcsmngr -lvolcom_sysinfo -o test_basic

# Build cgroup creation test program
test_cgroups: test_cgroups.c $(LIBRARY) $(SYSINFO_LIB)
	$(CC) $(CFLAGS) test_cgroups.c -L. -L../volcom_sysinfo -lvolcom_rcsmngr -lvolcom_sysinfo -o test_cgroups

# Build the sysinfo library if it doesn't exist
$(SYSINFO_LIB):
	$(MAKE) -C ../volcom_sysinfo libvolcom_sysinfo.a

# Clean build files
clean:
	rm -f $(OBJECT) $(LIBRARY) test_rcsmngr test_basic test_cgroups

# Install header and library (optional)
install: $(LIBRARY) $(HEADERS)
	mkdir -p /usr/local/include/volcom
	mkdir -p /usr/local/lib
	cp $(HEADERS) /usr/local/include/volcom/
	cp $(LIBRARY) /usr/local/lib/

# Show help
help:
	@echo "Available targets:"
	@echo "  all           - Build library and test program"
	@echo "  $(LIBRARY)    - Build static library"
	@echo "  test_rcsmngr  - Build test program"
	@echo "  clean         - Remove build files"
	@echo "  install       - Install library and headers system-wide"
	@echo "  help          - Show this help"

.PHONY: all clean install help
