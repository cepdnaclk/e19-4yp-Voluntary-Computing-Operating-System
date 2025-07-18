# Voluntary Computing Operating System - Refactored Architecture

## Overview

This is the refactored version of the Voluntary Computing Operating System with improved modularity, separation of concerns, and maintainability.

## Architecture

The system is now organized into the following modules:

### 1. **volcom_main_new.c** - Main Entry Point
- Single entry point for the application
- Handles mode selection (Employer/Employee)
- Manages application lifecycle
- Provides user interface and signal handling

### 2. **volcom_agents/** - Agent Behaviors
- **Header**: `volcom_agents.h` (single header for all agent functionality)
- **employer_mode.c**: Implements employer behavior (task distribution, employee discovery)
- **employee_mode.c**: Implements employee behavior (resource broadcasting, task processing)
- **task_buffer.c**: Thread-safe task buffer implementation for employees

### 3. **volcom_net/** - Network Communication
- **Header**: `volcom_net.h` (single header for all networking functionality)
- **protocol.c**: JSON-based communication protocol implementation
- Handles TCP/UDP communication, connection management
- Protocol utilities for task and result metadata

### 4. **volcom_scheduler/** - Task Scheduling and Chunking
- **Header**: `volcom_scheduler.h` (single header for scheduling functionality)
- **task_scheduler.c**: Task scheduling algorithms, chunk management
- Multiple scheduling policies (Round Robin, Load Balanced, Priority-based, Deadline-aware)
- Task distribution and assignment logic

### 5. **volcom_utils/** - Utility Functions
- **Header**: `volcom_utils.h` (single header for utilities)
- **net_utils.c**: Network utilities, configuration management, system info display
- Task reception and result sending utilities
- Configuration file handling

### 6. **volcom_sysinfo/** - System Information (Existing)
- System resource monitoring
- CPU and memory usage calculation

## Key Improvements

### 1. **Separation of Concerns**
- Each module has a specific responsibility
- Clear interfaces between modules
- Reduced coupling between components

### 2. **Single Header per Module**
- Each folder contains only one header file
- Simplified include structure
- Better encapsulation

### 3. **Improved Error Handling**
- Consistent error codes across modules
- Better error reporting and logging
- Graceful degradation

### 4. **Thread Safety**
- Proper synchronization primitives
- Thread-safe data structures
- Race condition prevention

### 5. **Modularity**
- Each module can be tested independently
- Easy to extend and modify
- Better code reuse

## Building the System

### Prerequisites
```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get update
sudo apt-get install -y build-essential libcjson-dev

# Or use the automated installer
make -f Makefile_new install-deps
```

### Build Commands
```bash
# Standard build
make -f Makefile_new all

# Debug build (with debug symbols and detailed logging)
make -f Makefile_new debug

# Release build (optimized)
make -f Makefile_new release

# Clean build artifacts
make -f Makefile_new clean

# Show build configuration
make -f Makefile_new info

# Get help with available targets
make -f Makefile_new help
```

### Running the System
```bash
# Build and run automatically
make -f Makefile_new run

# Or build first, then run
make -f Makefile_new all
./volcom_main_new

# Run with verbose output (if built with debug)
make -f Makefile_new debug
./volcom_main_new
```

## Usage

### Main Menu
1. **Employer Mode**: Offload tasks to volunteer nodes
2. **Employee Mode**: Volunteer resources to process tasks
3. **Show System Information**: Display current system status
4. **Exit**: Quit the application

### Employer Mode
- Discovers available employee nodes via UDP broadcast
- Distributes task chunks to employees
- Monitors task completion
- Supports multiple scheduling policies

### Employee Mode
- Broadcasts system resources and availability
- Receives and processes tasks from employers
- Manages task queue with configurable buffer
- Sends results back to employers

## Configuration

The system supports configuration through:
- Command line arguments
- Configuration files
- Runtime parameter adjustment

## Module Testing

Individual modules can be tested:
```bash
# Test agent behaviors
make -f Makefile_new test-agents

# Test networking
make -f Makefile_new test-net

# Test scheduler
make -f Makefile_new test-scheduler

# Create directory structure (if needed)
make -f Makefile_new create-dirs
```

## Development Guidelines

