#ifndef VOLCOM_SYSINFO_H
#define VOLCOM_SYSINFO_H

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Memory information structure
struct memory_info_s {
    unsigned long total;
    unsigned long free;
    unsigned long buffers;
    unsigned long cached;
    unsigned long swap_total;
    unsigned long swap_free;
};

// CPU core usage structure
struct cpu_core_usage_s {
    unsigned long user;
    unsigned long nice;
    unsigned long system;
    unsigned long idle;
    unsigned long iowait;
    unsigned long irq;
    unsigned long softirq;
    unsigned long steal;
    unsigned long guest;
    unsigned long guest_nice;
    unsigned long total;
    unsigned long used;
    double usage_percent;
};

// CPU information structure
struct cpu_info_s {
    int cpu_count;
    int logical_processors;
    char model[256];
    struct cpu_core_usage_s overall_usage;
    struct cpu_core_usage_s *core_usage; 
};

// GPU information structure
struct gpu_info_s {
    char model[256];
    unsigned long memory;
    char driver[256];
};

// System information structure
struct system_info_s {
    struct memory_info_s mem_info;
    struct cpu_info_s cpu_info;
    struct gpu_info_s gpu_info;
};

// Memory Configuration structure
struct memory_config_s {
    unsigned long allocated_memory_size_max;
    unsigned long allocated_memory_size_high;
    unsigned long allocated_swap_memory_size_max;
    unsigned long allocated_swap_memory_size_high;
    int memory_oom_protection;
};

// CPU Configuration structure
struct cpu_config_s {
    int allocated_logical_processors;
    int allocated_cpu_share;
};

// Configuration structure to hold all system information
struct config_s {
    struct memory_config_s mem_config;
    struct cpu_config_s cpu_config;
};

// Function declarations for data collection
struct memory_info_s get_memory_info(void);
struct cpu_info_s get_cpu_info(void);
struct cpu_info_s get_cpu_usage(struct cpu_info_s cpu_info);
struct gpu_info_s get_gpu_info(void);
struct system_info_s get_system_info(void);

// Function declarations for configuration management
int save_config(const char *filename, struct config_s config);
struct config_s load_config(const char *filename);

// Function declarations for printing information
void print_memory_info(struct memory_info_s mem_info);
void print_cpu_info(struct cpu_info_s cpu_info);
void print_cpu_usage(struct cpu_info_s cpu_usage);
void print_gpu_info(struct gpu_info_s gpu_info);
void print_system_info(struct system_info_s sys_info);
void print_config(struct config_s config);

// Memory management function
void free_cpu_usage(struct cpu_info_s *cpu_usage);

// Utility functions for calculations
double calculate_memory_usage_percent(struct memory_info_s mem_info);
double calculate_cpu_usage_percent(struct cpu_core_usage_s cpu_usage);

#endif // VOLCOM_SYSINFO_H
