# Volcom System Information Module

A C library module for collecting and displaying system information on Linux systems.

## Features

-   Memory usage information (RAM and Swap)
-   CPU information (model, cores, per-core usage)
-   GPU information (basic detection)
-   Modular design with reusable structures and functions
-   Easy integration into other C programs

## Files Structure

```
volcom_sysinfo/
├── volcom_sysinfo.h        # Header file with structs and function declarations
├── volcom_sysinfo.c        # Implementation file
├── main_example.c          # Full example showing all features
├── Makefile               # Build configuration
└── README.md              # This file
```

## Building

### Build everything:

```bash
make all
```

### Build just the library:

```bash
make libvolcom_sysinfo.a
```

### Clean build files:

```bash
make clean
```

## Usage in Your Program

### 1. Include the header file:

```c
#include "volcom_sysinfo.h"
```

### 2. Use the structures and functions:

#### Get Memory Information:

```c
struct memory_info_s mem_info = get_memory_info();
printf("Total Memory: %lu MB\n", mem_info.total/1024);
printf("Memory Usage: %.2f%%\n", calculate_memory_usage_percent(mem_info));
```

#### Get CPU Information:

```c
struct cpu_info_s cpu_info = get_cpu_info();
printf("CPU Model: %s\n", cpu_info.model);
printf("Physical Cores: %d\n", cpu_info.cpu_count);
printf("Logical Processors: %d\n", cpu_info.logical_processors);
```

#### Get CPU Usage (with per-core details):

```c
struct cpu_info_s cpu_info = get_cpu_info();
struct cpu_info_s cpu_usage = get_cpu_usage(cpu_info);
printf("Overall CPU Usage: %.2f%%\n", cpu_usage.overall_usage.usage_percent);

// Per-core usage
for (int i = 0; i < cpu_usage.logical_processors; i++) {
    printf("CPU%d: %.2f%%\n", i, cpu_usage.core_usage[i].usage_percent);
}

// Don't forget to free memory!
free_cpu_usage(&cpu_usage);
```

#### Get GPU Information:

```c
struct gpu_info_s gpu_info = get_gpu_info();
printf("GPU: %s\n", gpu_info.model);
```

### 3. Compile your program:

#### Method 1: Link with static library

```bash
gcc your_program.c -L. -lvolcom_sysinfo -o your_program
```

#### Method 2: Link object file directly

```bash
gcc your_program.c volcom_sysinfo.o -o your_program
```

## Data Structures

### struct memory_info_s

-   `total`, `free`, `buffers`, `cached` - Memory amounts in KB
-   `swap_total`, `swap_free` - Swap amounts in KB

### struct cpu_core_usage_s

-   Time spent in different CPU states (in jiffies)
-   `usage_percent` - Calculated usage percentage

### struct cpu_info_s

-   `cpu_count` - Physical CPU cores
-   `logical_processors` - Logical processors (with hyperthreading)
-   `model[256]` - CPU model name
-   `overall_usage` - Overall CPU usage stats
-   `core_usage[]` - Array of per-core usage stats

### struct gpu_info_s

-   `model[256]` - GPU model name
-   `driver[256]` - Driver information
-   `memory` - GPU memory (if available)

## Available Functions

### Data Collection:

-   `get_memory_info()` - Get memory and swap information
-   `get_cpu_info()` - Get CPU model and core count
-   `get_cpu_usage()` - Get CPU usage statistics
-   `get_gpu_info()` - Get GPU information

### Display Functions:

-   `print_memory_info()` - Print formatted memory info
-   `print_cpu_info()` - Print formatted CPU info
-   `print_cpu_usage()` - Print formatted CPU usage
-   `print_gpu_info()` - Print formatted GPU info

### Utility Functions:

-   `calculate_memory_usage_percent()` - Calculate memory usage percentage
-   `calculate_cpu_usage_percent()` - Calculate CPU usage percentage
-   `free_cpu_usage()` - Free allocated memory for CPU usage data

## Examples

See `main_example.c` for a comprehensive example showing all features.

## Notes

-   The module reads from `/proc/meminfo`, `/proc/cpuinfo`, and `/proc/stat`
-   CPU usage data represents cumulative time since boot
-   Memory for per-core CPU usage is dynamically allocated and must be freed
-   GPU detection uses `lspci` command and may require root privileges for full information

## Integration Tips

1. **Copy the module files** (`volcom_sysinfo.h` and `volcom_sysinfo.c`) to your project
2. **Include the header** in your source files
3. **Add the source file** to your build process
4. **Remember to call** `free_cpu_usage()` to prevent memory leaks
5. **Handle error cases** - functions may return empty/zero values on failure
