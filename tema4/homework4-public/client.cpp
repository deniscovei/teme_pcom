#include <arpa/inet.h>
#include <netdb.h>      /* struct hostent, gethostbyname */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <stdio.h>      /* printf, sprintf */
#include <stdlib.h>     /* exit, atoi, malloc, free */
#include <string.h>     /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <unistd.h>     /* read, write, close */

#include <cctype>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "helpers.h"
#include "requests.h"

using namespace std;

char *host_ip = (char *)"34.246.184.49";
int portno = 8080;

unordered_map<string, string> info;

unordered_map<string, int> commands = {
    {"register", REGISTER},
    {"login", LOGIN},
    {"enter_library", ENTER_LIBRARY},
    {"get_books", GET_BOOKS},
    {"get_book", GET_BOOK},
    {"add_book", ADD_BOOK},
    {"delete_book", DELETE_BOOK},
    {"logout", LOGOUT},
    {"exit", EXIT}};

void prompt_error(char *response) {
    char *error_message_start = strstr(response, "error");
    error_message_start += error_message_start == NULL ? 0 : 8;

    char *error_message_end = strstr(error_message_start, "\"");
    error_message_end -= error_message_end == NULL ? 0 : 1;

    cout << "ERROR: " << string(error_message_start, error_message_end) << "\n";
}

void _register() {
    char *message;
    char *response;

    int sockfd = open_connection(host_ip, portno, AF_INET, SOCK_STREAM, 0);

    char username[LINELEN], password[LINELEN];

    cout << "username=";
    cin.getline(username, LINELEN);
    cout << "password=";
    cin.getline(password, LINELEN);

    if (strchr(username, ' ')) {
        cout << "ERROR: Username " << username << " contains invalid characters!\n";
        close_connection(sockfd);
        return;
    }

    string body_data = "{\n\"username\": \"" + string(username) + "\",\n\"password\": \"" + string(password) + "\"\n}";
    message = compute_post_request(host_ip, (char *)"/api/v1/tema/auth/register", (char *)"application/json", {}, {body_data});

    send_to_server(sockfd, message);

    response = receive_from_server(sockfd);

    if (!strncmp(strrchr(response, '\n') + 1, "ok", 2)) {
        cout << "SUCCESS: User " << username << " has successfully registered!\n";
    } else {
        prompt_error(response);
    }

    free(message);
    free(response);
    close_connection(sockfd);
}

void login() {
    char *message;
    char *response;

    int sockfd = open_connection(host_ip, portno, AF_INET, SOCK_STREAM, 0);

    char username[LINELEN], password[LINELEN];

    cout << "username=";
    cin.getline(username, LINELEN);
    cout << "password=";
    cin.getline(password, LINELEN);

    if (strchr(username, ' ')) {
        cout << "ERROR: Username " << username << " contains invalid characters!\n";
        close_connection(sockfd);
        return;
    }

    string body_data = "{\n\"username\": \"" + string(username) + "\",\n\"password\": \"" + string(password) + "\"\n}";
    message = compute_post_request(host_ip, (char *)"/api/v1/tema/auth/login", (char *)"application/json", {}, {body_data});

    send_to_server(sockfd, message);

    response = receive_from_server(sockfd);

    if (strstr(response, "error")) {
        prompt_error(response);
    } else {
        char *cookie_start = strstr(response, "Set-Cookie") + 12;
        char *cookie_end = strstr(cookie_start, ";");
        info["connect"] = string(cookie_start, cookie_end);
        cout << "SUCCESS: User " << username << " has successfully logged in!\n";
    }

    free(message);
    free(response);
    close_connection(sockfd);
}

void enter_library() {
    char *message;
    char *response;

    int sockfd = open_connection(host_ip, portno, AF_INET, SOCK_STREAM, 0);

    message = compute_get_request(host_ip, (char *)"/api/v1/tema/library/access", NULL, {}, {info["connect"]});

    send_to_server(sockfd, message);

    response = receive_from_server(sockfd);

    if (strstr(response, "error")) {
        prompt_error(response);
    } else {
        char *token_start = strstr(response, "token") + 8;
        char *token_end = strstr(token_start, "\"");
        info["token"] = string(token_start, token_end);
        cout << "SUCCESS: User has successfully entered the library!\n";
    }

    free(message);
    free(response);
    close_connection(sockfd);
}

void get_books() {
    char *message;
    char *response;

    int sockfd = open_connection(host_ip, portno, AF_INET, SOCK_STREAM, 0);

    string auth = "Authorization: Bearer " + info["token"];
    message = compute_get_request(host_ip, (char *)"/api/v1/tema/library/books", NULL, {auth}, {});

    send_to_server(sockfd, message);

    response = receive_from_server(sockfd);

    if (strstr(response, "error")) {
        prompt_error(response);
    } else {
        char *books_start = strstr(response, "[");
        char *books_end = strrchr(books_start, ']');
        cout << string(books_start, books_end + 1) << "\n";
    }

    free(message);
    free(response);
    close_connection(sockfd);
}

