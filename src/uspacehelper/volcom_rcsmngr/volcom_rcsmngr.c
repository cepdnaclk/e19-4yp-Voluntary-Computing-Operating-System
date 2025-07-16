#define _GNU_SOURCE
#include "volcom_rcsmngr.h"

int volcom_rcsmngr_init(struct volcom_rcsmngr_s *manager, const char *cgroup_name) {

    if (!manager || !cgroup_name) {
        fprintf(stderr, "Invalid parameters for rcsmngr_init\n");
        return -1;
    }

    if (volcom_check_cgroup_v2_support() != 0) {
        fprintf(stderr, "Cgroup v2 is not supported or not mounted\n");
        return -1;
    }

    memset(manager, 0, sizeof(struct volcom_rcsmngr_s));
    
    snprintf(manager->root_cgroup_path, MAX_PATH_LEN, "%s/%s", CGROUP_V2_ROOT, cgroup_name);
    
    strncpy(manager->main_cgroup.name, cgroup_name, MAX_NAME_LEN - 1);
    strncpy(manager->main_cgroup.path, manager->root_cgroup_path, MAX_PATH_LEN - 1);
    strncpy(manager->main_cgroup.parent_path, CGROUP_V2_ROOT, MAX_PATH_LEN - 1);
    manager->main_cgroup.is_created = 0;
    manager->main_cgroup.pid_capacity = 10;
    manager->main_cgroup.pids = malloc(manager->main_cgroup.pid_capacity * sizeof(pid_t));

    manager->child_capacity = 5;
    manager->child_cgroups = malloc(manager->child_capacity * sizeof(struct cgroup_info_s));
    
    if (!manager->main_cgroup.pids || !manager->child_cgroups) {
        fprintf(stderr, "Failed to allocate memory for cgroup structures\n");
        volcom_rcsmngr_cleanup(manager);
        return -1;
    }
    
    manager->is_initialized = 1;
    printf("\nResource manager initialized for cgroup: %s\n", cgroup_name);
    return 0;
}

int volcom_create_main_cgroup(struct volcom_rcsmngr_s *manager, struct config_s config) {

    if (!manager || !manager->is_initialized) {
        fprintf(stderr, "Manager not initialized\n");
        return -1;
    }

    if (manager->main_cgroup.is_created) {
        fprintf(stderr, "Main cgroup already created\n");
        return -1;
    }

    if (mkdir(manager->main_cgroup.path, 0755) != 0 && errno != EEXIST) {
        perror("Failed to create main cgroup directory");
        return -1;
    }

    if (volcom_set_memory_limit(manager->main_cgroup.path, config.mem_config.allocated_memory_size_max * 1024) != 0) {
        fprintf(stderr, "Failed to set memory limit\n");
        return -1;
    }

    if (volcom_set_cpu_limit(manager->main_cgroup.path, config.cpu_config.allocated_cpu_share, 
                            config.cpu_config.allocated_logical_processors) != 0) {
        fprintf(stderr, "Failed to set CPU limit\n");
        return -1;
    }

    if (volcom_enable_controllers(manager->main_cgroup.path, "memory cpu") != 0) {
        fprintf(stderr, "Failed to enable controllers in main cgroup for children\n");
        return -1;
    }

    manager->main_cgroup.is_created = 1;
    printf("Main cgroup created: %s\n", manager->main_cgroup.path);
    printf("  Memory limit: %lu KB\n", config.mem_config.allocated_memory_size_max);
    printf("  CPU share: %d\n", config.cpu_config.allocated_cpu_share);
    printf("  CPU count: %d\n", config.cpu_config.allocated_logical_processors);
    
    return 0;
}

