#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <libgen.h> // for basename()
#include "send_task.h"

#define EMPLOYEE_PORT 12345
#define BUFFER_SIZE 4096

// Validate file path
int file_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

// Sanitize IP address for filename (e.g., 192.168.1.2 -> 192_168_1_2)
void sanitize_ip(const char *ip, char *output) {
    for (int i = 0; ip[i]; ++i) {
        output[i] = (ip[i] == '.') ? '_' : ip[i];
    }
    output[strlen(ip)] = '\0';
}

int send_file(int sockfd, const char *filepath, const char *ip) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        perror("[send_task] Failed to open file");
        return 0;
    }

    char filename[256];
    char *base = basename((char *)filepath);
    strncpy(filename, base, sizeof(filename));
    filename[sizeof(filename) - 1] = '\0';

    // Send filename to employee
    send(sockfd, filename, strlen(filename) + 1, 0);  // +1 to send null terminator
    printf("[send_task] Sent filename to employee: %s\n", filename);

    // Construct task filename for log/reference
    char sanitized_ip[64];
    sanitize_ip(ip, sanitized_ip);

    char task_filename[512];
    snprintf(task_filename, sizeof(task_filename), "task_%ld_%s_%s", time(NULL), sanitized_ip, filename);
    printf("[send_task] Task filename (internal): %s\n", task_filename);

    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    printf("[send_task] Sending file contents...\n");

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        if (send(sockfd, buffer, bytes_read, 0) != bytes_read) {
            perror("[send_task] Failed to send file");
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);

    // Notify EOF for file but keep socket open for result
    if (shutdown(sockfd, SHUT_WR) < 0) {
        perror("[send_task] Failed to shutdown write stream");
    }

    printf("[send_task] File sent successfully.\n");
    return 1;
}

int receive_task_result(int sockfd, const char *ip, const char *original_filename);

int send_task_to_employee(const char *ip) {
    struct sockaddr_in addr;
    int sockfd;
    char response[32] = {0};

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("[send_task] Socket creation failed");
        return 0;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(EMPLOYEE_PORT);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        perror("[send_task] Invalid IP address");
        close(sockfd);
        return 0;
    }

    printf("[send_task] Connecting to %s...\n", ip);
    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[send_task] Connection failed");
        close(sockfd);
        return 0;
    }

    // Wait for accept/reject
    ssize_t n = recv(sockfd, response, sizeof(response) - 1, 0);
    if (n <= 0) {
        printf("[send_task] No response from employee.\n");
        close(sockfd);
        return 0;
    }

    response[n] = '\0';
    if (strcmp(response, "REJECT") == 0) {
        printf("[send_task] Employee rejected the task.\n");
        close(sockfd);
        return 0;
    }

    printf("[send_task] Employee accepted. Please provide the file path to send.\n");

    // Prompt for file path
    char filepath[512];
    while (1) {
        printf("Enter path to file: ");
        if (!fgets(filepath, sizeof(filepath), stdin)) continue;
        filepath[strcspn(filepath, "\n")] = 0;  // strip newline

        if (file_exists(filepath)) break;
        printf("[send_task] File does not exist or is not a regular file. Try again.\n");
    }

    // Extract original filename
    char *original_filename = basename(filepath);

    int result = send_file(sockfd, filepath, ip);
    if (result) {
        printf("[send_task] Waiting for task result from employee...\n");
        int recv_success = receive_task_result(sockfd, ip, original_filename);
        close(sockfd);
        return recv_success;
    }

    close(sockfd);
    return 0;
}

static void ensure_directory_exists(const char *dir) {
    struct stat st = {0};
    if (stat(dir, &st) == -1) {
        mkdir(dir, 0755);
    }
}

int receive_task_result(int sockfd, const char *ip, const char *original_filename) {
    ensure_directory_exists("received_outputs");

    char sanitized_ip[64];
    sanitize_ip(ip, sanitized_ip);

    char result_filename[512];
    snprintf(result_filename, sizeof(result_filename),
             "received_outputs/result_%ld_%s_%s", time(NULL), sanitized_ip, original_filename);

    FILE *fp = fopen(result_filename, "wb");
    if (!fp) {
        perror("[send_task] Failed to create result file");
        return 0;
    }

    char buffer[BUFFER_SIZE];
    ssize_t len;
    size_t total = 0;
    const char *marker = "RESULT\n";
    size_t marker_len = strlen(marker);

    printf("[send_task] Waiting for result marker from employee (max 60s)...\n");

    time_t start_time = time(NULL);
    char tempbuf[512] = {0};
    size_t collected = 0;

    while (time(NULL) - start_time < 60) {
        ssize_t n = recv(sockfd, &tempbuf[collected], sizeof(tempbuf) - collected - 1, MSG_DONTWAIT);
        if (n > 0) {
            collected += n;
            tempbuf[collected] = '\0';

            char *found = strstr(tempbuf, marker);
            if (found) {
                printf("[send_task] Result marker received.\n");

                size_t offset = (found - tempbuf) + marker_len;
                size_t remaining = collected - offset;
                if (remaining > 0) {
                    fwrite(found + marker_len, 1, remaining, fp);
                    total += remaining;
                }
                break;
            }

            if (collected > sizeof(tempbuf) - 128) {
                memmove(tempbuf, tempbuf + 256, collected - 256);
                collected -= 256;
            }
        } else {
            usleep(200 * 1000);
        }
    }

    if (time(NULL) - start_time >= 60) {
        printf("[send_task] Timed out waiting for result marker.\n");
        fclose(fp);
        return 0;
    }

    while ((len = recv(sockfd, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, len, fp);
        total += len;
    }

    fclose(fp);
    printf("[send_task] Result received: %zu bytes → saved to %s\n", total, result_filename);
    return (total > 0);
}
