#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/socket.h>
#include "send_result.h"


#define BUFFER_SIZE 4096

// // Extracts IP from file name like result_1752345678_192_168_1_10_matrix_calc.bin
// static int extract_ip_from_filename(const char *filename, char *ip_out, size_t ip_out_size) {
//     const char *prefix = "result_";
//     const char *p = strstr(filename, prefix);
//     if (!p) return 0;

//     p += strlen(prefix);  // Skip "result_"
//     while (*p && *p >= '0' && *p <= '9') p++;  // Skip timestamp and underscore

//     if (*p != '_') return 0;
//     p++;  // Move past underscore

//     // Copy digits and replace _ with . up to 4 segments (3 underscores)
//     int segments = 0;
//     size_t i = 0;
//     while (*p && i < ip_out_size - 1) {
//         if (*p == '_') {
//             ip_out[i++] = '.';
//             segments++;
//             if (segments > 3) break;
//         } else if (*p >= '0' && *p <= '9') {
//             ip_out[i++] = *p;
//         } else {
//             break;
//         }
//         p++;
//     }
//     ip_out[i] = '\0';

//     return (segments == 3);  // Should have 3 dots = 4 segments
// }

void send_output_to_employer(int client_fd, const char *result_file_path) {

    // Open file
    FILE *fp = fopen(result_file_path, "rb");
    if (!fp) {
        perror("[send_result] Failed to open result file");
        return;
    }

    // Step 1: Send marker
    const char *header = "RESULT\n";
    send(client_fd, header, strlen(header), 0);

    // Step 2: Send actual file
    char buffer[BUFFER_SIZE];
    size_t total_sent = 0;
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if ((size_t)send(client_fd, buffer, bytes, 0) != (size_t)bytes) {
            perror("[send_result] Failed to send result file");
            break;
        }
        total_sent += bytes;
    }

    fclose(fp);
    printf("[send_result] Result file sent (%zu bytes): %s\n", total_sent, result_file_path);
}
