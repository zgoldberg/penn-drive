#ifndef COMMON_MAIL_H
#define COMMON_MAIL_H

#define MAX_CPUT_ITERATIONS 100
#define DELIMITER_STRING ";"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <signal.h>
#include <sys/select.h>
#include <errno.h>
#include <dirent.h>
#include <sys/file.h>

#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>

#ifdef BAZEL_BUILD
#include "kvstore/protos/kvstore.grpc.pb.h"
#else
#include "kvstore.grpc.pb.h"
#endif

#include "front_end_kv_client.h"


using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using grpc::InsecureChannelCredentials;

using kvstore::KeyValueStore;
using kvstore::PutRequest;
using kvstore::CPutRequest;
using kvstore::DeleteRequest;
using kvstore::GetRequest;

using kvstore::GetReply;
using kvstore::CPutReply;
using kvstore::PutReply;
using kvstore::DeleteReply;
using kvstore::ResponseCode;

using namespace std;


bool cput_loop(string row,
               string column,
               string value,
               KeyValueClient &client);
int append_to_metadata(string row, string column, string data_to_append, KeyValueClient &client);
string delete_hash_from_metadata_string(string metadata, string hash_to_delete);
string trim_str(const string &s);
vector<string> separate_string(string s, string delimiter);
bool user_exists(string username, KeyValueClient &client);
string get_user_password(string username, KeyValueClient &client);
bool string_in_vector(string s, vector<string>* vec);

#endif
