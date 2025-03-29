#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "common.h"

using namespace std;

void release_resources(pollfd *fds, const int &tcpfd, const int &udpfd) {
    delete fds;
    close(tcpfd);
    close(udpfd);
    for (auto &subscriber : subscribers) {
        close(subscriber.first);
    }
}

uint16_t get_meta_udp_packet_len(meta_udp_packet &packet) {
    if (packet.data_type == INT) {
        return 5;
    }

    if (packet.data_type == SHORT_REAL) {
        return 2;
    }

    if (packet.data_type == FLOAT) {
        return 6;
    }

    if (packet.data_type == STRING) {
        for (int i = 0; i < UDP_MAXSIZE; i++) {
            if (packet.data[i] == '\0') {
                return i;
            }
        }

        return UDP_MAXSIZE;
    }

    return 0;
}

void handle_new_subscriber_request(pollfd fds[], int &poll_size, int &nr_clients, const int &tcpfd, const int &udpfd) {
    sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    const int subscriberfd = accept(tcpfd, (sockaddr *)&cli_addr, &cli_len);

    if (subscriberfd < 0) {
        perror("accept");
        release_resources(fds, tcpfd, udpfd);
        exit(EXIT_FAILURE);
    }

    if (nr_clients == poll_size) {
        poll_size *= 2;
        fds = (pollfd *)realloc(fds, poll_size * sizeof(pollfd));
    }

    fds[nr_clients].fd = subscriberfd;
    fds[nr_clients].events = POLLIN;
    nr_clients++;

    int enable = 1;
    if (setsockopt(subscriberfd, IPPROTO_TCP, TCP_NODELAY, (char *)&enable, sizeof(int)) < 0) {
        perror("setsockopt(TCP_NODELAY) failed");
        release_resources(fds, tcpfd, udpfd);
        exit(EXIT_FAILURE);
    }

    char subscriber_id[ID_MAXSIZE] = {0};
    if (recv_all(subscriberfd, &subscriber_id, ID_MAXSIZE, 0) < 0) {
        release_resources(fds, tcpfd, udpfd);
        perror("recv_all");
        exit(EXIT_FAILURE);
    }

    string id = string(subscriber_id);
    subscribers[subscriberfd] = subscriber_t(subscriberfd, id, cli_addr.sin_addr.s_addr, ntohs(cli_addr.sin_port));

    if (online_subscribers.contains(id)) {
        cout << "Client " << subscriber_id << " already connected." << '\n';
        close(subscriberfd);
        nr_clients--;
        return;
    }

    // New client <ID_CLIENT> connected from IP:PORT.
    cout << "New client " << subscriber_id << " connected from " << inet_ntoa(cli_addr.sin_addr) << ":" << ntohs(cli_addr.sin_port) << "." << '\n';

    online_subscribers.insert(id);
}

void handle_server_input(pollfd *fds, const int &tcpfd, const int &udpfd) {
    char buffer[IN_BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    if (fgets(buffer, IN_BUFFER_SIZE, stdin) == NULL) {
        perror("fgets");
        release_resources(fds, tcpfd, udpfd);
        exit(EXIT_FAILURE);
    }

    if (strncmp(buffer, "exit", 4) == 0) {
        release_resources(fds, tcpfd, udpfd);
        exit(EXIT_SUCCESS);
    }

    cerr << "Unknown command" << '\n';
}

vector<string> tokenize(const string &str, const char &delim) {
    vector<string> tokens;
    stringstream ss(str);
    string token;

    while (getline(ss, token, delim)) {
        tokens.push_back(token);
    }

    return tokens;
}

bool topic_match(const string &topic_pattern, const string &target_topic) {
    if (topic_pattern == "*" || topic_pattern == target_topic) {
        return true;
    }

    string token;

    vector<string> pattern_tokens = tokenize(topic_pattern, '/');
    vector<string> target_tokens = tokenize(target_topic, '/');

    vector<vector<bool>> dp(target_tokens.size() + 1, vector<bool>(pattern_tokens.size() + 1));
    dp[target_tokens.size()][pattern_tokens.size()] = true;

    for (int i = target_tokens.size(); i >= 0; i--) {
        for (int j = pattern_tokens.size() - 1; j >= 0; j--) {
            if (j < pattern_tokens.size() && pattern_tokens[j] == "*") {
                dp[i][j] = dp[i][j + 1] || (i < target_tokens.size() && dp[i + 1][j]);
            } else {
                bool first_match = (i < target_tokens.size() && (pattern_tokens[j] == target_tokens[i] || pattern_tokens[j] == "+"));
                dp[i][j] = first_match && dp[i + 1][j + 1];
            }
        }
    }

    return dp[0][0];
}

void handle_udp_client_request(pollfd *fds, int tcpfd, int udpfd) {
    udp_packet received_packet;
    sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);

    memset(&received_packet, 0, sizeof(received_packet));
    if (recvfrom(udpfd, (char *)(&received_packet), sizeof(udp_packet), 0, (sockaddr *)&cli_addr, &cli_len) < 0) {
        perror("recvfrom");
        release_resources(fds, tcpfd, udpfd);
        exit(EXIT_FAILURE);
    }

    meta_udp_packet meta_packet;
    meta_packet.ip = cli_addr.sin_addr.s_addr;
    meta_packet.port = ntohs(cli_addr.sin_port);
    memcpy(&meta_packet.topic, received_packet.topic, TOPIC_MAXSIZE);
    memcpy(&meta_packet.data_type, &received_packet.data_type, sizeof(uint8_t));
    memcpy(&meta_packet.data, received_packet.data, UDP_MAXSIZE);

    char topic[TOPIC_MAXSIZE + 1];
    memcpy(topic, received_packet.topic, TOPIC_MAXSIZE);
    topic[TOPIC_MAXSIZE] = '\0';

    uint16_t len = get_meta_udp_packet_len(meta_packet);
    uint16_t nth_ord_len = htons(len);

    string received_topic = string(received_packet.topic);

    for (auto &[subscriber, subscriber_topics] : subscriber_topics) {
        for (auto &subscriber_topic : subscriber_topics) {
            if (topic_match(subscriber_topic, received_topic)) {
                if (online_subscribers.contains(subscriber.id)) {
                    if (send_all(subscriber.sockfd, &nth_ord_len, sizeof(uint16_t), 0) < 0) {
                        perror("send_all");
                        release_resources(fds, tcpfd, udpfd);
                        return;
                    }

                    if (send_all(subscriber.sockfd, &meta_packet, sizeof(meta_udp_packet) - UDP_MAXSIZE + len, 0) < 0) {
                        perror("send_all");
                        release_resources(fds, tcpfd, udpfd);
                        return;
                    }
                }

                break;
            }
        }
    }
}

