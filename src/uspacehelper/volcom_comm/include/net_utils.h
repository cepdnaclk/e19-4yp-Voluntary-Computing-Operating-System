#ifndef NET_UTILS_H
#define NET_UTILS_H

void send_udp_broadcast(const char* message);
void get_local_ip(char* buffer, size_t size);
int start_tcp_server(int port);
int accept_tcp_connection(int server_fd);

#endif
