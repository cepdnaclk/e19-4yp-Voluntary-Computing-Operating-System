#include "volcom_net.h"

static int sockfd = -1;
static struct sockaddr_in broadcast_addr;

bool udp_broadcaster_init(struct udp_config_s *config){

    if (!config) {
        fprintf(stderr, "Invalid UDP config\n");
        return false;
    }

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        return false;
    }

    int broadcast_enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable))) {
        perror("setsockopt (SO_BROADCAST) failed");
        close(sockfd);
        sockfd = -1;
        return false;
    }

    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(config->port);
    
     if (inet_pton(AF_INET, config->ip, &broadcast_addr.sin_addr) <= 0) {
        perror("inet_pton failed");
        close(sockfd);
        sockfd = -1;
        return false;
    }

    printf("UDP broadcaster initialized on port %d, IP %s\n", config->port, config->ip);
    return true;
}

bool send_udp_broadcast(const char* message) {
    
    if (sockfd < 0) {
        fprintf(stderr, "Broadcaster not initialized\n");
        return false;
    }

    ssize_t sent_bytes = sendto(sockfd, message, strlen(message), 0,
                              (const struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));
    
    
    if (sent_bytes < 0) {
        perror("sendto failed");
        return false;
    }
    
    return true;
}

void udp_broadcaster_cleanup() {
    if (sockfd >= 0) {
        close(sockfd);
        sockfd = -1;
    }
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