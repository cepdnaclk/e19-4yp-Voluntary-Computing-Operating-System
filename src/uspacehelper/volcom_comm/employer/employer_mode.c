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

#define PORT 9876
#define BUFFER_SIZE 2048
#define COLLECTION_TIMEOUT 5       // Time to wait after first employee is found
#define STALE_THRESHOLD 15

static char selected_ip[64] = "";

const char* get_selected_employee_ip() {
    return selected_ip;
}

void run_employer_mode() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

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

while (1) {
    EmployeeNodeWrapper *list = get_candidate_list();
    if (!list) {
        printf("[!] No active employees found.\n");
        return;
    }

    char *chosen_ip = select_employee_ip(list);
    if (!chosen_ip) {
        printf("[!] No valid employee selected. Exiting.\n");
        return;
    }

    printf("[Employer] Attempting to send task to %s...\n", chosen_ip);
    
    int success = send_task_to_employee(chosen_ip);
    free(chosen_ip);

    if (success) {
        printf("[✔] Task executed and result received successfully.\n");
        break;  // Done
    } else {
        printf("\n[!] Task failed. Either employee rejected, connection failed, or no result was received in time.\n");
        printf("[!] You may try offloading the task to another available employee.\n");
        sleep(1);  // small delay for usability
        continue;  // Retry loop
    }
}

}


//store sent tasks and delete them when result is received