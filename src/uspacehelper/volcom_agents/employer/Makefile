# Makefile for volcom_employer module

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread -fPIC -D_GNU_SOURCE
LDFLAGS = -pthread

# Source files
SOURCES = volcom_employer.c
HEADERS = volcom_employer.h
OBJECTS = $(SOURCES:.c=.o)

# Shared volcom_agents sources
SHARED_SOURCES = ../task_buffer.c ../result_queue.c
SHARED_OBJECTS = $(SHARED_SOURCES:.c=.o)

# All objects
ALL_OBJECTS = $(OBJECTS) $(SHARED_OBJECTS)

# Target executables and libraries
LIBRARY = libvolcom_employer.a
EXECUTABLE = volcom_employer
TEST_EXECUTABLE = test_employer

# Dependencies - external libraries
SYSINFO_LIB = ../../volcom_sysinfo/libvolcom_sysinfo.a
NET_LIB = ../../volcom_net/libvolcom_net.a
SCHEDULER_LIB = ../../volcom_scheduler/libvolcom_scheduler.a
UTILS_LIB = ../../volcom_utils/libvolcom_utils.a

# All dependencies
ALL_LIBS = $(SYSINFO_LIB) $(NET_LIB) $(SCHEDULER_LIB) $(UTILS_LIB)

# Library directories for linking
LIB_DIRS = -L../../volcom_sysinfo -L../../volcom_net -L../../volcom_scheduler -L../../volcom_utils

# Library flags
LIBS = -lvolcom_sysinfo -lvolcom_net -lvolcom_scheduler -lvolcom_utils -lcjson

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

# Build test executable (if test_employer.c exists)
$(TEST_EXECUTABLE): test_employer.c $(LIBRARY) $(ALL_LIBS)
	$(CC) $(CFLAGS) -o $(TEST_EXECUTABLE) test_employer.c -L. -lvolcom_employer $(LIB_DIRS) $(LIBS) $(LDFLAGS)
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

$(SCHEDULER_LIB):
	$(MAKE) -C ../../volcom_scheduler

$(UTILS_LIB):
	$(MAKE) -C ../../volcom_utils

# Clean build files
clean:
	rm -f $(OBJECTS) $(SHARED_OBJECTS) $(LIBRARY) $(EXECUTABLE) $(TEST_EXECUTABLE)
	@echo "Cleaned employer module build files"

# Clean all including dependencies
clean-all: clean
	$(MAKE) -C ../../volcom_sysinfo clean
	$(MAKE) -C ../../volcom_net clean
	$(MAKE) -C ../../volcom_scheduler clean
	$(MAKE) -C ../../volcom_utils clean

# Install library and headers (optional)
install: $(LIBRARY) $(HEADERS)
	mkdir -p /usr/local/include/volcom
	mkdir -p /usr/local/lib
	cp $(HEADERS) /usr/local/include/volcom/
	cp $(LIBRARY) /usr/local/lib/
	@echo "Installed employer module"

# Run the employer program
run: $(EXECUTABLE)
	./$(EXECUTABLE)

# Run tests (if test executable exists)
test: $(TEST_EXECUTABLE)
	./$(TEST_EXECUTABLE)

# Check if required directories exist
check-dirs:
	@echo "Checking for required directories and files..."
	@test -d ../../volcom_sysinfo || echo "Warning: volcom_sysinfo directory not found"
	@test -d ../../volcom_net || echo "Warning: volcom_net directory not found"
	@test -d ../../volcom_scheduler || echo "Warning: volcom_scheduler directory not found"
	@test -d ../../volcom_utils || echo "Warning: volcom_utils directory not found"
	@test -f ../task_buffer.c || echo "Warning: task_buffer.c not found in parent directory"
	@test -f ../result_queue.c || echo "Warning: result_queue.c not found in parent directory"
	@echo "Directory check complete"

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
	@echo "  run           - Build and run the employer program"
	@echo "  test          - Build and run tests"
	@echo "  check-dirs    - Check if required directories exist"
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

# Create required directories (for results path mentioned in source)
setup:
	@echo "Creating required directories..."
	mkdir -p /home/dasun/Projects/e19-4yp-Voluntary-Computing-Operating-System/src/uspacehelper/results
	@echo "Setup complete"

.PHONY: all clean clean-all install run test help debug check-dirs setup