void handle_subscriber(pollfd *fds, int &nr_clients, int n, const int &tcpfd, const int &udpfd) {
    int subscriberfd = fds[n].fd;
    uint16_t len;

    if (recv_all(subscriberfd, &len, sizeof(uint16_t), 0) < 0) {
        perror("recv_all");
        release_resources(fds, tcpfd, udpfd);
        exit(EXIT_FAILURE);
    }

    len = ntohs(len);

    char buff[MSG_MAXSIZE] = {0};

    int rc = recv_all(subscriberfd, buff, len, 0);
    subscriber_t subscriber = subscribers[subscriberfd];

    // Close connection
    if (rc <= 0 || strncmp(buff, "exit", 4) == 0) {
        cout << "Client " << subscriber.id << " disconnected." << '\n';
        online_subscribers.erase(subscriber.id);
        close(subscriberfd);

        fds[n] = fds[nr_clients - 1];
        memset(&fds[nr_clients - 1], 0, sizeof(pollfd));
        nr_clients--;
        return;
    }

    // Subscribe client to a topic
    if (strncmp(buff, "subscribe", 9) == 0) {
        string topic = string(buff + 10);
        subscriber_topics[subscriber].insert(topic);
        return;
    }

    // Unsubscribe client from a topic
    if (strncmp(buff, "unsubscribe", 11) == 0) {
        string topic = string(buff + 12);
        subscriber_topics[subscriber].erase(topic);
        return;
    }
}

void run_server(const int &tcpfd, const int &udpfd) {
    int clientfd;
    int nr_clients = 3;
    int poll_size = 3;

    pollfd *fds = (pollfd *)malloc(poll_size * sizeof(pollfd));

    // Add the tcpfd to the pollfd array
    fds[0].fd = tcpfd;
    fds[0].events = POLLIN;

    if (listen(tcpfd, MAX_CONNECTIONS) < 0) {
        release_resources(fds, tcpfd, udpfd);
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Add the udpfd to the pollfd array
    fds[1].fd = udpfd;
    fds[1].events = POLLIN;

    // Add the STDIN_FILENO to the pollfd array
    fds[2].fd = STDIN_FILENO;
    fds[2].events = POLLIN;

    while (1) {
        if (poll(fds, nr_clients, -1) < 0) {
            release_resources(fds, tcpfd, udpfd);
            perror("poll");
            exit(EXIT_FAILURE);
        }

        for (int n = 0; n < nr_clients; n++) {
            if (!(fds[n].revents & POLLIN)) {
                continue;
            }

            if (fds[n].fd == tcpfd) {
                handle_new_subscriber_request(fds, poll_size, nr_clients, tcpfd, udpfd);
            } else if (fds[n].fd == udpfd) {
                handle_udp_client_request(fds, tcpfd, udpfd);
            } else if (fds[n].fd == STDIN_FILENO) {
                handle_server_input(fds, tcpfd, udpfd);
            } else {
                handle_subscriber(fds, nr_clients, n, tcpfd, udpfd);
            }
        }
    }
}

const int setup_tcp(sockaddr_in &serv_addr) {
    const int tcpfd = socket(AF_INET, SOCK_STREAM, 0);

    if (tcpfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    const int enable = 1;

    if (setsockopt(tcpfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        close(tcpfd);
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(tcpfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0) {
        close(tcpfd);
        perror("setsockopt(TCP_NODELAY) failed");
        exit(EXIT_FAILURE);
    }

    if (bind(tcpfd, (const sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(tcpfd);
        exit(EXIT_FAILURE);
    }

    return tcpfd;
}

const int setup_udp(sockaddr_in &serv_addr, const int &tcpfd) {
    const int udpfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (udpfd < 0) {
        close(tcpfd);
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // setNonBlocking(udpfd);

    if (setsockopt(udpfd, SOL_SOCKET, SO_REUSEADDR, &udpfd, sizeof(int)) < 0) {
        close(tcpfd);
        close(udpfd);
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(EXIT_FAILURE);
    }

    if (bind(udpfd, (const sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(tcpfd);
        close(udpfd);
        perror("bind");
        exit(EXIT_FAILURE);
    }

    return udpfd;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        cout << "Usage: " << argv[0] << " <port>" << '\n';
        return 1;
    }

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    uint16_t port;

    if (sscanf(argv[1], "%hu", &port) != 1) {
        cerr << "Invalid port" << '\n';
        return 1;
    }

    sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(sockaddr_in);

    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    const int tcpfd = setup_tcp(serv_addr);
    const int udpfd = setup_udp(serv_addr, tcpfd);

    run_server(tcpfd, udpfd);
    return 0;
}
