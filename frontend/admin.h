#ifndef ADMIN_H
#define ADMIN_H

#include "arpa/inet.h"
#include <sys/socket.h>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "http.h"

using namespace std;

void send_change_status(int client_fd, http_message &req);
void send_node_status(int client_fd,
                     const map<string, bool> &nodes_to_status,
                     http_message &req);
void change_node_status(int client_fd, http_message &req);
void request_node_status(int client_fd, http_message &req);
void get_admin_rows(int client_fd, http_message &req);
void get_cell_data(int client_fd, http_message &req);

# endif /* ADMIN_H */
