#ifndef VOLCOM_RCSMNGR_H
#define VOLCOM_RCSMNGR_H

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "../volcom_sysinfo/volcom_sysinfo.h"

// Default cgroup v2 mount point
#define CGROUP_V2_ROOT "/sys/fs/cgroup"
#define VOLCOM_CGROUP_NAME "volcom"
#define MAX_PATH_LEN 512
#define MAX_NAME_LEN 64

// Cgroup management structure
struct cgroup_info_s {
    char name[MAX_NAME_LEN];
    char path[MAX_PATH_LEN];
    char parent_path[MAX_PATH_LEN];
    int is_created;
    pid_t *pids;
    int pid_count;
    int pid_capacity;
};

// Cgroup manager structure
struct volcom_rcsmngr_s {
    char root_cgroup_path[MAX_PATH_LEN];
    struct cgroup_info_s main_cgroup;
    struct cgroup_info_s *child_cgroups;
    int child_count;
    int child_capacity;
    int is_initialized;
};

// Function declarations for cgroup management
int volcom_rcsmngr_init(struct volcom_rcsmngr_s *manager, const char *cgroup_name);
int volcom_create_main_cgroup(struct volcom_rcsmngr_s *manager, struct config_s config);
int volcom_create_child_cgroup(struct volcom_rcsmngr_s *manager, const char *child_name, struct config_s config);
int volcom_add_pid_to_cgroup(struct volcom_rcsmngr_s *manager, const char *cgroup_name, pid_t pid);
int volcom_remove_pid_from_cgroup(struct volcom_rcsmngr_s *manager, const char *cgroup_name, pid_t pid);
int volcom_delete_child_cgroup(struct volcom_rcsmngr_s *manager, const char *child_name);
int volcom_delete_main_cgroup(struct volcom_rcsmngr_s *manager);

// Print cgroup information
int volcom_get_cgroup_pids(struct volcom_rcsmngr_s *manager, const char *cgroup_name, pid_t **pids, int *count);
void volcom_print_cgroup_info(struct volcom_rcsmngr_s *manager);

// Cleanup and free resources
void volcom_rcsmngr_cleanup(struct volcom_rcsmngr_s *manager);

// Utility functions
int volcom_check_cgroup_v2_support(void);
int volcom_set_memory_limit(const char *cgroup_path, unsigned long limit_bytes);
int volcom_set_cpu_limit(const char *cgroup_path, int cpu_shares, int cpu_count);
int volcom_enable_controllers(const char *cgroup_path, const char *controllers);

#endif // VOLCOM_RCSMNGR_H
