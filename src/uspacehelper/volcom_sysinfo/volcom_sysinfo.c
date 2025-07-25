#include "volcom_sysinfo.h"

struct memory_info_s get_memory_info(){

    struct memory_info_s mem_info = {0};
    FILE *fp = fopen("/proc/meminfo", "r");
    
    if (!fp) {
        perror("Failed to open /proc/meminfo");
        return mem_info;
    }

    char line[256];
    
    while (fgets(line, sizeof(line), fp)){
        if (strncmp(line, "MemTotal:", 9) == 0) {
            sscanf(line, "MemTotal: %lu kB", &mem_info.total);
        } else if (strncmp(line, "MemFree:", 8) == 0) {
            sscanf(line, "MemFree: %lu kB", &mem_info.free);
        } else if (strncmp(line, "Buffers:", 8) == 0) {
            sscanf(line, "Buffers: %lu kB", &mem_info.buffers);
        } else if (strncmp(line, "Cached:", 7) == 0) {
            sscanf(line, "Cached: %lu kB", &mem_info.cached);
        } else if (strncmp(line, "SwapTotal:", 10) == 0) {
            sscanf(line, "SwapTotal: %lu kB", &mem_info.swap_total);
        } else if (strncmp(line, "SwapFree:", 9) == 0) {
            sscanf(line, "SwapFree: %lu kB", &mem_info.swap_free);
        }
    }

    fclose(fp);
    return mem_info;
}

struct cpu_info_s get_cpu_info(){

    struct cpu_info_s cpu_info = {0};
    strcpy(cpu_info.model, "Unknown");
    FILE *fp = fopen("/proc/cpuinfo", "r");

    if (!fp) {
        perror("Failed to open /proc/cpuinfo");
        return cpu_info;
    }

    char line[256];
    int first_cpu = 1;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "processor", 9) == 0) {
            cpu_info.logical_processors++;
        }
        
        if (first_cpu && strncmp(line, "cpu cores", 9) == 0) {
            sscanf(line, "cpu cores : %d", &cpu_info.cpu_count);
        }
        
        if (first_cpu && strncmp(line, "model name", 10) == 0) {
            sscanf(line, "model name : %[^\n]", cpu_info.model);
        }
        
        if (strncmp(line, "power management", 16) == 0 || 
            (strlen(line) == 1 && line[0] == '\n')) {
            first_cpu = 0;
        }
    }
    
    fclose(fp);
    return cpu_info;
}

struct cpu_info_s get_cpu_usage(struct cpu_info_s cpu_info){

    FILE *fp = fopen("/proc/stat", "r");

    if (!fp) {
        perror("Failed to open /proc/stat");
        return cpu_info;
    }

    char line[256];
    
    cpu_info.core_usage = malloc(cpu_info.logical_processors * sizeof(struct cpu_core_usage_s));
    if (!cpu_info.core_usage) {
        perror("Failed to allocate memory for core usage");
        fclose(fp);
        return cpu_info;
    }
    
