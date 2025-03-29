#ifndef __COMMON_H__
#define __COMMON_H__

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <string>
#include <unordered_map>
#include <unordered_set>

using namespace std;

#define INT 0
#define SHORT_REAL 1
#define FLOAT 2
#define STRING 3

#define IN_BUFFER_SIZE 2048
#define TOPIC_MAXSIZE 50
#define MSG_MAXSIZE 2048
#define UDP_MAXSIZE 1500
#define ID_MAXSIZE 128

#define LOCALHOST "127.0.0.1"

#define MAX_CONNECTIONS 128

typedef struct {
    char topic[TOPIC_MAXSIZE];
    uint8_t data_type;
    char data[UDP_MAXSIZE];
} __attribute__((packed, aligned(1))) udp_packet;

typedef struct {
    uint32_t ip;
    uint16_t port;
    char topic[TOPIC_MAXSIZE];
    uint8_t data_type;
    char data[UDP_MAXSIZE];
} __attribute__((packed, aligned(1))) meta_udp_packet;

struct subscriber_t {
    int sockfd;
    string id;
    uint32_t ip;
    uint16_t port;

    subscriber_t() : sockfd(-1), ip(0), port(0) {}

    subscriber_t(int sockfd, string id, uint32_t ip, uint16_t port) : sockfd(sockfd), id(id), ip(ip), port(port){};

    bool operator==(const subscriber_t& other) const {
        return id == other.id;
    }

    struct subscriber_hash {
        inline size_t operator()(const subscriber_t& subscriber) const {
            return hash<string>{}(subscriber.id);
        }
    };
};

// [socket_fd] -> [subscriber]
unordered_map<int, subscriber_t> subscribers;

// [subscriber] -> [topics]
unordered_map<subscriber_t, unordered_set<string>, subscriber_t::subscriber_hash> subscriber_topics;

// [id]
unordered_set<string> online_subscribers;

int recv_all(int sockfd, void* buffer, size_t len, int flags = 0) {
    size_t bytes_received = 0;
    size_t bytes_remaining = len;
    char* buff = (char*)buffer;

    while (bytes_remaining) {
        int received = recv(sockfd, buff + bytes_received, bytes_remaining, flags);

        if (received < 0) {
            return -1;
        } else if (received == 0) {
            break;
        }

        bytes_received += received;
        bytes_remaining -= received;
    }

    return bytes_received;
}

int send_all(int sockfd, void* buffer, size_t len, int flags = 0) {
    size_t bytes_sent = 0;
    size_t bytes_remaining = len;
    char* buff = (char*)buffer;

    while (bytes_remaining) {
        int sent = send(sockfd, buff + bytes_sent, bytes_remaining, flags);

        if (sent < 0) {
            return -1;
        } else if (sent == 0) {
            break;
        }

        bytes_sent += sent;
        bytes_remaining -= sent;
    }

    return bytes_sent;
}

#endif
