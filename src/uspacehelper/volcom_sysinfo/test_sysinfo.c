#include "volcom_sysinfo.h"

int main() {
    printf("=== System Information Tool ===\n\n");

    // Get system information using the module
    struct memory_info_s mem_info = get_memory_info();
    struct cpu_info_s cpu_info = get_cpu_info();
    struct cpu_info_s cpu_usage = get_cpu_usage(cpu_info);
    struct gpu_info_s gpu_info = get_gpu_info();

    // Print all system information
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
