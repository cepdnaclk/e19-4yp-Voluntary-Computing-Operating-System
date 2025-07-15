# Makefile for volcom_sysinfo module

CC = gcc
CFLAGS = -Wall -Wextra -std=c99
SOURCES = volcom_sysinfo.c
HEADERS = volcom_sysinfo.h
OBJECT = volcom_sysinfo.o
LIBRARY = libvolcom_sysinfo.a

# Default target
all: $(LIBRARY) test_sysinfo

# Create object file
$(OBJECT): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -c $(SOURCES) -o $(OBJECT)

# Create static library
$(LIBRARY): $(OBJECT)
	ar rcs $(LIBRARY) $(OBJECT)

# Compile test_sysinfo
test_sysinfo: test_sysinfo.c $(LIBRARY)
	$(CC) $(CFLAGS) test_sysinfo.c -L. -lvolcom_sysinfo -o test_sysinfo
	./test_sysinfo

# Clean build files
clean:
	rm -f $(OBJECT) $(LIBRARY) test_sysinfo

# Install header and library (optional)
install: $(LIBRARY) $(HEADERS)
	mkdir -p /usr/local/include/volcom
	mkdir -p /usr/local/lib
	cp $(HEADERS) /usr/local/include/volcom/
	cp $(LIBRARY) /usr/local/lib/

# Show help
help:
	@echo "Available targets:"
	@echo "  all           - Build library and test_sysinfo"
	@echo "  $(LIBRARY)    - Build static library"
	@echo "  test_sysinfo  - Build example using static library"
	@echo "  clean         - Remove build files"
	@echo "  install       - Install library and headers system-wide"
	@echo "  help          - Show this help"

.PHONY: all clean install help
