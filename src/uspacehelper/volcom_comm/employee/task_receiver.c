#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include "task_receiver.h"

#define BUFFER_SIZE 4096
#define SAVE_DIR "received_tasks"

static void ensure_directory_exists(const char *dir) {
    struct stat st = {0};
    if (stat(dir, &st) == -1) {
        mkdir(dir, 0755);
    }
}

// Returns a malloc'ed string with file path, or NULL on failure
char* handle_task_receive(int client_fd) {
    ensure_directory_exists(SAVE_DIR);

    char *filepath = malloc(256);
    if (!filepath) return NULL;

    snprintf(filepath, 256, "%s/task_%ld.bin", SAVE_DIR, time(NULL));

    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        perror("[task_receiver] Failed to create file");
        free(filepath);
        return NULL;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    size_t total = 0;

    printf("[task_receiver] Waiting for file data...\n");

    while ((bytes_received = recv(client_fd, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, bytes_received, fp);
        total += bytes_received;
    }

    fclose(fp);

    if (total == 0) {
        printf("[task_receiver] Received empty file or connection closed.\n");
        free(filepath);
        return NULL;
    }

    printf("[task_receiver] File received. Bytes: %zu, saved to: %s\n", total, filepath);
    return filepath;
}
