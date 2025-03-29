#ifndef _REQUESTS_
#define _REQUESTS_

#include <string>
#include <vector>
using namespace std;


char *compute_get_request(char *host, char *url, char *query_params,
                          vector<string> headers, vector<string> cookies);

char *compute_delete_request(char *host, char *url, char *query_params,
                          vector<string> headers, vector<string> cookies);

char *compute_post_request(char *host, char *url, char *content_type, vector<string>headers, vector<string>body_data);

#endif
