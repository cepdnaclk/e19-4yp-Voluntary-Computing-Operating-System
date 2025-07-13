#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include "send_result.h"

#define BUFFER_SIZE 4096

// Helper to find latest file with IP pattern
static char* find_latest_result_file(const char *directory, const char *ip) {
    DIR *dir = opendir(directory);
    if (!dir) {
        perror("[send_result] Failed to open result directory");
        return NULL;
    }

    struct dirent *entry;
    struct stat file_stat;
    char filepath[512];
    time_t latest_mtime = 0;
    char *latest_file = NULL;

    while ((entry = readdir(dir))) {
        if (entry->d_type != DT_REG) continue;

        if (!strstr(entry->d_name, ip)) continue;

        snprintf(filepath, sizeof(filepath), "%s/%s", directory, entry->d_name);

        if (stat(filepath, &file_stat) == 0) {
            if (file_stat.st_mtime > latest_mtime) {
                latest_mtime = file_stat.st_mtime;

                free(latest_file);
                latest_file = strdup(filepath);
            }
        }
    }

    closedir(dir);
    return latest_file;
}

void send_output_to_employer(int client_fd, const char *result_path, const char *employer_ip) {
    char *header = "RESULT\n";
    send(client_fd, header, strlen(header), 0);

    char *latest_result = find_latest_result_file(result_path, employer_ip);
    if (!latest_result) {
        printf("[send_result] No result file found for IP: %s\n", employer_ip);
        return;
    }

    FILE *fp = fopen(latest_result, "rb");
    if (!fp) {
        perror("[send_result] Failed to open result file");
        free(latest_result);
        return;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes;
    size_t total_sent = 0;

    printf("[send_result] Sending result file: %s\n", latest_result);

    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (send(client_fd, buffer, bytes, 0) != bytes) {
            perror("[send_result] Failed during send");
            break;
        }
        total_sent += bytes;
    }

    fclose(fp);
    printf("[send_result] Result sent (%zu bytes).\n", total_sent);
    free(latest_result);
}