    int current_core = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "cpu ", 4) == 0) {
            sscanf(line, "cpu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu", 
                   &cpu_info.overall_usage.user, &cpu_info.overall_usage.nice, 
                   &cpu_info.overall_usage.system, &cpu_info.overall_usage.idle, 
                   &cpu_info.overall_usage.iowait, &cpu_info.overall_usage.irq, 
                   &cpu_info.overall_usage.softirq, &cpu_info.overall_usage.steal, 
                   &cpu_info.overall_usage.guest, &cpu_info.overall_usage.guest_nice);
            
            cpu_info.overall_usage.total = cpu_info.overall_usage.user + cpu_info.overall_usage.nice + 
                                           cpu_info.overall_usage.system + cpu_info.overall_usage.idle + 
                                           cpu_info.overall_usage.iowait + cpu_info.overall_usage.irq + 
                                           cpu_info.overall_usage.softirq + cpu_info.overall_usage.steal;
            cpu_info.overall_usage.used = cpu_info.overall_usage.total - cpu_info.overall_usage.idle - cpu_info.overall_usage.iowait;
            
            if (cpu_info.overall_usage.total > 0) {
                cpu_info.overall_usage.usage_percent = (double)cpu_info.overall_usage.used / cpu_info.overall_usage.total * 100.0;
            }
        }
        else if (strncmp(line, "cpu", 3) == 0 && line[3] >= '0' && line[3] <= '9' && current_core < cpu_info.logical_processors) {
            sscanf(line, "cpu%*d %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu", 
                   &cpu_info.core_usage[current_core].user, &cpu_info.core_usage[current_core].nice, 
                   &cpu_info.core_usage[current_core].system, &cpu_info.core_usage[current_core].idle, 
                   &cpu_info.core_usage[current_core].iowait, &cpu_info.core_usage[current_core].irq, 
                   &cpu_info.core_usage[current_core].softirq, &cpu_info.core_usage[current_core].steal, 
                   &cpu_info.core_usage[current_core].guest, &cpu_info.core_usage[current_core].guest_nice);
            
            cpu_info.core_usage[current_core].total = cpu_info.core_usage[current_core].user + 
                                                      cpu_info.core_usage[current_core].nice + 
                                                      cpu_info.core_usage[current_core].system + 
                                                      cpu_info.core_usage[current_core].idle + 
                                                      cpu_info.core_usage[current_core].iowait + 
                                                      cpu_info.core_usage[current_core].irq + 
                                                      cpu_info.core_usage[current_core].softirq + 
                                                      cpu_info.core_usage[current_core].steal;
            cpu_info.core_usage[current_core].used = cpu_info.core_usage[current_core].total - 
                                                     cpu_info.core_usage[current_core].idle - 
                                                     cpu_info.core_usage[current_core].iowait;
            
            if (cpu_info.core_usage[current_core].total > 0) {
                cpu_info.core_usage[current_core].usage_percent = 
                    (double)cpu_info.core_usage[current_core].used / cpu_info.core_usage[current_core].total * 100.0;
            }
            
            current_core++;
        }
    }

    fclose(fp);
    return cpu_info;
}

struct gpu_info_s get_gpu_info(){

    struct gpu_info_s gpu_info = {0};
    
    strcpy(gpu_info.model, "Unknown");
    strcpy(gpu_info.driver, "Unknown");
    gpu_info.memory = 0;
    
    FILE *fp = popen("lspci | grep -i vga", "r");
    if (fp) {
        char line[256];
        if (fgets(line, sizeof(line), fp)) {
            char *start = strstr(line, ": ");
            if (start) {
                start += 2; 
                strncpy(gpu_info.model, start, sizeof(gpu_info.model) - 1);
                char *newline = strchr(gpu_info.model, '\n');
                if (newline) *newline = '\0';
            }
        }
        pclose(fp);
    }
    
    return gpu_info;
}

struct system_info_s get_system_info() {
    struct system_info_s config = {0};
    
    config.mem_info = get_memory_info();
    
    config.cpu_info = get_cpu_info();
    
    config.cpu_info = get_cpu_usage(config.cpu_info);
    
    config.gpu_info = get_gpu_info();
    
    return config;
}

int save_config(const char *filename, struct config_s config) {

    FILE *fp = fopen(filename, "w");

    if (!fp) {
        perror("Failed to open config file for writing");
        return -1;
    }

    fprintf(fp, "System Configuration:\n");
    fprintf(fp, "----------------------\n");

    fprintf(fp, "\nMemory Configuration:\n");
    fprintf(fp, "----------------------\n");
    fprintf(fp, "Memory Size (Max): %lu kB\n", config.mem_config.allocated_memory_size_max);
    fprintf(fp, "Memory Size (High): %lu kB\n", config.mem_config.allocated_memory_size_high);
    fprintf(fp, "Swap Memory Size (Max): %lu kB\n", config.mem_config.allocated_swap_memory_size_max);
    fprintf(fp, "Swap Memory Size (High): %lu kB\n", config.mem_config.allocated_swap_memory_size_high);
    fprintf(fp, "Memory OOM Protection: %d\n", config.mem_config.memory_oom_protection);
    
    fprintf(fp, "\nCPU Configuration:\n");
    fprintf(fp, "----------------------\n");
    fprintf(fp, "Allocated Logical Processors: %d\n", config.cpu_config.allocated_logical_processors);
    fprintf(fp, "Allocated CPU Share: %d\n", config.cpu_config.allocated_cpu_share);

    fclose(fp);
    return 0;
}

