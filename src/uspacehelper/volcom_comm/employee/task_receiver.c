#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <time.h>
#include "task_receiver.h"

#define BUFFER_SIZE 4096
#define SAVE_DIR "received_tasks"

void ensure_directory_exists(const char *dir) {
    struct stat st = {0};
    if (stat(dir, &st) == -1) mkdir(dir, 0755);
}

static void sanitize_ip(const char *ip, char *out, size_t size) {
    snprintf(out, size, "%s", ip);
    for (char *p = out; *p; p++) if (*p == '.') *p = '_';
}

char* handle_task_receive(int client_fd, const char *employer_ip) {
    ensure_directory_exists(SAVE_DIR);

    char original_filename[256] = "file.bin"; // Default fallback
    char header[512];
    recv(client_fd, header, sizeof(header) - 1, 0);

    if (strncmp(header, "FILENAME:", 9) == 0) {
        strncpy(original_filename, header + 9, sizeof(original_filename));
        original_filename[strcspn(original_filename, "\n")] = 0;
    }

    char *filepath = malloc(512);
    char ip_sanitized[64];
    sanitize_ip(employer_ip, ip_sanitized, sizeof(ip_sanitized));
    snprintf(filepath, 512, "%s/task_%ld_%s_%s", SAVE_DIR, time(NULL), ip_sanitized, original_filename);

    FILE *fp = fopen(filepath, "wb");
    if (!fp) return NULL;

    char buffer[BUFFER_SIZE];
    ssize_t n;
    while ((n = recv(client_fd, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, n, fp);
    }
    fclose(fp);

    return filepath;
}