void get_book() {
    char *message;
    char *response;

    int sockfd = open_connection(host_ip, portno, AF_INET, SOCK_STREAM, 0);

    char id[LINELEN];

    cout << "id=";
    cin.getline(id, LINELEN);

    string auth = "Authorization: Bearer " + info["token"];
    message = compute_get_request(host_ip, (char *)("/api/v1/tema/library/books/" + (string)id).c_str(), NULL, {auth}, {});

    send_to_server(sockfd, message);

    response = receive_from_server(sockfd);

    if (strstr(response, "error")) {
        prompt_error(response);
    } else {
        char *book_start = strstr(response, "{");
        char *book_end = strrchr(book_start, '}');
        cout << string(book_start, book_end + 1) << "\n";
    }

    free(message);
    free(response);
    close_connection(sockfd);
}

void add_book() {
    char *message;
    char *response;

    int sockfd = open_connection(host_ip, portno, AF_INET, SOCK_STREAM, 0);

    char title[LINELEN], author[LINELEN], genre[LINELEN], publisher[LINELEN], page_count[LINELEN];

    cout << "title=";
    cin.getline(title, LINELEN);
    cout << "author=";
    cin.getline(author, LINELEN);
    cout << "genre=";
    cin.getline(genre, LINELEN);
    cout << "publisher=";
    cin.getline(publisher, LINELEN);
    cout << "page_count=";
    cin.getline(page_count, LINELEN);

    if (page_count[0] == '\0' || page_count[0] == '0') {
        cout << "ERROR: Invalid page count!\n";
        close_connection(sockfd);
        return;
    }

    for (int i = 0; page_count[i]; i++) {
        if (!isdigit(page_count[i])) {
            cout << "ERROR: Page count must be a number!\n";
            close_connection(sockfd);
            return;
        }
    }

    string auth = "Authorization: Bearer " + info["token"];
    string body_data = "{\n\"title\": \"" + string(title) + "\",\n\"author\": \"" + string(author) + "\",\n\"genre\": \"" + string(genre) + "\",\n\"publisher\": \"" + string(publisher) + "\",\n\"page_count\": " + string(page_count) + "\n}";
    message = compute_post_request(host_ip, (char *)"/api/v1/tema/library/books", (char *)"application/json", {auth}, {body_data});

    send_to_server(sockfd, message);

    response = receive_from_server(sockfd);

    if (strstr(response, "error")) {
        prompt_error(response);
    } else {
        cout << "SUCCESS: Book " << title << " has been successfully added!\n";
    }

    free(message);
    free(response);
    close_connection(sockfd);
}

void delete_book() {
    char *message;
    char *response;

    int sockfd = open_connection(host_ip, portno, AF_INET, SOCK_STREAM, 0);

    char id[LINELEN];

    cout << "id=";
    cin.getline(id, LINELEN);

    string auth = "Authorization: Bearer " + info["token"];
    message = compute_delete_request(host_ip, (char *)("/api/v1/tema/library/books/" + (string)id).c_str(), NULL, {auth}, {});

    send_to_server(sockfd, message);

    response = receive_from_server(sockfd);

    if (strstr(response, "error")) {
        prompt_error(response);
    } else {
        cout << "SUCCESS: Book " << id << " has been successfully deleted!\n";
    }

    free(message);
    free(response);
    close_connection(sockfd);
}

void logout() {
    char *message;
    char *response;

    int sockfd = open_connection(host_ip, portno, AF_INET, SOCK_STREAM, 0);

    message = compute_get_request(host_ip, (char *)"/api/v1/tema/auth/logout", NULL, {}, {info["connect"]});

    send_to_server(sockfd, message);

    response = receive_from_server(sockfd);

    if (strstr(response, "error")) {
        prompt_error(response);
    } else {
        cout << "SUCCESS: You have successfully logged out!\n";
    }

    free(message);
    free(response);
    close_connection(sockfd);
}

int main(int argc, char *argv[]) {
    char buffer[LINELEN];

    while (cin.getline(buffer, LINELEN)) {
        switch (commands[(string)buffer]) {
            case REGISTER:
                _register();
                break;
            case LOGIN:
                login();
                break;
            case ENTER_LIBRARY:
                enter_library();
                break;
            case GET_BOOKS:
                get_books();
                break;
            case GET_BOOK:
                get_book();
                break;
            case ADD_BOOK:
                add_book();
                break;
            case DELETE_BOOK:
                delete_book();
                break;
            case LOGOUT:
                logout();
                break;
            case EXIT:
                return 0;
            default:
                cout << "ERROR: Invalid command!\n";
                break;
        }
    }

    return 0;
}