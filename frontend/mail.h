#ifndef MAIL_H
#define MAIL_H

#define EXTERNAL_EMAIL_BUFFER_SIZE 102400
#define EMAIL_METADATA_COLUMN_NAME "email_metadata"
#define EMAIL_SENT_METADATA_COLUMN_NAME "sent_email_metadata"
#define EMAIL_DELIMITER_STRING ";"

#define LOCAL_DOMAIN "localhost"

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
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <grpcpp/grpcpp.h>
#include <memory>

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


bool init_emails_metadata(string username, KeyValueClient &client);
string mail_timestamp();
int write_email(string recipient, string sender, string email_body, KeyValueClient &client);
string list_email_hashes(string username, KeyValueClient &client);
string list_sent_email_hashes(string username, KeyValueClient &client);
string get_email_from_hash(string username, string hash, KeyValueClient &client);
int delete_email_from_hash(string username, string hash, KeyValueClient &client);
int get_num_messages(string username, KeyValueClient &client);
int get_maildrop_size(string username, KeyValueClient &client);

string get_domain(string address);
string get_before_domain(string address);
bool send_email_external(string sender_username, string recipient_address, string email_body);

#endif /* MAIL_H */