### Adding New Features
1. Identify the appropriate module
2. Update the corresponding header file
3. Implement in the module's source files
4. Update dependencies in Makefile
5. Add tests if applicable

### Code Style
- Use consistent naming conventions
- Follow C99 standard
- Include proper error handling
- Document public interfaces
- Use defensive programming practices

## Migration from Legacy Code

The refactoring maintains backward compatibility while improving structure:

### Legacy Location → New Location
- `volcom_comm/comm_main.c` → `volcom_main_new.c`
- `volcom_comm/employer/` → `volcom_agents/employer_mode.c`
- `volcom_comm/employee/` → `volcom_agents/employee_mode.c`
- `volcom_comm/utils/` → `volcom_utils/` and `volcom_net/`
- Task distribution logic → `volcom_scheduler/`

### Functionality Preserved
- All existing network protocols
- Employee discovery mechanism
- Task distribution and processing
- System resource monitoring
- Configuration management

## Future Enhancements

### Planned Features
1. **Dynamic Load Balancing**: Real-time resource-based task assignment
2. **Fault Tolerance**: Task retry and failover mechanisms
3. **Security**: Authentication and encryption
4. **Monitoring**: Advanced metrics and logging
5. **Web Interface**: Browser-based management console

### Extensibility Points
- New scheduling algorithms in `volcom_scheduler`
- Additional network protocols in `volcom_net`
- Enhanced monitoring in `volcom_utils`
- Custom agent behaviors in `volcom_agents`

## Troubleshooting

### Common Build Issues

1. **Missing Dependencies**
   ```bash
   # Error: cjson/cJSON.h not found
   sudo apt-get install libcjson-dev
   
   # Error: pthread functions not found  
   sudo apt-get install build-essential
   ```

2. **Compilation Errors**
   ```bash
   # Clean and rebuild
   make -f Makefile_new clean
   make -f Makefile_new all
   
   # Check configuration
   make -f Makefile_new info
   ```

3. **Permission Issues**
   ```bash
   # Make sure you have write permissions
   chmod +w .
   
   # If installing dependencies fails
   sudo make -f Makefile_new install-deps
   ```

### Runtime Issues

1. **Network Issues**: 
   - Check firewall settings for ports 9876 (UDP) and 12345 (TCP)
   - Ensure network interfaces are properly configured
   ```bash
   # Check if ports are available
   netstat -tuln | grep -E '9876|12345'
   ```

2. **Resource Issues**: 
   - Verify system has sufficient memory and CPU
   - Check that chunked_set directory exists (for employer mode)
   ```bash
   # Create chunked_set directory if needed
   mkdir -p /home/geeth99/Desktop/chuncked_set
   ```

3. **Permission Issues**: 
   - Ensure proper file and network permissions
   - Run with appropriate user privileges
   
### Debug Information

For detailed debugging:
```bash
# Build with debug symbols
make -f Makefile_new debug

# Run with debug output
./volcom_main_new

# Check system resources
./volcom_main_new
# Then select option 3 (Show System Information)
```

### Getting Help

```bash
# Show all available make targets
make -f Makefile_new help

# Show build configuration
make -f Makefile_new info

# Verify dependencies are installed
pkg-config --exists libcjson && echo "cJSON found" || echo "cJSON missing"
```

### Debug Mode
Build with debug flags for detailed logging:
```bash
make -f Makefile_new debug
./volcom_main_new
```

### Quick Start Guide
```bash
# 1. Clone and navigate to the project
cd /path/to/e19-4yp-Voluntary-Computing-Operating-System/src/uspacehelper

# 2. Install dependencies
make -f Makefile_new install-deps

# 3. Build the system
make -f Makefile_new all

# 4. Run the system
./volcom_main_new

# 5. Choose your mode:
#    - Press 1 for Employer Mode (task distribution)
#    - Press 2 for Employee Mode (resource volunteering)
#    - Press 3 for System Information
#    - Press 4 to Exit
```

### Build Output
After successful compilation, you'll see:
```
Build complete: volcom_main_new
```

The executable `volcom_main_new` will be created in the current directory.

## Build System Details

### Makefile Targets

The `Makefile_new` provides several useful targets:

