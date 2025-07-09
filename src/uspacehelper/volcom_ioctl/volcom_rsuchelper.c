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
    int cpu_share;
    pid_t pid; // Required for kernel to assign to cgroup
};


#define IOCTL_ALLOCATE_TASK _IOW('k', 1, struct task_config)

void run_node_in_cgroup(int fd, const char *task_name, const char *script_path, int mem_limit, int cpu_share) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child will be assigned to cgroup by parent, then exec Node.js
        pause();  // Wait until parent puts us in the cgroup
        execlp("node", "node", script_path, NULL);
        perror("execlp failed");
        exit(1);
    } else if (pid > 0) {
        struct task_config config = {0};
        strncpy(config.task_name, task_name, TASK_NAME_LEN - 1);
        config.memory_limit_mb = mem_limit;
        config.cpu_share = cpu_share;
        config.pid = pid;

        if (ioctl(fd, IOCTL_ALLOCATE_TASK, &config) < 0) {
            perror("ioctl failed");
            kill(pid, SIGKILL);
            return;
        }

        printf("Cgroup for task '%s' created, PID %d assigned\n", task_name, pid);
        kill(pid, SIGCONT); // Resume child after cgroup assignment
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
    int cpu_share = 20000; // 20%, for example

    int fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return 1;
    }

    run_node_in_cgroup(fd, task_name, script_path, mem_limit, cpu_share);

    close(fd);
    return 0;
}
