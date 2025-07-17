#ifndef VOLOCOM_NET_H
#define VOLOCOM_NET_H

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Define broadcast port and TCP port
#define BROADCAST_PORT 9876
#define TCP_PORT 12345

struct udp_config_s{
    int port;
    const char *ip;
    int interval_sec;
};

bool udp_broadcaster_init(struct udp_config_s *config);
bool send_udp_broadcast(const char* message);
void udp_broadcaster_cleanup(void);

void get_local_ip(char* buffer, size_t size);
int start_tcp_server(int port);
int accept_tcp_connection(int server_fd);

#endif // VOLOCOM_NET_H