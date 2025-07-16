#ifndef PROTOCOL_H
#define PROTOCOL_H
#include <stdint.h>
#include <stddef.h>
#include <cjson/cJSON.h>

// Protocol version
#define PROTOCOL_VERSION 1

// Send a JSON object with length prefix
typedef enum { PROTOCOL_OK = 0, PROTOCOL_ERR = -1 } protocol_status_t;

protocol_status_t send_json(int sockfd, cJSON *json);
protocol_status_t recv_json(int sockfd, cJSON **json_out);

// Utility: create task/result metadata
cJSON* create_task_metadata(const char *task_id, const char *chunk_filename, const char *sender_id, const char *receiver_id, const char *status);
cJSON* create_result_metadata(const char *task_id, const char *chunk_filename, const char *sender_id, const char *receiver_id, const char *status);

#endif // PROTOCOL_H
