#include "protocol.h"
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

protocol_status_t send_json(int sockfd, cJSON *json) {
    char *json_str = cJSON_PrintUnformatted(json);
    if (!json_str) return PROTOCOL_ERR;
    uint32_t len = strlen(json_str);
    uint32_t net_len = htonl(len);
    if (write(sockfd, &net_len, sizeof(net_len)) != sizeof(net_len)) {
        free(json_str);
        return PROTOCOL_ERR;
    }
    if (write(sockfd, json_str, len) != (ssize_t)len) {
        free(json_str);
        return PROTOCOL_ERR;
    }
    free(json_str);
    return PROTOCOL_OK;
}

protocol_status_t recv_json(int sockfd, cJSON **json_out) {
    uint32_t net_len;
    if (read(sockfd, &net_len, sizeof(net_len)) != sizeof(net_len)) return PROTOCOL_ERR;
    uint32_t len = ntohl(net_len);
    if (len == 0 || len > 65536) return PROTOCOL_ERR;
    char *buf = malloc(len + 1);
    if (!buf) return PROTOCOL_ERR;
    size_t recvd = 0;
    while (recvd < len) {
        ssize_t n = read(sockfd, buf + recvd, len - recvd);
        if (n <= 0) { free(buf); return PROTOCOL_ERR; }
        recvd += n;
    }
    buf[len] = '\0';
    *json_out = cJSON_Parse(buf);
    free(buf);
    return *json_out ? PROTOCOL_OK : PROTOCOL_ERR;
}

cJSON* create_task_metadata(const char *task_id, const char *chunk_filename, const char *sender_id, const char *receiver_id, const char *status) {
    cJSON *meta = cJSON_CreateObject();
    cJSON_AddStringToObject(meta, "type", "task");
    cJSON_AddStringToObject(meta, "version", "1");
    cJSON_AddStringToObject(meta, "task_id", task_id);
    cJSON_AddStringToObject(meta, "chunk_filename", chunk_filename);
    cJSON_AddStringToObject(meta, "sender_id", sender_id);
    cJSON_AddStringToObject(meta, "receiver_id", receiver_id);
    cJSON_AddStringToObject(meta, "status", status);
    return meta;
}

cJSON* create_result_metadata(const char *task_id, const char *chunk_filename, const char *sender_id, const char *receiver_id, const char *status) {
    cJSON *meta = cJSON_CreateObject();
    cJSON_AddStringToObject(meta, "type", "result");
    cJSON_AddStringToObject(meta, "version", "1");
    cJSON_AddStringToObject(meta, "task_id", task_id);
    cJSON_AddStringToObject(meta, "chunk_filename", chunk_filename);
    cJSON_AddStringToObject(meta, "sender_id", sender_id);
    cJSON_AddStringToObject(meta, "receiver_id", receiver_id);
    cJSON_AddStringToObject(meta, "status", status);
    return meta;
}
