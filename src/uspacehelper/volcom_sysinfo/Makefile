# Makefile for volcom_sysinfo module

CC = gcc
CFLAGS = -Wall -Wextra -std=c99
SOURCES = volcom_sysinfo.c
HEADERS = volcom_sysinfo.h
OBJECT = volcom_sysinfo.o
LIBRARY = libvolcom_sysinfo.a

# Default target
all: $(LIBRARY) main_example

# Create object file
$(OBJECT): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -c $(SOURCES) -o $(OBJECT)

# Create static library
$(LIBRARY): $(OBJECT)
	ar rcs $(LIBRARY) $(OBJECT)

# Compile main example
main_example: main_example.c $(LIBRARY)
	$(CC) $(CFLAGS) main_example.c -L. -lvolcom_sysinfo -o main_example

# Clean build files
clean:
	rm -f $(OBJECT) $(LIBRARY) main_example

# Install header and library (optional)
install: $(LIBRARY) $(HEADERS)
	mkdir -p /usr/local/include/volcom
	mkdir -p /usr/local/lib
	cp $(HEADERS) /usr/local/include/volcom/
	cp $(LIBRARY) /usr/local/lib/

# Show help
help:
	@echo "Available targets:"
	@echo "  all           - Build library and main example"
	@echo "  $(LIBRARY)    - Build static library"
	@echo "  main_example  - Build example using static library"
	@echo "  clean         - Remove build files"
	@echo "  install       - Install library and headers system-wide"
	@echo "  help          - Show this help"

.PHONY: all clean install help
