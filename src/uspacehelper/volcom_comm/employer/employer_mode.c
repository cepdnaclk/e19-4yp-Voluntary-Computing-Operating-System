#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <time.h>
#include "cJSON.h"
#include "employee_list.h"
#include "employer_mode.h"
#include "select_employee.h"
#include "send_task.h"

// Forward declaration for new send function
int send_task_to_employee_with_file(const char *ip, const char *filepath);
#include <dirent.h>
#include <sys/types.h>

#define CHUNKED_SET_PATH "/home/dasun/Projects/e19-4yp-Voluntary-Computing-Operating-System/src/uspacehelper/scripts"
#define MAX_CHUNKS 1024
#define MAX_FILENAME_LEN 256
#include <dirent.h>
#include <sys/types.h>

#define PORT 9876
#define BUFFER_SIZE 2048
#define COLLECTION_TIMEOUT 5       // Time to wait after first employee is found
#define STALE_THRESHOLD 15






char chunk_files[MAX_CHUNKS][MAX_FILENAME_LEN];
int num_chunks = 0;

void scan_chunked_set() {
    DIR *dir;
    struct dirent *entry;

    num_chunks = 0;
    dir = opendir(CHUNKED_SET_PATH);
    if (!dir) {
        perror("[Employer] Failed to open chuncked_set directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL && num_chunks < MAX_CHUNKS) {
        if (entry->d_type == DT_REG) { // Only regular files
            strncpy(chunk_files[num_chunks], entry->d_name, MAX_FILENAME_LEN - 1);
            chunk_files[num_chunks][MAX_FILENAME_LEN - 1] = '\0';
            num_chunks++;
        }
    }
    closedir(dir);

    printf("[Employer] Found %d chunk files in chuncked_set.\n", num_chunks);
}

void run_employer_mode() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // Scan the chunked_set directory for chunk files
    scan_chunked_set();

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("[Employer] Listening for employee broadcasts on port %d...\n", PORT);

    time_t first_found_time = 0;

    while (1) {
        ssize_t len = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0,
                               (struct sockaddr *) &client_addr, &addr_len);
        if (len < 0) {
            perror("recvfrom");
            continue;
        }

        buffer[len] = '\0';

        cJSON *json = cJSON_Parse(buffer);
        if (!json) continue;

        cJSON *ip = cJSON_GetObjectItem(json, "ip");
        cJSON *free_mem_mb = cJSON_GetObjectItem(json, "free_mem_mb");
        cJSON *mem_usage = cJSON_GetObjectItem(json, "mem_usage_percent");
        cJSON *cpu_usage = cJSON_GetObjectItem(json, "cpu_usage_percent");
        cJSON *cpu_model = cJSON_GetObjectItem(json, "cpu_model");
        cJSON *logical_cores = cJSON_GetObjectItem(json, "logical_cores");

        if (cJSON_IsString(ip) && cJSON_IsNumber(free_mem_mb) &&
            cJSON_IsNumber(mem_usage) && cJSON_IsNumber(cpu_usage) &&
            cJSON_IsString(cpu_model) && cJSON_IsNumber(logical_cores)) {

            EmployeeNode node;
            memset(&node, 0, sizeof(EmployeeNode));
            strncpy(node.ip, ip->valuestring, sizeof(node.ip) - 1);
            node.free_mem_mb = free_mem_mb->valuedouble;
            node.mem_usage_percent = mem_usage->valuedouble;
            node.cpu_usage_percent = cpu_usage->valuedouble;
            strncpy(node.cpu_model, cpu_model->valuestring, sizeof(node.cpu_model) - 1);
            node.logical_cores = logical_cores->valueint;
            node.last_seen = time(NULL);

            add_or_update_employee(node);

            if (first_found_time == 0) {
                first_found_time = time(NULL);
                printf("\n[+] First employee discovered. Collecting for %d seconds...\n", COLLECTION_TIMEOUT);
            }
        }

        cJSON_Delete(json);

        if (first_found_time != 0 && time(NULL) - first_found_time >= COLLECTION_TIMEOUT) {
            break; // stop collecting
        }
    }

    close(sockfd);

    printf("\n[Employer] Finalizing employee list...\n");
    cleanup_old_employees(STALE_THRESHOLD);

    // The chunk_files[] array now contains all chunk files in chuncked_set
    // Assign these to employees in round robin fashion

    EmployeeNodeWrapper *list = get_candidate_list();
    if (!list) {
        printf("[!] No active employees found.\n");
        return;
    }

    // Count employees
    int employee_count = 0;
    EmployeeNodeWrapper *cur = list;
    while (cur) {
        employee_count++;
        cur = cur->next;
    }
    if (employee_count == 0) {
        printf("[!] No employees available for task assignment.\n");
        return;
    }

    // Build array of employee IPs
    char **employee_ips = malloc(employee_count * sizeof(char*));
    cur = list;
    int idx = 0;

    while (cur) {
        employee_ips[idx] = strdup(cur->data.ip); // Use correct struct member from EmployeeNodeWrapper
        idx++;
        cur = cur->next;
    }

    printf("[Employer] Assigning %d chunk files to %d employees (round robin)\n", num_chunks, employee_count);

    for (int i = 0; i < num_chunks; ++i) {
        int emp_idx = i % employee_count;
        char *emp_ip = employee_ips[emp_idx];

        // Build full path to chunk file
        char chunk_path[512];
        snprintf(chunk_path, sizeof(chunk_path), "%s/%s", CHUNKED_SET_PATH, chunk_files[i]);

        printf("[Employer] Sending chunk %s to employee %s\n", chunk_files[i], emp_ip);

        int success = send_task_to_employee_with_file(emp_ip, chunk_path);
        if (success) {
            printf("[✔] Chunk %s sent and result received from %s.\n", chunk_files[i], emp_ip);
        } else {
            printf("[!] Failed to send chunk %s to %s.\n", chunk_files[i], emp_ip);
        }
    }

    // Free employee IPs
    for (int i = 0; i < employee_count; ++i) {
        free(employee_ips[i]);
    }
    free(employee_ips);
}

//store sent tasks and delete them when result is received