#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>


#define BROADCAST_PORT 9876
#define TCP_PORT 12345

void send_udp_broadcast(const char* message) {
    int sockfd;
    struct sockaddr_in addr;
    int broadcast = 1;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket failed");
        return;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        perror("setsockopt failed");
        close(sockfd);
        return;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(BROADCAST_PORT);
    addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    sendto(sockfd, message, strlen(message), 0, (struct sockaddr*)&addr, sizeof(addr));
    close(sockfd);
}

void get_local_ip(char* buffer, size_t size) {
    struct ifaddrs *ifaddr, *ifa;
    getifaddrs(&ifaddr);

    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr &&
            ifa->ifa_addr->sa_family == AF_INET &&
            !(ifa->ifa_flags & IFF_LOOPBACK)) {
            getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), buffer, size, NULL, 0, NI_NUMERICHOST);
            break;
        }
    }

    freeifaddrs(ifaddr);
}

int start_tcp_server(int port) {
    int sockfd;
    struct sockaddr_in addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("TCP socket failed");
        return -1;
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("TCP bind failed");
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, 5) < 0) {
        perror("TCP listen failed");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int accept_tcp_connection(int server_fd) {
    struct sockaddr_in client;
    socklen_t len = sizeof(client);
    return accept(server_fd, (struct sockaddr*)&client, &len);
}