| Target | Description |
|--------|-------------|
| `all` | Default build target (same as `make -f Makefile_new`) |
| `clean` | Remove all build artifacts (*.o files and executable) |
| `debug` | Build with debug symbols (-g) and debug logging |
| `release` | Build with optimizations (-O2) and release flags |
| `install-deps` | Install required system dependencies |
| `create-dirs` | Create the modular directory structure |
| `info` | Display current build configuration |
| `run` | Build and immediately run the application |
| `test-agents` | Compile and test the agents module |
| `test-net` | Compile and test the networking module |
| `test-scheduler` | Compile and test the scheduler module |
| `help` | Show all available targets and their descriptions |

### Build Dependencies

The system automatically tracks dependencies between modules:

- `volcom_main_new.c` depends on all module headers
- Agent modules depend on utils, networking, and scheduler
- Network module is self-contained
- Scheduler depends on networking for communication
- Utils depend on sysinfo for system monitoring

### Compiler Flags

- **Standard Build**: `-Wall -Wextra -pthread -std=c99`
- **Debug Build**: Adds `-g -DDEBUG -O0`
- **Release Build**: Adds `-O2 -DNDEBUG`
- **Libraries**: `-lcjson -lm -lpthread`

### Include Paths

The build system automatically includes:
- `./volcom_agents` - Agent behavior headers
- `./volcom_net` - Network communication headers  
- `./volcom_scheduler` - Task scheduling headers
- `./volcom_utils` - Utility function headers
- `./volcom_sysinfo` - System information headers
- `/usr/include/cjson` - JSON library headers

## Contributing

When contributing to this codebase:
1. Follow the established module structure
2. Maintain the single-header-per-module principle
3. Add appropriate tests
4. Update documentation
5. Ensure backward compatibility where possible

## File Structure Overview

### New Files Created in Refactoring

```
src/uspacehelper/
├── volcom_main_new.c              # NEW: Refactored main entry point
├── Makefile_new                   # NEW: Updated build system
├── README_REFACTORED.md           # NEW: This documentation
│
├── volcom_agents/                 # NEW: Agent behavior module
│   ├── volcom_agents.h           # NEW: Unified agent header
│   ├── employer_mode.c           # NEW: Refactored employer logic
│   ├── employee_mode.c           # NEW: Refactored employee logic
│   └── task_buffer.c             # NEW: Thread-safe task queue
│
├── volcom_net/                    # NEW: Network communication module
│   ├── volcom_net.h              # NEW: Network protocol header
│   └── protocol.c                # MOVED: From volcom_comm/utils/
│
├── volcom_scheduler/              # NEW: Task scheduling module  
│   ├── volcom_scheduler.h        # NEW: Scheduling algorithms header
│   └── task_scheduler.c          # NEW: Task distribution logic
│
├── volcom_utils/                  # NEW: Utility functions module
│   ├── volcom_utils.h            # NEW: Utilities header
│   └── net_utils.c               # REFACTORED: From volcom_comm/utils/
│
└── volcom_comm/                   # LEGACY: Original implementation (preserved)
    ├── comm_main.c               # LEGACY: Original main file
    ├── employer/                 # LEGACY: Original employer code
    ├── employee/                 # LEGACY: Original employee code
    └── utils/                    # LEGACY: Original utilities
```

### Key Design Decisions

1. **Naming Convention**: All new files use `volcom_main_new.c` and `Makefile_new` to avoid conflicts with existing code
2. **Backward Compatibility**: Original `volcom_comm` folder is preserved unchanged
3. **Single Headers**: Each module has exactly one header file as requested
4. **Clear Separation**: Network, scheduling, and agent logic are completely separated
5. **Thread Safety**: All shared data structures use proper synchronization

### Migration Path

To fully migrate from the old system to the new:

1. **Current State**: Both systems coexist
   ```bash
   # Old system
   make -f Makefile
   ./volcom_main
   
   # New system  
   make -f Makefile_new
   ./volcom_main_new
   ```

2. **Future Migration**: When ready to replace the old system:
   ```bash
   # Backup old system
   mv Makefile Makefile_old
   mv volcom_main.c volcom_main_old.c
   
   # Promote new system
   mv Makefile_new Makefile
   mv volcom_main_new.c volcom_main.c
   ```

3. **Complete Transition**: Eventually remove legacy `volcom_comm` folder after thorough testing
