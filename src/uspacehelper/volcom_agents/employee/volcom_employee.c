#include "volcom_employee.h"
#include "../../volcom_sysinfo/volcom_sysinfo.h"
#include "../../volcom_net/volcom_net.h"

static pthread_t broadcaster_thread;
static pthread_t worker_thread;
static struct udp_config_s udp_config;
static struct tcp_config_s tcp_config;

void broadcast_loop() {

    printf("[Employee] Starting broadcast loop...\n");
    char local_ip[64] = "Unknown";
    get_local_ip(local_ip, sizeof(local_ip));

    while (1) {
       
        struct system_info_s sys_info = get_system_info();

        // TODO: implement broadcasting
        get_local_ip(local_ip, sizeof(local_ip));
        char message[512];
        snprintf(message, sizeof(message),"");
    }
}

void run_employee_mode(){

    printf("[Employee Mode] Starting...\n");

    udp_config.port = BROADCAST_PORT;
    udp_config.ip = "255.255.255.255";
    udp_config.interval_sec = 60;

    // TODO: implement TCP client configuration
    // tcp_config.port = TCP_PORT;
    // tcp_config.ip = "127.0.0.1";
    // tcp_config.max_connections = 5;
    // tcp_config.timeout_sec = 10;

    if (!udp_broadcaster_init(&udp_config)) {
        fprintf(stderr, "[Employee Mode] Failed to initialize UDP broadcaster\n");
        return;
    }

    if (pthread_create(&broadcaster_thread, NULL, broadcast_loop, NULL) != 0) {
        perror("Failed to create broadcaster thread");
        return;
    }

    if (pthread_create(&worker_thread, NULL, worker_loop, NULL) != 0) {
        perror("Failed to create worker thread");
        return;
    }

}