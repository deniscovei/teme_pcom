#include "requests.h"

#include <arpa/inet.h>
#include <netdb.h>      /* struct hostent, gethostbyname */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <stdio.h>
#include <stdlib.h>     /* exit, atoi, malloc, free */
#include <string.h>     /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <unistd.h>     /* read, write, close */

#include <vector>

#include "helpers.h"

using namespace std;

char *compute_delete_request(char *host, char *url, char *query_params,
                             vector<string> headers, vector<string> cookies) {
    char *message = (char *)calloc(BUFLEN, sizeof(char));
    char *line = (char *)calloc(LINELEN, sizeof(char));

    if (query_params != NULL) {
        sprintf(line, "DELETE %s?%s HTTP/1.1", url, query_params);
    } else {
        sprintf(line, "DELETE %s HTTP/1.1", url);
    }

    compute_message(message, line);

    memset(line, 0, LINELEN);
    sprintf(line, "Host: %s", host);
    compute_message(message, line);

    for (string header : headers) {
        memset(line, 0, LINELEN);
        sprintf(line, "%s", header.c_str());
        compute_message(message, line);
    }

    for (string cookie : cookies) {
        memset(line, 0, LINELEN);
        sprintf(line, "Cookie: %s", cookie.c_str());
        compute_message(message, line);
    }

    compute_message(message, "");
    free(line);
    return message;
}

char *compute_get_request(char *host, char *url, char *query_params,
                          vector<string> headers, vector<string> cookies) {
    char *message = (char *)calloc(BUFLEN, sizeof(char));
    char *line = (char *)calloc(LINELEN, sizeof(char));

    if (query_params != NULL) {
        sprintf(line, "GET %s?%s HTTP/1.1", url, query_params);
    } else {
        sprintf(line, "GET %s HTTP/1.1", url);
    }

    compute_message(message, line);

    memset(line, 0, LINELEN);
    sprintf(line, "Host: %s", host);
    compute_message(message, line);

    for (string header : headers) {
        memset(line, 0, LINELEN);
        sprintf(line, "%s", header.c_str());
        compute_message(message, line);
    }

    for (string cookie : cookies) {
        memset(line, 0, LINELEN);
        sprintf(line, "Cookie: %s", cookie.c_str());
        compute_message(message, line);
    }

    compute_message(message, "");

    free(line);
    return message;
}

char *compute_post_request(char *host, char *url, char *content_type, vector<string> headers, vector<string> body_data) {
    char *message = (char *)calloc(BUFLEN, sizeof(char));
    char *line = (char *)calloc(LINELEN, sizeof(char));
    char *body_data_buffer = (char *)calloc(LINELEN, sizeof(char));

    sprintf(line, "POST %s HTTP/1.1", url);
    compute_message(message, line);

    memset(line, 0, LINELEN);
    sprintf(line, "Host: %s", host);
    compute_message(message, line);

    for (string header : headers) {
        memset(line, 0, LINELEN);
        sprintf(line, "%s", header.c_str());
        compute_message(message, line);
    }

    memset(line, 0, LINELEN);
    sprintf(line, "Content-Type: %s", content_type);
    compute_message(message, line);

    int total_size = 0;
    for (string data : body_data) {
        total_size += data.size() + 1;
        strcat(body_data_buffer, data.c_str());
        strcat(body_data_buffer, "\n");
    }

    memset(line, 0, LINELEN);
    sprintf(line, "Content-Length: %d", total_size);
    compute_message(message, line);

    compute_message(message, "");

    memset(line, 0, LINELEN);
    strcat(message, body_data_buffer);

    free(line);
    free(body_data_buffer);
    return message;
}
