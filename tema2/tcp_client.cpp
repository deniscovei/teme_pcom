#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

#include "common.h"

using namespace std;

void print_udp_packet(meta_udp_packet &received_packet) {
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(received_packet.ip), ip_str, INET_ADDRSTRLEN);
    string message = ip_str + (string)":" + to_string(ntohs(received_packet.port)) + " - ";

    char topic[TOPIC_MAXSIZE + 1];
    memcpy(topic, received_packet.topic, TOPIC_MAXSIZE);
    topic[TOPIC_MAXSIZE] = '\0';
    message += (string)topic + " - ";

    if (received_packet.data_type == INT) {
        uint8_t sign = received_packet.data[0];
        uint32_t value = ntohl(*(uint32_t *)(received_packet.data + 1));

        message += "INT - " + (string)(sign == 1 && value != 0 ? "-" : "") + to_string(value);
    } else if (received_packet.data_type == SHORT_REAL) {
        uint16_t value = ntohs(*(uint16_t *)received_packet.data);
        string res = to_string(value);
        res.insert(res.size() - 2, ".");

        message += "SHORT_REAL - " + res;
    } else if (received_packet.data_type == FLOAT) {
        uint8_t sign = received_packet.data[0];
        uint32_t value = ntohl(*(uint32_t *)(received_packet.data + 1));
        uint8_t power = received_packet.data[5];

        string res = to_string(value);

        if (res.size() <= power) {
            for (int i = 0; i <= power - res.size() + 1; i++) {
                res.insert(0, "0");
            }
        }

        if (power != 0) {
            res.insert(res.size() - power, ".");
        }

        message += "FLOAT - " + (string)(sign == 1 ? "-" : "") + res;
    } else if (received_packet.data_type == STRING) {
        message += "STRING - " + string(received_packet.data);
    }

    cout << message << '\n';
}

void handle_server_messages(int sockfd) {
    uint16_t len;

    if (recv_all(sockfd, &len, sizeof(len), 0) < 0) {
        perror("recv_all");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    len = ntohs(len);

    meta_udp_packet recv_packet;
    memset(&recv_packet, 0, sizeof(meta_udp_packet));

    int rc = recv_all(sockfd, &recv_packet, sizeof(meta_udp_packet) - UDP_MAXSIZE + len, 0);
    if (rc < 0) {
        close(sockfd);
        perror("recv_all");
        exit(EXIT_FAILURE);
    }

    if (rc == 0) {
        close(sockfd);
        exit(EXIT_SUCCESS);
    } else {
        print_udp_packet(recv_packet);
    }
}

void handle_commands(int sockfd, string subscriber_id) {
    char buff[IN_BUFFER_SIZE];
    memset(buff, 0, IN_BUFFER_SIZE);

    if (fgets(buff, IN_BUFFER_SIZE, stdin) && !isspace(buff[0])) {
        buff[strcspn(buff, "\n")] = 0;

        uint16_t len = strlen(buff) + 1;
        uint16_t len_n = htons(len);

        buff[len - 1] = '\0';

        if (send_all(sockfd, &len_n, sizeof(uint16_t), 0) < 0) {
            perror("send_all");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        if (send_all(sockfd, &buff, len, 0) < 0) {
            perror("send_all");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        if (strncmp(buff, "subscribe", 9) == 0) {
            cout << "Subscribed to topic " << buff + 10 << '\n';
            return;
        }
        
        if (strncmp(buff, "unsubscribe", 11) == 0) {
            cout << "Unsubscribed from topic " << buff + 12 << '\n';
            return;
        }
        
        if (strncmp(buff, "exit", 4) == 0) {
            close(sockfd);
            exit(EXIT_SUCCESS);
        }
    }
}

void run_client(int sockfd, string subscriber_id) {
    pollfd fds[2];

    fds[0].fd = sockfd;
    fds[0].events = POLLIN;

    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;

    while (1) {
        if (poll(fds, 2, -1) < 0) {
            perror("poll");
            return;
        }

        if (fds[0].revents & POLLIN) {
            handle_server_messages(sockfd);
        }

        if (fds[1].revents & POLLIN) {
            handle_commands(sockfd, subscriber_id);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        cout << "\n Usage: " << argv[0] << " <ID_CLIENT> <IP_SERVER> <PORT_SERVER>" << '\n';
        return 1;
    }

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    uint16_t port;
    int rc = sscanf(argv[3], "%hu", &port);
    if (rc != 1) {
        cerr << "Invalid port\n";
        return 1;
    }

    const int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt");
        return 1;
    }

    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0) {
        close(sockfd);
        perror("setsockopt(TCP_NODELAY) failed");
        return 1;
    }

    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(sockaddr_in);

    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr) < 0) {
        close(sockfd);
        perror("inet_pton");
        return 1;
    }

    if (connect(sockfd, (sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sockfd);
        perror("connect");
        return 1;
    }

    char subscriber_id[ID_MAXSIZE] = {0};
    strcpy(subscriber_id, argv[1]);

    if (send_all(sockfd, subscriber_id, ID_MAXSIZE, 0) < 0) {
        close(sockfd);
        perror("send_all");
        return 1;
    }

    run_client(sockfd, (string)subscriber_id);

    close(sockfd);

    return 0;
}
