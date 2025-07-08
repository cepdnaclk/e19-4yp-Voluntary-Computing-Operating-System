#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct memory_info_s {
    unsigned long total;
    unsigned long free;
    unsigned long buffers;
    unsigned long cached;
    unsigned long swap_total;
    unsigned long swap_free;
};

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

struct cpu_info_s {
    int cpu_count;
    int logical_processors;
    char model[256];
    struct cpu_core_usage_s overall_usage;
    struct cpu_core_usage_s *core_usage;  // Array for each logical processor
};

struct gpu_info_s {
    char model[256];
    unsigned long memory;
    char driver[256];
};

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
    FILE *fp = fopen("/proc/cpuinfo", "r");

    if (!fp) {
        perror("Failed to open /proc/cpuinfo");
        strcpy(cpu_info.model, "Unknown");
        return cpu_info;
    }

    char line[256];
    int first_cpu = 1;
    strcpy(cpu_info.model, "Unknown");

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

struct cpu_info_s get_cpu_usage(){

    struct cpu_info_s cpu_usage = {0};
    FILE *fp = fopen("/proc/stat", "r");

    if (!fp) {
        perror("Failed to open /proc/stat");
        return cpu_usage;
    }

    char line[256];
    int core_count = 0;

    // First pass: count the number of logical processors
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "cpu", 3) == 0 && line[3] >= '0' && line[3] <= '9') {
            core_count++;
        }
    }
    
    // Allocate memory for core usage array
    cpu_usage.core_usage = malloc(core_count * sizeof(struct cpu_core_usage_s));
    if (!cpu_usage.core_usage) {
        perror("Failed to allocate memory for core usage");
        fclose(fp);
        return cpu_usage;
    }
    
    cpu_usage.logical_processors = core_count;
    
    // Reset file pointer to beginning
    rewind(fp);
    
    int current_core = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "cpu ", 4) == 0) {
            // Overall CPU usage
            sscanf(line, "cpu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu", 
                   &cpu_usage.overall_usage.user, &cpu_usage.overall_usage.nice, 
                   &cpu_usage.overall_usage.system, &cpu_usage.overall_usage.idle, 
                   &cpu_usage.overall_usage.iowait, &cpu_usage.overall_usage.irq, 
                   &cpu_usage.overall_usage.softirq, &cpu_usage.overall_usage.steal, 
                   &cpu_usage.overall_usage.guest, &cpu_usage.overall_usage.guest_nice);
            
            // Calculate overall totals
            cpu_usage.overall_usage.total = cpu_usage.overall_usage.user + cpu_usage.overall_usage.nice + 
                                           cpu_usage.overall_usage.system + cpu_usage.overall_usage.idle + 
                                           cpu_usage.overall_usage.iowait + cpu_usage.overall_usage.irq + 
                                           cpu_usage.overall_usage.softirq + cpu_usage.overall_usage.steal;
            cpu_usage.overall_usage.used = cpu_usage.overall_usage.total - cpu_usage.overall_usage.idle - cpu_usage.overall_usage.iowait;
            
            if (cpu_usage.overall_usage.total > 0) {
                cpu_usage.overall_usage.usage_percent = (double)cpu_usage.overall_usage.used / cpu_usage.overall_usage.total * 100.0;
            }
        }
        else if (strncmp(line, "cpu", 3) == 0 && line[3] >= '0' && line[3] <= '9' && current_core < core_count) {
            // Individual CPU core usage
            sscanf(line, "cpu%*d %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu", 
                   &cpu_usage.core_usage[current_core].user, &cpu_usage.core_usage[current_core].nice, 
                   &cpu_usage.core_usage[current_core].system, &cpu_usage.core_usage[current_core].idle, 
                   &cpu_usage.core_usage[current_core].iowait, &cpu_usage.core_usage[current_core].irq, 
                   &cpu_usage.core_usage[current_core].softirq, &cpu_usage.core_usage[current_core].steal, 
                   &cpu_usage.core_usage[current_core].guest, &cpu_usage.core_usage[current_core].guest_nice);
            
            // Calculate per-core totals
            cpu_usage.core_usage[current_core].total = cpu_usage.core_usage[current_core].user + 
                                                      cpu_usage.core_usage[current_core].nice + 
                                                      cpu_usage.core_usage[current_core].system + 
                                                      cpu_usage.core_usage[current_core].idle + 
                                                      cpu_usage.core_usage[current_core].iowait + 
                                                      cpu_usage.core_usage[current_core].irq + 
                                                      cpu_usage.core_usage[current_core].softirq + 
                                                      cpu_usage.core_usage[current_core].steal;
            cpu_usage.core_usage[current_core].used = cpu_usage.core_usage[current_core].total - 
                                                     cpu_usage.core_usage[current_core].idle - 
                                                     cpu_usage.core_usage[current_core].iowait;
            
            if (cpu_usage.core_usage[current_core].total > 0) {
                cpu_usage.core_usage[current_core].usage_percent = 
                    (double)cpu_usage.core_usage[current_core].used / cpu_usage.core_usage[current_core].total * 100.0;
            }
            
            current_core++;
        }
    }

    fclose(fp);
    return cpu_usage;
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
                start += 2; // Skip ": "
                strncpy(gpu_info.model, start, sizeof(gpu_info.model) - 1);
                // Remove newline if present
                char *newline = strchr(gpu_info.model, '\n');
                if (newline) *newline = '\0';
            }
        }
        pclose(fp);
    }
    
    return gpu_info;
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
    
    unsigned long used_memory = mem_info.total - mem_info.free - mem_info.buffers - mem_info.cached;
    double memory_usage_percent = (double)used_memory / mem_info.total * 100.0;
    printf("  Memory Usage:  %.2f%%\n", memory_usage_percent);
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

void free_cpu_usage(struct cpu_info_s *cpu_usage) {
    if (cpu_usage->core_usage) {
        free(cpu_usage->core_usage);
        cpu_usage->core_usage = NULL;
    }
}

int main() {
    printf("=== System Information ===\n\n");

    // Get system information
    struct memory_info_s mem_info = get_memory_info();
    struct cpu_info_s cpu_info = get_cpu_info();
    struct cpu_info_s cpu_usage = get_cpu_usage();
    struct gpu_info_s gpu_info = get_gpu_info();

    // Print system information with proper spacing
    print_memory_info(mem_info);
    printf("\n");
    
    print_cpu_info(cpu_info);
    printf("\n");
    
    print_cpu_usage(cpu_usage);
    printf("\n");
    
    print_gpu_info(gpu_info);
    printf("\n");

    // Clean up allocated memory
    free_cpu_usage(&cpu_usage);

    return 0;
}