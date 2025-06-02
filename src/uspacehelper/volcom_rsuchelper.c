#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#define DEVICE_PATH "/dev/resalloc"
#define TASK_NAME_LEN 64

struct task_config {
    char task_name[TASK_NAME_LEN];
    int memory_limit_mb;
    int cpu_share; // optional
};

#define IOCTL_ALLOCATE_TASK _IOW('k', 1, struct task_config)

void run_node_in_cgroup(const char *task_name, const char *script_path) {
    char cgroup_procs_path[256];
    snprintf(cgroup_procs_path, sizeof(cgroup_procs_path),
             "/sys/fs/cgroup/resgroup/%s/cgroup.procs", task_name);

    pid_t pid = fork();
    if (pid == 0) {
        // Child: launch Node.js process
        char pid_str[16];
        snprintf(pid_str, sizeof(pid_str), "%d", getpid());

        FILE *f = fopen(cgroup_procs_path, "w");
        if (!f) {
            perror("Failed to write to cgroup.procs");
            exit(1);
        }
        fprintf(f, "%s", pid_str);
        fclose(f);

        execlp("node", "node", script_path, NULL);
        perror("execlp failed");
        exit(1);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);  // Wait for Node.js to complete
    } else {
        perror("fork failed");
    }
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s <task_name> <mem_MB> <js_script>\n", argv[0]);
        return 1;
    }

    const char *task_name = argv[1];
    int mem_limit = atoi(argv[2]);
    const char *script_path = argv[3];

    int fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return 1;
    }

    struct task_config config = {0};
    strncpy(config.task_name, task_name, TASK_NAME_LEN - 1);
    config.memory_limit_mb = mem_limit;

    if (ioctl(fd, IOCTL_ALLOCATE_TASK, &config) < 0) {
        perror("ioctl failed");
        close(fd);
        return 1;
    }

    printf("Cgroup for task '%s' created with %dMB memory\n", task_name, mem_limit);
    run_node_in_cgroup(task_name, script_path);

    close(fd);
    return 0;
}
