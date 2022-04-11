#ifndef USERS_H
#define USERS_H

#define MAX_NUM_CONNECTIONS 100
#define BUFFER_SIZE 102400

#include <string.h>
#include <string>
#include <map>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "mail.h"
#include "common_mail_files.h"
#include "http.h"
#include "front_end_kv_client.h"

using namespace std;

void handle_new_connection(int client_fd, http_message &req);
void handle_registration(int client_fd, http_message &req);
void handle_login(int client_fd, http_message &req);

int change_password(string username, string new_password, KeyValueClient &client);

# endif /* USERS_H */
