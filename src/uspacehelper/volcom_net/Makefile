# Makefile for volcom_net module with Unix socket support

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread
LDFLAGS = -pthread

# Source files
SOURCES = volcom_net.c
OBJECTS = $(SOURCES:.c=.o)

# Test files
TEST_SOURCES = test_tcp_udp.c
TEST_OBJECTS = $(TEST_SOURCES:.c=.o)

DEMO_SOURCES = simple_tcp_demo.c
DEMO_OBJECTS = $(DEMO_SOURCES:.c=.o)

UNIX_TEST_SOURCES = test_unix_socket.c
UNIX_TEST_OBJECTS = $(UNIX_TEST_SOURCES:.c=.o)

# Targets
LIBRARY = libvolcom_net.a
TEST_EXECUTABLE = test_tcp_udp
DEMO_EXECUTABLE = simple_tcp_demo
UNIX_TEST_EXECUTABLE = test_unix_socket

# Default target
all: $(LIBRARY) $(TEST_EXECUTABLE) $(DEMO_EXECUTABLE) $(UNIX_TEST_EXECUTABLE)

# Build static library
$(LIBRARY): $(OBJECTS)
	ar rcs $(LIBRARY) $(OBJECTS)
	@echo "Created static library: $(LIBRARY)"

# Build test executable
$(TEST_EXECUTABLE): $(TEST_OBJECTS) $(LIBRARY)
	$(CC) $(CFLAGS) -o $(TEST_EXECUTABLE) $(TEST_OBJECTS) $(LIBRARY) $(LDFLAGS)
	@echo "Created test executable: $(TEST_EXECUTABLE)"

# Build demo executable
$(DEMO_EXECUTABLE): $(DEMO_OBJECTS) $(LIBRARY)
	$(CC) $(CFLAGS) -o $(DEMO_EXECUTABLE) $(DEMO_OBJECTS) $(LIBRARY) $(LDFLAGS)
	@echo "Created demo executable: $(DEMO_EXECUTABLE)"

# Build Unix socket test executable
$(UNIX_TEST_EXECUTABLE): $(UNIX_TEST_OBJECTS) $(LIBRARY)
	$(CC) $(CFLAGS) -o $(UNIX_TEST_EXECUTABLE) $(UNIX_TEST_OBJECTS) $(LIBRARY) $(LDFLAGS)
	@echo "Created Unix socket test executable: $(UNIX_TEST_EXECUTABLE)"

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TEST_OBJECTS) $(DEMO_OBJECTS) $(UNIX_TEST_OBJECTS) $(LIBRARY) $(TEST_EXECUTABLE) $(DEMO_EXECUTABLE) $(UNIX_TEST_EXECUTABLE)
	@echo "Cleaned build artifacts"

# Install library (optional)
install: $(LIBRARY)
	sudo cp $(LIBRARY) /usr/local/lib/
	sudo cp volcom_net.h /usr/local/include/
	@echo "Installed library and header to system"

# Test targets
test: $(TEST_EXECUTABLE)
	./$(TEST_EXECUTABLE)

demo: $(DEMO_EXECUTABLE)
	@echo "Run './$(DEMO_EXECUTABLE) server' or './$(DEMO_EXECUTABLE) client'"

unix-test: $(UNIX_TEST_EXECUTABLE)
	./$(UNIX_TEST_EXECUTABLE)

# Help target
help:
	@echo "Available targets:"
	@echo "  all          - Build library and all executables"
	@echo "  $(LIBRARY)   - Build static library only"
	@echo "  test         - Build and run TCP/UDP test executable"
	@echo "  demo         - Build demo executable"
	@echo "  unix-test    - Build and run Unix socket test executable"
	@echo "  clean        - Remove build artifacts"
	@echo "  install      - Install library to system (requires sudo)"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Demo usage:"
	@echo "  ./$(DEMO_EXECUTABLE) server    - Run TCP server"
	@echo "  ./$(DEMO_EXECUTABLE) client    - Run TCP client"
	@echo "  ./$(UNIX_TEST_EXECUTABLE)      - Run Unix socket test"

.PHONY: all clean install test demo unix-test help