struct config_s load_config(const char *filename) {

    struct config_s config = {0};
    FILE *fp = fopen(filename, "r");

    if (!fp) {
        perror("Failed to open config file for reading");
        return config;
    }   
    char line[256];

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "Memory Size (Max): %lu kB", &config.mem_config.allocated_memory_size_max) == 1) continue;
        if (sscanf(line, "Memory Size (High): %lu kB", &config.mem_config.allocated_memory_size_high) == 1) continue;
        if (sscanf(line, "Swap Memory Size (Max): %lu kB", &config.mem_config.allocated_swap_memory_size_max) == 1) continue;
        if (sscanf(line, "Swap Memory Size (High): %lu kB", &config.mem_config.allocated_swap_memory_size_high) == 1) continue;
        if (sscanf(line, "Memory OOM Protection: %d", &config.mem_config.memory_oom_protection) == 1) continue;
        if (sscanf(line, "Allocated Logical Processors: %d", &config.cpu_config.allocated_logical_processors) == 1) continue;
        if (sscanf(line, "Allocated CPU Share: %d", &config.cpu_config.allocated_cpu_share) == 1) continue;
    }

    if (config.mem_config.allocated_memory_size_max == 0) {
        config.mem_config.allocated_memory_size_max = 1024 * 1024; // Default to 1 GB
    }

    if (config.mem_config.allocated_memory_size_high == 0) {
        config.mem_config.allocated_memory_size_high = 512 * 1024; // Default to 512 MB
    }

    if (config.mem_config.allocated_swap_memory_size_max == 0) {
        config.mem_config.allocated_swap_memory_size_max = 1024 * 1024; // Default to 1 GB
    }

    if (config.mem_config.allocated_swap_memory_size_high == 0) {
        config.mem_config.allocated_swap_memory_size_high = 512 * 1024; // Default to 512 MB
    }

    if (config.cpu_config.allocated_logical_processors == 0) {
        config.cpu_config.allocated_logical_processors = 1; // Default to 1 logical processor
    }

    if (config.cpu_config.allocated_cpu_share == 0) {
        config.cpu_config.allocated_cpu_share = 1024; // Default to 1024
    }   

    if (config.mem_config.memory_oom_protection == 0) {
        config.mem_config.memory_oom_protection = 1; // Default to enabled
    }

    fclose(fp);
    return config;
}

void print_memory_info(struct memory_info_s mem_info) {
    printf("Memory Information:\n");
    printf("  Total Memory:  %lu MB (%.2f GB)\n", 
           mem_info.total/1024, (double)mem_info.total/(1024*1024));
    printf("  Free Memory:   %lu MB (%.2f GB)\n", 
           mem_info.free/1024, (double)mem_info.free/(1024*1024));
    printf("  Buffers:       %lu MB\n", mem_info.buffers/1024);
    printf("  Cached:        %lu MB\n", mem_info.cached/1024);
    printf("  Total Swap:    %lu MB\n", mem_info.swap_total/1024);
    printf("  Free Swap:     %lu MB\n", mem_info.swap_free/1024);
    printf("  Memory Usage:  %.2f%%\n", calculate_memory_usage_percent(mem_info));
}

void print_cpu_info(struct cpu_info_s cpu_info) {
    printf("CPU Information:\n");
    printf("  Model:             %s\n", cpu_info.model);
    printf("  Physical cores:    %d\n", cpu_info.cpu_count);
    printf("  Logical processors: %d\n", cpu_info.logical_processors);
}

