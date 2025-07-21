# Makefile for volcom_employee module

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread -fPIC -D_GNU_SOURCE -I..
LDFLAGS = -pthread

# Source files
SOURCES = volcom_employee.c
HEADERS = volcom_employee.h
OBJECTS = $(SOURCES:.c=.o)

# Shared volcom_agents sources
SHARED_SOURCES = ../task_buffer.c ../result_queue.c
SHARED_OBJECTS = $(SHARED_SOURCES:.c=.o)

# All objects
ALL_OBJECTS = $(OBJECTS) $(SHARED_OBJECTS)

# Target executables and libraries
LIBRARY = libvolcom_employee.a
EXECUTABLE = volcom_employee
TEST_EXECUTABLE = test_employee

# Dependencies - external libraries
SYSINFO_LIB = ../../volcom_sysinfo/libvolcom_sysinfo.a
NET_LIB = ../../volcom_net/libvolcom_net.a
RCSMNGR_LIB = ../../volcom_rcsmngr/libvolcom_rcsmngr.a
UTILS_LIB = ../../volcom_utils/libvolcom_utils.a

# All dependencies
ALL_LIBS = $(SYSINFO_LIB) $(NET_LIB) $(RCSMNGR_LIB) $(UTILS_LIB)

# Library directories for linking
LIB_DIRS = -L../../volcom_sysinfo -L../../volcom_net -L../../volcom_rcsmngr -L../../volcom_utils

# Library flags
LIBS = -lvolcom_sysinfo -lvolcom_net -lvolcom_rcsmngr -lvolcom_utils -lcjson

# Default target
all: $(LIBRARY) $(EXECUTABLE)

# Build static library
$(LIBRARY): $(ALL_OBJECTS)
	ar rcs $(LIBRARY) $(ALL_OBJECTS)
	@echo "Created static library: $(LIBRARY)"

# Build main executable
$(EXECUTABLE): $(ALL_OBJECTS) $(ALL_LIBS)
	$(CC) $(CFLAGS) -o $(EXECUTABLE) $(ALL_OBJECTS) $(LIB_DIRS) $(LIBS) $(LDFLAGS)
	@echo "Created executable: $(EXECUTABLE)"

# Build test executable (if test_employee.c exists)
$(TEST_EXECUTABLE): test_employee.c $(LIBRARY) $(ALL_LIBS)
	$(CC) $(CFLAGS) -o $(TEST_EXECUTABLE) test_employee.c -L. -lvolcom_employee $(LIB_DIRS) $(LIBS) $(LDFLAGS)
	@echo "Created test executable: $(TEST_EXECUTABLE)"

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile shared source files
../%.o: ../%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build dependencies if they don't exist
$(SYSINFO_LIB):
	$(MAKE) -C ../../volcom_sysinfo

$(NET_LIB):
	$(MAKE) -C ../../volcom_net

$(RCSMNGR_LIB):
	$(MAKE) -C ../../volcom_rcsmngr

$(UTILS_LIB):
	$(MAKE) -C ../../volcom_utils

# Clean build files
clean:
	rm -f $(OBJECTS) $(SHARED_OBJECTS) $(LIBRARY) $(EXECUTABLE) $(TEST_EXECUTABLE)
	@echo "Cleaned employee module build files"

# Clean all including dependencies
clean-all: clean
	$(MAKE) -C ../../volcom_sysinfo clean
	$(MAKE) -C ../../volcom_net clean
	$(MAKE) -C ../../volcom_rcsmngr clean
	$(MAKE) -C ../../volcom_utils clean

# Install library and headers (optional)
install: $(LIBRARY) $(HEADERS)
	mkdir -p /usr/local/include/volcom
	mkdir -p /usr/local/lib
	cp $(HEADERS) /usr/local/include/volcom/
	cp $(LIBRARY) /usr/local/lib/
	@echo "Installed employee module"

# Run the employee program
run: $(EXECUTABLE)
	./$(EXECUTABLE)

# Run tests (if test executable exists)
test: $(TEST_EXECUTABLE)
	./$(TEST_EXECUTABLE)

# Show help
help:
	@echo "Available targets:"
	@echo "  all           - Build library and executable"
	@echo "  $(LIBRARY)    - Build static library"
	@echo "  $(EXECUTABLE) - Build main executable"
	@echo "  $(TEST_EXECUTABLE) - Build test executable"
	@echo "  clean         - Remove build files"
	@echo "  clean-all     - Remove build files and clean dependencies"
	@echo "  install       - Install library and headers system-wide"
	@echo "  run           - Build and run the employee program"
	@echo "  test          - Build and run tests"
	@echo "  help          - Show this help"

# Debug target - show variables
debug:
	@echo "CC: $(CC)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo "SOURCES: $(SOURCES)"
	@echo "OBJECTS: $(OBJECTS)"
	@echo "SHARED_OBJECTS: $(SHARED_OBJECTS)"
	@echo "ALL_LIBS: $(ALL_LIBS)"
	@echo "LIB_DIRS: $(LIB_DIRS)"
	@echo "LIBS: $(LIBS)"

.PHONY: all clean clean-all install run test help debug
