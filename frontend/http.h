#ifndef HTTP_H
#define HTTP_H

#include <sys/socket.h>
#include <string.h>
#include <map>
#include <string>
#include <chrono>

#ifdef BAZEL_BUILD
#include "kvstore/protos/kvstore.grpc.pb.h"
#else
#include "helloworld.grpc.pb.h"
#endif

#include <grpcpp/grpcpp.h>
#include "front_end_kv_client.h"
#include "common_mail_files.h"

using namespace std;

enum RequestType {
    GET,
    POST,
    HEAD
};

struct redir_thread_args {
    int client_fd;
    string connecting_ip_addr;
};

struct http_message {
    string top_line;
    map<string, string> headers;
    int body_length;
    string body;

    // For http requests only
    string route;
    RequestType request_type;
    map<string, string> cookies;
};

std::string get_timestamp();
string serialize_http_response(const http_message& resp);
int write_message(int fd, string buffer);
int read_message(int fd, char* buffer);
void parse_http_header(string line, http_message& m);
pair<string, string> username_sid_from_request(http_message &req);
bool verify_user(string &username, string &sid, KeyValueClient &client);

#endif /* HTTP_H */