void print_cpu_usage(struct cpu_info_s cpu_usage) {
    printf("CPU Usage (since boot):\n");
    printf("  Overall CPU Usage:\n");
    printf("    User:      %lu jiffies\n", cpu_usage.overall_usage.user);
    printf("    Nice:      %lu jiffies\n", cpu_usage.overall_usage.nice);
    printf("    System:    %lu jiffies\n", cpu_usage.overall_usage.system);
    printf("    Idle:      %lu jiffies\n", cpu_usage.overall_usage.idle);
    printf("    IOWait:    %lu jiffies\n", cpu_usage.overall_usage.iowait);
    printf("    IRQ:       %lu jiffies\n", cpu_usage.overall_usage.irq);
    printf("    SoftIRQ:   %lu jiffies\n", cpu_usage.overall_usage.softirq);
    printf("    Total:     %lu jiffies\n", cpu_usage.overall_usage.total);
    printf("    Usage:     %.2f%% (active)\n", cpu_usage.overall_usage.usage_percent);
    
    printf("\n  Per-Core Usage:\n");
    for (int i = 0; i < cpu_usage.logical_processors; i++) {
        printf("    CPU%d: %.2f%% (User: %lu, System: %lu, Idle: %lu)\n", 
               i, 
               cpu_usage.core_usage[i].usage_percent,
               cpu_usage.core_usage[i].user,
               cpu_usage.core_usage[i].system,
               cpu_usage.core_usage[i].idle);
    }
}

void print_gpu_info(struct gpu_info_s gpu_info) {
    printf("GPU Information:\n");
    printf("  Model:  %s\n", gpu_info.model);
    printf("  Driver: %s\n", gpu_info.driver);
    if (gpu_info.memory > 0) {
        printf("  Memory: %lu MB\n", gpu_info.memory);
    } else {
        printf("  Memory: Unknown\n");
    }
}

void print_system_info(struct system_info_s sys_info) {
    printf("System Information:\n");
    printf("----------------------\n");
    print_memory_info(sys_info.mem_info);
    print_cpu_info(sys_info.cpu_info);
    print_cpu_usage(sys_info.cpu_info);
    print_gpu_info(sys_info.gpu_info);
}

void print_config(struct config_s config) {
    printf("Configuration Information:\n");
    printf("----------------------------\n");
    printf("Memory Configuration:\n");
    printf("  Max Allocated Memory: %lu MB\n", config.mem_config.allocated_memory_size_max / 1024);
    printf("  High Allocated Memory: %lu MB\n", config.mem_config.allocated_memory_size_high / 1024);
    printf("  Max Allocated Swap: %lu MB\n", config.mem_config.allocated_swap_memory_size_max / 1024);
    printf("  High Allocated Swap: %lu MB\n", config.mem_config.allocated_swap_memory_size_high / 1024);
    printf("  OOM Protection: %s\n", config.mem_config.memory_oom_protection ? "Enabled" : "Disabled");

    printf("\nCPU Configuration:\n");
    printf("  Allocated Logical Processors: %d\n", config.cpu_config.allocated_logical_processors);
    printf("  Allocated CPU Share: %d\n", config.cpu_config.allocated_cpu_share);
}

void free_cpu_usage(struct cpu_info_s *cpu_usage) {
    if (cpu_usage->core_usage) {
        free(cpu_usage->core_usage);
        cpu_usage->core_usage = NULL;
    }
}

// Utility function to calculate memory usage percentage
double calculate_memory_usage_percent(struct memory_info_s mem_info) {
    unsigned long used_memory = mem_info.total - mem_info.free - mem_info.buffers - mem_info.cached;
    return (double)used_memory / mem_info.total * 100.0;
}

// Utility function to calculate CPU usage percentage
double calculate_cpu_usage_percent(struct cpu_core_usage_s cpu_usage) {
    if (cpu_usage.total > 0) {
        return (double)cpu_usage.used / cpu_usage.total * 100.0;
    }
    return 0.0;
}