int volcom_create_child_cgroup(struct volcom_rcsmngr_s *manager, const char *child_name, struct config_s config) {

    if (!manager || !manager->is_initialized || !child_name || !manager->main_cgroup.is_created) {
        fprintf(stderr, "Invalid parameters or main cgroup not created\n");
        return -1;
    }

    if (manager->child_count >= manager->child_capacity) {
        manager->child_capacity *= 2;
        manager->child_cgroups = realloc(manager->child_cgroups, 
                                        manager->child_capacity * sizeof(struct cgroup_info_s));
        if (!manager->child_cgroups) {
            fprintf(stderr, "Failed to expand child cgroups array\n");
            return -1;
        }
    }

    struct cgroup_info_s *child = &manager->child_cgroups[manager->child_count];
    memset(child, 0, sizeof(struct cgroup_info_s));

    strncpy(child->name, child_name, MAX_NAME_LEN - 1);
    snprintf(child->path, MAX_PATH_LEN, "%s/%s", manager->main_cgroup.path, child_name);
    strncpy(child->parent_path, manager->main_cgroup.path, MAX_PATH_LEN - 1);
    
    if (mkdir(child->path, 0755) != 0 && errno != EEXIST) {
        perror("Failed to create child cgroup directory");
        return -1;
    }

    unsigned long child_memory_limit = config.mem_config.allocated_memory_size_high * 1024;
    if (volcom_set_memory_limit(child->path, child_memory_limit) != 0) {
        fprintf(stderr, "Failed to set memory limit for child cgroup\n");
        return -1;
    }

    int child_cpu_share = config.cpu_config.allocated_cpu_share / 2;
    if (volcom_set_cpu_limit(child->path, child_cpu_share, 1) != 0) {
        fprintf(stderr, "Failed to set CPU limit for child cgroup\n");
        return -1;
    }

    child->pid_capacity = 10;
    child->pids = malloc(child->pid_capacity * sizeof(pid_t));
    if (!child->pids) {
        fprintf(stderr, "Failed to allocate PID array for child cgroup\n");
        rmdir(child->path);
        return -1;
    }

    child->is_created = 1;
    manager->child_count++;

    printf("Child cgroup created: %s\n", child->path);
    printf("  Memory limit: %lu KB\n", child_memory_limit / 1024);
    printf("  CPU share: %d\n", child_cpu_share);
    
    return 0;
}

int volcom_add_pid_to_cgroup(struct volcom_rcsmngr_s *manager, const char *cgroup_name, pid_t pid) {
    
    if (!manager || !manager->is_initialized || !cgroup_name || pid <= 0) {
        fprintf(stderr, "Invalid parameters for add_pid_to_cgroup\n");
        return -1;
    }

    struct cgroup_info_s *target_cgroup = NULL;
    
    if (strcmp(cgroup_name, manager->main_cgroup.name) == 0) {
        target_cgroup = &manager->main_cgroup;
    } else {
        for (int i = 0; i < manager->child_count; i++) {
            if (strcmp(cgroup_name, manager->child_cgroups[i].name) == 0) {
                target_cgroup = &manager->child_cgroups[i];
                break;
            }
        }
    }

    if (!target_cgroup || !target_cgroup->is_created) {
        fprintf(stderr, "Cgroup '%s' not found or not created\n", cgroup_name);
        return -1;
    }

    // Write PID to cgroup.procs file
    char procs_path[MAX_PATH_LEN];
    snprintf(procs_path, MAX_PATH_LEN, "%s/cgroup.procs", target_cgroup->path);
    
    FILE *fp = fopen(procs_path, "w");
    if (!fp) {
        perror("Failed to open cgroup.procs file");
        return -1;
    }

    if (fprintf(fp, "%d\n", pid) < 0) {
        perror("Failed to write PID to cgroup.procs");
        fclose(fp);
        return -1;
    }

    fclose(fp);

    if (target_cgroup->pid_count >= target_cgroup->pid_capacity) {
        target_cgroup->pid_capacity *= 2;
        target_cgroup->pids = realloc(target_cgroup->pids, 
                                     target_cgroup->pid_capacity * sizeof(pid_t));
        if (!target_cgroup->pids) {
            fprintf(stderr, "Failed to expand PID array\n");
            return -1;
        }
    }

    target_cgroup->pids[target_cgroup->pid_count] = pid;
    target_cgroup->pid_count++;

    printf("PID %d added to cgroup '%s'\n", pid, cgroup_name);
    return 0;
}

int volcom_remove_pid_from_cgroup(struct volcom_rcsmngr_s *manager, const char *cgroup_name, pid_t pid) {
    
    if (!manager || !manager->is_initialized || !cgroup_name || pid <= 0) {
        fprintf(stderr, "Invalid parameters for remove_pid_from_cgroup\n");
        return -1;
    }

    struct cgroup_info_s *target_cgroup = NULL;
    
    if (strcmp(cgroup_name, manager->main_cgroup.name) == 0) {
        target_cgroup = &manager->main_cgroup;
    } else {
        for (int i = 0; i < manager->child_count; i++) {
            if (strcmp(cgroup_name, manager->child_cgroups[i].name) == 0) {
                target_cgroup = &manager->child_cgroups[i];
                break;
            }
        }
    }

    if (!target_cgroup) {
        fprintf(stderr, "Cgroup '%s' not found\n", cgroup_name);
        return -1;
    }

    // Remove PID from our tracking array
    for (int i = 0; i < target_cgroup->pid_count; i++) {
        if (target_cgroup->pids[i] == pid) {
            // Shift remaining PIDs
            for (int j = i; j < target_cgroup->pid_count - 1; j++) {
                target_cgroup->pids[j] = target_cgroup->pids[j + 1];
            }
            target_cgroup->pid_count--;
            printf("PID %d removed from cgroup '%s'\n", pid, cgroup_name);
            return 0;
        }
    }

    fprintf(stderr, "PID %d not found in cgroup '%s'\n", pid, cgroup_name);
    return -1;
}

