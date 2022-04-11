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
#include "front_end_kv_client.h"
#include <memory>
#include <string>

#include "front_end_kv_client.h"

#ifdef BAZEL_BUILD
#include "kvstore/protos/kvstore.grpc.pb.h"
#else
#include "kvstore.grpc.pb.h"
#endif

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
               KeyValueClient &client)
{
    for (int i = 0; i < MAX_CPUT_ITERATIONS; i++) {
        GetReply old_value_response = client.get_call(row, column);
        string old_value = old_value_response.value();
        if (old_value_response.error().size()) {
            continue;
        }
        CPutReply cput_response = client.cput_call(row,
                                                   column,
                                                   old_value,
                                                   value);
        // TODO: get ResponseCode enum
        switch (cput_response.response()) {
            case (ResponseCode::SUCCESS):
                return true;
            case(ResponseCode::FAILURE):
                return false;
            case (ResponseCode::CPUT_VAL_MISMATCH):
                break;
            case (ResponseCode::MISSING_KEY):
                break;
            default:
              break;
        }
    }
    return false;
}


int append_to_metadata(string row, string column, string data_to_append, KeyValueClient &client) {
  GetReply get_response = client.get_call(row, column);
  if (get_response.error().size()) {
    fprintf(stderr, "1st get failed\n");
    return 0;
  }
  string metadata = get_response.value();

  metadata.append(DELIMITER_STRING);
  metadata.append(data_to_append);
  bool cput_loop_response = cput_loop(row, column, metadata, client);

  if (!cput_loop_response) {
    fprintf(stderr, "2nd put failed\n");
    return 0;
  }

  return 1;
}


string delete_hash_from_metadata_string(string metadata, string hash_to_delete) {
    size_t position = 0;
    vector<string> hashes;
    while ((position = metadata.find(";")) != std::string::npos) {
        std::string substring = metadata.substr(0, position);
        if (substring != hash_to_delete) {
            hashes.push_back(substring);
        }
        metadata.erase(0, position + 1);
    }

    if (metadata != hash_to_delete) {
        hashes.push_back(metadata);
    }

    string new_metadata;
    for (long unsigned int i = 0; i < hashes.size(); i++) {
        new_metadata.append(hashes.at(i));
        new_metadata.append(";");
    }

    return new_metadata;
}


string trim_str(const string &s)
{
    auto iter_start = s.begin();
    auto iter_end = s.end();
    while (iter_start != iter_end && isspace(*iter_start)) {
        iter_start++;
    }

    do {
        iter_end--;
    } while (std::distance(iter_start, iter_end) > 0 && isspace(*iter_end));

    return string(iter_start, iter_end + 1);
}

vector<string> separate_string(string s, string delimiter) {
    size_t pos = 0;
    vector<string> ret;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        if (token.size() > 0) {
            ret.push_back(token);
        }
        s.erase(0, pos + delimiter.length());
    }

    if (s.size() > 0) {
        ret.push_back(s);
    }
    return ret;
}

bool user_exists(string username, KeyValueClient &client) {
  GetReply get_response = client.get_call(username, "password");
  if (get_response.response() == kvstore::ResponseCode::SUCCESS) {
    return true;
  }
  return false;
}

string get_user_password(string username, KeyValueClient &client) {
  GetReply get_response = client.get_call(username, "password");
  if (get_response.response() == kvstore::ResponseCode::SUCCESS) {
    return get_response.value();
  }
  return "false";
}


bool string_in_vector(string s, vector<string>* vec) {
  for (unsigned long i = 0; i < vec->size(); i++) {
    if (vec->at(i) == s) {
      return true;
    }
  }
  return false;
}
