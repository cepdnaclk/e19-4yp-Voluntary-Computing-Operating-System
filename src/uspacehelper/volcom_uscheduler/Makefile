# Makefile for volcom_uscheduler module

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread -fPIC
SOURCES = volcom_uscheduler.c
HEADERS = volcom_uscheduler.h
OBJECT = volcom_uscheduler.o
LIBRARY = libvolcom_uscheduler.a

# Default target
all: $(LIBRARY) tests

# Create object file
$(OBJECT): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -c $(SOURCES) -o $(OBJECT)

# Create static library
$(LIBRARY): $(OBJECT)
	ar rcs $(LIBRARY) $(OBJECT)

# Build test program
test_uscheduler: test_uscheduler.c $(LIBRARY)
	$(CC) $(CFLAGS) test_uscheduler.c -L. -lvolcom_uscheduler -o test_uscheduler

# Build performance test
test_performance: test_performance.c $(LIBRARY)
	$(CC) $(CFLAGS) test_performance.c -L. -lvolcom_uscheduler -o test_performance

# Build multithreaded test
test_multithread: test_multithread.c $(LIBRARY)
	$(CC) $(CFLAGS) test_multithread.c -L. -lvolcom_uscheduler -o test_multithread

# Build all tests
tests: test_uscheduler test_performance test_multithread

# Clean build files
clean:
	rm -f $(OBJECT) $(LIBRARY) test_uscheduler test_performance test_multithread

# Install header and library (optional)
install: $(LIBRARY) $(HEADERS)
	mkdir -p /usr/local/include/volcom
	mkdir -p /usr/local/lib
	cp $(HEADERS) /usr/local/include/volcom/
	cp $(LIBRARY) /usr/local/lib/

# Show help
help:
	@echo "Available targets:"
	@echo "  all           - Build library and basic test"
	@echo "  $(LIBRARY)    - Build static library"
	@echo "  test_uscheduler - Build basic test program"
	@echo "  test_performance - Build performance test"
	@echo "  test_multithread - Build multithreaded test"
	@echo "  tests         - Build all test programs"
	@echo "  clean         - Remove build files"
	@echo "  install       - Install library and headers system-wide"
	@echo "  help          - Show this help message"

.PHONY: all tests clean install help