int volcom_delete_child_cgroup(struct volcom_rcsmngr_s *manager, const char *child_name) {
    
    if (!manager || !manager->is_initialized || !child_name) {
        fprintf(stderr, "Invalid parameters for delete_child_cgroup\n");
        return -1;
    }

    for (int i = 0; i < manager->child_count; i++) {
        if (strcmp(manager->child_cgroups[i].name, child_name) == 0) {
            struct cgroup_info_s *child = &manager->child_cgroups[i];
            
            // Free PID array
            if (child->pids) {
                free(child->pids);
            }

            // Remove the directory
            if (rmdir(child->path) != 0) {
                perror("Failed to remove child cgroup directory");
                return -1;
            }

            // Shift remaining child cgroups
            for (int j = i; j < manager->child_count - 1; j++) {
                manager->child_cgroups[j] = manager->child_cgroups[j + 1];
            }
            manager->child_count--;

            printf("Child cgroup '%s' deleted\n", child_name);
            return 0;
        }
    }

    fprintf(stderr, "Child cgroup '%s' not found\n", child_name);
    return -1;
}

int volcom_delete_main_cgroup(struct volcom_rcsmngr_s *manager) {
    
    if (!manager || !manager->is_initialized) {
        fprintf(stderr, "Manager not initialized\n");
        return -1;
    }

    while (manager->child_count > 0) {
        volcom_delete_child_cgroup(manager, manager->child_cgroups[0].name);
    }

    if (manager->main_cgroup.is_created) {
        if (rmdir(manager->main_cgroup.path) != 0) {
            perror("Failed to remove main cgroup directory");
            return -1;
        }
        manager->main_cgroup.is_created = 0;
        printf("Main cgroup deleted: %s\n", manager->main_cgroup.path);
    }

    return 0;
}

int volcom_get_cgroup_pids(struct volcom_rcsmngr_s *manager, const char *cgroup_name, pid_t **pids, int *count) {
    
    if (!manager || !manager->is_initialized || !cgroup_name || !pids || !count) {
        fprintf(stderr, "Invalid parameters for get_cgroup_pids\n");
        return -1;
    }

    struct cgroup_info_s *target_cgroup = NULL;
    
    // Find the target cgroup
    if (strcmp(cgroup_name, manager->main_cgroup.name) == 0) {
        target_cgroup = &manager->main_cgroup;
    } else {
        for (int i = 0; i < manager->child_count; i++) {
            if (strcmp(cgroup_name, manager->child_cgroups[i].name) == 0) {
                target_cgroup = &manager->child_cgroups[i];
                break;
            }
        }
    }

    if (!target_cgroup) {
        fprintf(stderr, "Cgroup '%s' not found\n", cgroup_name);
        return -1;
    }

    *count = target_cgroup->pid_count;
    if (*count > 0) {
        *pids = malloc(*count * sizeof(pid_t));
        if (!*pids) {
            fprintf(stderr, "Failed to allocate memory for PID list\n");
            return -1;
        }
        memcpy(*pids, target_cgroup->pids, *count * sizeof(pid_t));
    } else {
        *pids = NULL;
    }

    return 0;
}

