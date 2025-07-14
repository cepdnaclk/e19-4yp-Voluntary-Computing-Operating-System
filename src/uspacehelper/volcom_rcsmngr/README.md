# Volcom Resource Manager Module (volcom_rcsmngr)

A C library module for managing cgroups v2 to control resource allocation for the Volcom system.

## Features

-   **Cgroup v2 Management**: Create, manage, and delete cgroups using the modern cgroups v2 interface
-   **Resource Limits**: Set memory and CPU limits based on configuration
-   **Process Management**: Add/remove processes to/from cgroups
-   **Hierarchical Structure**: Support for main cgroup with child cgroups
-   **Configuration Integration**: Uses `config_s` structure from volcom_sysinfo module

## Files Structure

```
volcom_rcsmngr/
├── volcom_rcsmngr.h        # Header file with structs and function declarations
├── volcom_rcsmngr.c        # Implementation file
├── test_rcsmngr.c          # Test program demonstrating usage
├── Makefile               # Build configuration
└── README.md              # This file
```

## Dependencies

-   **volcom_sysinfo module**: For configuration structures
-   **Linux kernel with cgroups v2 support**
-   **Root privileges**: Required for creating and managing cgroups

## Building

### Build the library:

```bash
make libvolcom_rcsmngr.a
```

### Build everything including test:

```bash
make all
```

### Clean build files:

```bash
make clean
```

## Usage

### 1. Include the header:

```c
#include "volcom_rcsmngr.h"
```

### 2. Initialize the resource manager:

```c
struct volcom_rcsmngr_s manager;
if (volcom_rcsmngr_init(&manager, "my_cgroup") != 0) {
    // Handle error
}
```

### 3. Create main cgroup with configuration:

```c
struct config_s config = load_config("config.txt");
if (volcom_create_main_cgroup(&manager, config) != 0) {
    // Handle error
}
```

### 4. Create child cgroups:

```c
if (volcom_create_child_cgroup(&manager, "worker1", config) != 0) {
    // Handle error
}
```

### 5. Add processes to cgroups:

```c
pid_t pid = 1234;
if (volcom_add_pid_to_cgroup(&manager, "worker1", pid) != 0) {
    // Handle error
}
```

### 6. Monitor and manage:

```c
// Print current state
volcom_print_cgroup_info(&manager);

// Get PIDs in a cgroup
pid_t *pids;
int count;
volcom_get_cgroup_pids(&manager, "worker1", &pids, &count);

// Remove PID from cgroup
volcom_remove_pid_from_cgroup(&manager, "worker1", pid);
```

### 7. Cleanup:

```c
// Delete child cgroups
volcom_delete_child_cgroup(&manager, "worker1");

// Delete main cgroup
volcom_delete_main_cgroup(&manager);

// Free resources
volcom_rcsmngr_cleanup(&manager);
```

## API Reference

### Core Functions

#### `volcom_rcsmngr_init(manager, cgroup_name)`

Initialize the resource manager with a specific cgroup name.

#### `volcom_create_main_cgroup(manager, config)`

Create the main cgroup with resource limits based on configuration:

-   Memory limit from `config.mem_config.allocated_memory_size_max`
-   CPU shares from `config.cpu_config.allocated_cpu_share`
-   CPU count from `config.cpu_config.allocated_logical_processors`

#### `volcom_create_child_cgroup(manager, child_name, config)`

Create a child cgroup under the main cgroup with proportional resource limits.

#### `volcom_add_pid_to_cgroup(manager, cgroup_name, pid)`

Add a process ID to a specific cgroup (main or child).

#### `volcom_remove_pid_from_cgroup(manager, cgroup_name, pid)`

Remove a process ID from a specific cgroup.

### Management Functions

#### `volcom_delete_child_cgroup(manager, child_name)`

Delete a specific child cgroup.

#### `volcom_delete_main_cgroup(manager)`

Delete the main cgroup and all its children.

#### `volcom_get_cgroup_pids(manager, cgroup_name, pids, count)`

Get list of all PIDs in a specific cgroup.

#### `volcom_print_cgroup_info(manager)`

Print detailed information about all cgroups and their processes.

#### `volcom_rcsmngr_cleanup(manager)`

Free all allocated resources and reset the manager.

### Utility Functions

#### `volcom_check_cgroup_v2_support()`

Check if cgroups v2 is supported and mounted on the system.

#### `volcom_set_memory_limit(cgroup_path, limit_bytes)`

Set memory limit for a specific cgroup path.

#### `volcom_set_cpu_limit(cgroup_path, cpu_shares, cpu_count)`

Set CPU limits (weight and max) for a specific cgroup path.

#### `volcom_enable_controllers(cgroup_path, controllers)`

Enable specific controllers (memory, cpu) for a cgroup.

## Configuration Integration

The module uses the `config_s` structure from volcom_sysinfo:

```c
struct config_s {
    struct memory_config_s mem_config;  // Memory configuration
    struct cpu_config_s cpu_config;     // CPU configuration
};
```

### Memory Configuration:

-   `allocated_memory_size_max`: Maximum memory limit (KB)
-   `allocated_memory_size_high`: High watermark for child cgroups (KB)

### CPU Configuration:

-   `allocated_logical_processors`: Number of CPU cores to allocate
-   `allocated_cpu_share`: CPU time share (weight)

## Cgroups v2 Features Used

-   **memory.max**: Set maximum memory usage
-   **cpu.weight**: Set CPU scheduling weight (equivalent to shares)
-   **cpu.max**: Set CPU quota and period
-   **cgroup.procs**: Manage processes in cgroup
-   **cgroup.subtree_control**: Enable controllers for child cgroups

## Testing

Run the test program to see the module in action:

```bash
sudo ./test_rcsmngr [config_file]
```

**Note**: Root privileges are required for cgroup management.

The test program will:

1. Create main and child cgroups
2. Set resource limits based on configuration
3. Create test processes and add them to cgroups
4. Demonstrate PID management
5. Clean up all resources

## Error Handling

All functions return:

-   `0` on success
-   `-1` on error (with error message printed to stderr)

Common error scenarios:

-   Insufficient privileges (need root for cgroup operations)
-   Cgroups v2 not supported or not mounted
-   Invalid parameters
-   Resource allocation failures

## System Requirements

-   Linux kernel 4.5+ with cgroups v2 support
-   Cgroups v2 mounted at `/sys/fs/cgroup`
-   Root privileges for cgroup operations
-   GCC compiler with C99 support

## Example Output

```
=== Volcom Resource Manager Information ===
Main Cgroup: volcom_test
  Path: /sys/fs/cgroup/volcom_test
  Status: Created
  PIDs: 1

Child Cgroups: 2
  [1] worker1
      Path: /sys/fs/cgroup/volcom_test/worker1
      Status: Created
      PIDs: 1
  [2] worker2
      Path: /sys/fs/cgroup/volcom_test/worker2
      Status: Created
      PIDs: 1
==========================================
```