void volcom_print_cgroup_info(struct volcom_rcsmngr_s *manager) {
    
    if (!manager || !manager->is_initialized) {
        printf("Resource manager not initialized\n");
        return;
    }

    printf("\n=== Volcom Resource Manager Information ===\n");
    printf("Main Cgroup: %s\n", manager->main_cgroup.name);
    printf("  Path: %s\n", manager->main_cgroup.path);
    printf("  Status: %s\n", manager->main_cgroup.is_created ? "Created" : "Not Created");
    printf("  PIDs: %d\n", manager->main_cgroup.pid_count);
    
    if (manager->main_cgroup.pid_count > 0) {
        printf("  PID List: ");
        for (int i = 0; i < manager->main_cgroup.pid_count; i++) {
            printf("%d ", manager->main_cgroup.pids[i]);
        }
        printf("\n");
    }

    printf("\nChild Cgroups: %d\n", manager->child_count);
    for (int i = 0; i < manager->child_count; i++) {
        struct cgroup_info_s *child = &manager->child_cgroups[i];
        printf("  [%d] %s\n", i + 1, child->name);
        printf("      Path: %s\n", child->path);
        printf("      Status: %s\n", child->is_created ? "Created" : "Not Created");
        printf("      PIDs: %d\n", child->pid_count);
        
        if (child->pid_count > 0) {
            printf("      PID List: ");
            for (int j = 0; j < child->pid_count; j++) {
                printf("%d ", child->pids[j]);
            }
            printf("\n");
        }
    }
    printf("==========================================\n\n");
}

void volcom_rcsmngr_cleanup(struct volcom_rcsmngr_s *manager) {
    
    if (!manager) return;

    if (manager->main_cgroup.pids) {
        free(manager->main_cgroup.pids);
    }

    for (int i = 0; i < manager->child_count; i++) {
        if (manager->child_cgroups[i].pids) {
            free(manager->child_cgroups[i].pids);
        }
    }

    if (manager->child_cgroups) {
        free(manager->child_cgroups);
    }

    memset(manager, 0, sizeof(struct volcom_rcsmngr_s));
    printf("Resource manager cleanup completed\n");
}

int volcom_check_cgroup_v2_support(void) {
    
    struct stat st;
    char cgroup_version_path[] = "/sys/fs/cgroup/cgroup.controllers";
    
    if (stat(cgroup_version_path, &st) == 0) {
        return 0; // cgroup v2 is supported
    }
    
    return -1; // cgroup v2 not supported or not mounted
}

int volcom_set_memory_limit(const char *cgroup_path, unsigned long limit_bytes) {
    
    char memory_max_path[MAX_PATH_LEN];
    snprintf(memory_max_path, MAX_PATH_LEN, "%s/memory.max", cgroup_path);
    
    FILE *fp = fopen(memory_max_path, "w");
    if (!fp) {
        perror("Failed to open memory.max file");
        return -1;
    }

    if (fprintf(fp, "%lu\n", limit_bytes) < 0) {
        perror("Failed to write memory limit");
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

int volcom_set_cpu_limit(const char *cgroup_path, int cpu_shares, int cpu_count) {
    char cpu_weight_path[MAX_PATH_LEN];
    char cpu_max_path[MAX_PATH_LEN];
    
    snprintf(cpu_weight_path, MAX_PATH_LEN, "%s/cpu.weight", cgroup_path);
    FILE *fp = fopen(cpu_weight_path, "w");
    if (fp) {
        fprintf(fp, "%d\n", cpu_shares);
        fclose(fp);
    }

    snprintf(cpu_max_path, MAX_PATH_LEN, "%s/cpu.max", cgroup_path);
    fp = fopen(cpu_max_path, "w");
    if (fp) {
        if (cpu_count > 0) {
            // Set quota to allow usage of specified number of CPUs
            fprintf(fp, "%d 100000\n", cpu_count * 100000);
        } else {
            fprintf(fp, "max 100000\n");
        }
        fclose(fp);
    }

    return 0;
}

int volcom_enable_controllers(const char *cgroup_path, const char *controllers) {
    
    char subtree_control_path[MAX_PATH_LEN];
    snprintf(subtree_control_path, MAX_PATH_LEN, "%s/cgroup.subtree_control", cgroup_path);
    
    // Parse controllers and enable them one by one
    char *controllers_copy = strdup(controllers);
    if (!controllers_copy) {
        fprintf(stderr, "Failed to allocate memory for controllers string\n");
        return -1;
    }
    
    char *token = strtok(controllers_copy, " ");
    while (token != NULL) {
        FILE *fp = fopen(subtree_control_path, "w");
        if (!fp) {
            perror("Failed to open cgroup.subtree_control file");
            free(controllers_copy);
            return -1;
        }

        char enable_command[64];
        snprintf(enable_command, sizeof(enable_command), "+%s", token);
        
        if (fprintf(fp, "%s", enable_command) < 0) {
            fprintf(stderr, "Failed to enable controller '%s'\n", token);
            fclose(fp);
            free(controllers_copy);
            return -1;
        }

        fclose(fp);
        printf("Enabled controller '%s' in %s\n", token, cgroup_path);
        
        token = strtok(NULL, " ");
    }
    
    free(controllers_copy);
    return 0;
}
