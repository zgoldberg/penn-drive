#include <arpa/inet.h>
#include <errno.h>
#include <grpcpp/grpcpp.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <boost/functional/hash.hpp>
#include <boost/uuid/uuid.hpp>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "admin.h"
#include "common_mail_files.h"
#include "files.h"
#include "http.h"
#include "mail.h"
#include "pop3.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "smtp.h"
#include "users.h"

#ifdef BAZEL_BUILD
#include "kvstore/protos/kvstore.grpc.pb.h"
#else
#include "helloworld.grpc.pb.h"
#endif

using namespace std;
using namespace rapidjson;

using grpc::Channel;

#define MAX_NUM_CONNECTIONS 100
#define BUFFER_SIZE 102400

volatile bool keep_running = true;
bool debug_mode = false;
volatile bool master_node = false;
map<string, bool> nodes_to_status;
volatile bool dormant = false;

// Signal handler for SIGINT so that program can exit gracefully
void sig_handler(int sig) {
    if (sig == SIGINT) {
        keep_running = false;
    }
}

// Function to parse command-line arguments and populate relevent variables
string parse_args(int argc, char** argv, int* port_number, int* smtp_port_number, int* pop3_port_number) {
    char c;
    *port_number = 8000;  // default port number
    *smtp_port_number = 0;
    *pop3_port_number = 0;
    while ((c = getopt(argc, argv, "vmp:s:t:")) != -1) {
        switch (c) {
            case 'v':
                debug_mode = true;
                break;
            case 'p':
                sscanf(optarg, "%d", port_number);
                break;
            case 'm':
                master_node = true;
                break;
            case 's':
                sscanf(optarg, "%d", smtp_port_number);
                break;
            case 't':
                sscanf(optarg, "%d", pop3_port_number);
                break;
        }
    }

    if (optind < argc) {
        return string(argv[optind]);
    } else if (master_node) {
        if (debug_mode) fprintf(stderr, "Expected configuration file name\n");
        exit(1);
    }
    return "";
}

void handle_create_dir(int client_fd, http_message& req, string dir_name) {
    http_message resp;
    KeyValueClient client;
    auto user_sid_pair = username_sid_from_request(req);
    string username = user_sid_pair.first;
    string sid = user_sid_pair.second;
    bool user_authorized = (username != "" && sid != "");

    if (user_authorized) {
        MasterBackendClient master_client(
            grpc::CreateChannel(
                "localhost:5000",
                grpc::InsecureChannelCredentials()));
        string channel = master_client.which_node_call(username);

        client = KeyValueClient(
            grpc::CreateChannel(
                channel,
                grpc::InsecureChannelCredentials()));
        user_authorized = verify_user(username, sid, client);
    }

    if (!user_authorized) {
        resp.top_line = "HTTP/1.1 401 Unauthorized";
        resp.headers.insert(make_pair("Date", get_timestamp()));
        resp.headers.insert(make_pair("Content-Type", "application/json"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Credentials",
            "true"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Origin",
            req.headers["Origin"]));
        resp.body = string("{}");
        resp.headers.insert(make_pair("Content-Length",
                                      to_string(resp.body.size())));
        auto s = serialize_http_response(resp);
        write_message(client_fd, s);
        return;
    }

    if (dir_name.front() != '/') {
        dir_name.insert(0, 1, '/');
    }

    if (dir_name.back() != '/') {
        dir_name.append("/");
    }

    dir_name.append("x");  // just dummy filename for build_out_directory_path
    build_out_directory_path(username, dir_name, client);

    resp.top_line = "HTTP/1.1 200 OK";
    resp.headers.insert(make_pair("Date", get_timestamp()));
    resp.headers.insert(make_pair("Content-Type", "application/json"));
    resp.headers.insert(make_pair("Access-Control-Allow-Origin", "*"));
    resp.body = string("{}");
    resp.headers.insert(make_pair("Content-Length", to_string(resp.body.size())));
    auto s = serialize_http_response(resp);
    write_message(client_fd, s);
}


void handle_delete_dir(int client_fd, http_message& req, string dir_name) {
      http_message resp;
      KeyValueClient client;
      auto user_sid_pair = username_sid_from_request(req);
      string username = user_sid_pair.first;
      string sid = user_sid_pair.second;
      bool user_authorized = (username != "") && (username != "");

      if (user_authorized) {
          MasterBackendClient master_client(
              grpc::CreateChannel(
                  "localhost:5000",
                  grpc::InsecureChannelCredentials()));
          string channel = master_client.which_node_call(username);

          client = KeyValueClient(
              grpc::CreateChannel(
                  channel,
                  grpc::InsecureChannelCredentials()));

          user_authorized = verify_user(username, sid, client);
      }

      if (!user_authorized) {
          resp.top_line = "HTTP/1.1 401 Unauthorized";
          resp.headers.insert(make_pair("Date", get_timestamp()));
          resp.headers.insert(make_pair("Content-Type", "application/json"));
          resp.headers.insert(make_pair(
              "Access-Control-Allow-Credentials",
              "true"));
          resp.headers.insert(make_pair(
              "Access-Control-Allow-Origin",
              req.headers["Origin"]));
          resp.body = string("{}");
          resp.headers.insert(make_pair("Content-Length",
                                        to_string(resp.body.size())));
          auto s = serialize_http_response(resp);
          write_message(client_fd, s);
          return;
      }

      if (dir_name.front() != '/') {
          dir_name.insert(0, 1, '/');
      }

      cout << "HERE 203" << endl;

      delete_dir(username, dir_name, client);

      cout << "HERE 207" << endl;

      resp.top_line = "HTTP/1.1 200 OK";
      resp.headers.insert(make_pair("Date", get_timestamp()));
      resp.headers.insert(make_pair("Content-Type", "application/json"));
      resp.headers.insert(make_pair(
          "Access-Control-Allow-Credentials",
          "true"));
      resp.headers.insert(make_pair(
          "Access-Control-Allow-Origin",
          req.headers["Origin"]));
      resp.body = string("{}");
      resp.headers.insert(make_pair("Content-Length", to_string(resp.body.size())));
      auto s = serialize_http_response(resp);
      write_message(client_fd, s);

}


void handle_delete_file(int client_fd, http_message req, string absolute_file_path) {
    http_message resp;
    KeyValueClient client;
    auto user_sid_pair = username_sid_from_request(req);
    string username = user_sid_pair.first;
    string sid = user_sid_pair.second;
    bool user_authorized = (username != "") && (username != "");

    if (user_authorized) {
        MasterBackendClient master_client(
            grpc::CreateChannel(
                "localhost:5000",
                grpc::InsecureChannelCredentials()));
        string channel = master_client.which_node_call(username);

        client = KeyValueClient(
            grpc::CreateChannel(
                channel,
                grpc::InsecureChannelCredentials()));

        user_authorized = verify_user(username, sid, client);
    }

    if (!user_authorized) {
        resp.top_line = "HTTP/1.1 401 Unauthorized";
        resp.headers.insert(make_pair("Date", get_timestamp()));
        resp.headers.insert(make_pair("Content-Type", "application/json"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Credentials",
            "true"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Origin",
            req.headers["Origin"]));
        resp.body = string("{}");
        resp.headers.insert(make_pair("Content-Length",
                                      to_string(resp.body.size())));
        auto s = serialize_http_response(resp);
        write_message(client_fd, s);
        return;
    }

    if (absolute_file_path.front() != '/') {
        absolute_file_path.insert(0, 1, '/');
    }

    delete_file(username, absolute_file_path, client);

    resp.top_line = "HTTP/1.1 200 OK";
    resp.headers.insert(make_pair("Date", get_timestamp()));
    resp.headers.insert(make_pair("Content-Type", "application/json"));
    resp.headers.insert(make_pair(
        "Access-Control-Allow-Credentials",
        "true"));
    resp.headers.insert(make_pair(
        "Access-Control-Allow-Origin",
        req.headers["Origin"]));
    resp.body = string("{}");
    resp.headers.insert(make_pair("Content-Length", to_string(resp.body.size())));
    auto s = serialize_http_response(resp);
    write_message(client_fd, s);
}

void get_file(int client_fd, http_message req) {
    http_message resp;
    KeyValueClient client;
    auto user_sid_pair = username_sid_from_request(req);
    string username = user_sid_pair.first;
    string sid = user_sid_pair.second;
    bool user_authorized = (username != "") && (sid != "");

    if (user_authorized) {
        MasterBackendClient master_client(
            grpc::CreateChannel(
                "localhost:5000",
                grpc::InsecureChannelCredentials()));

        string channel = master_client.which_node_call(username);
        client = KeyValueClient(
            grpc::CreateChannel(
                channel,
                grpc::InsecureChannelCredentials()));

        user_authorized = verify_user(username, sid, client);
    }

    if (!user_authorized) {
        resp.top_line = "HTTP/1.1 401 Unauthorized";
        resp.headers.insert(make_pair("Date", get_timestamp()));
        resp.headers.insert(make_pair("Content-Type", "application/json"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Credentials",
            "true"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Origin",
            req.headers["Origin"]));
        resp.body = string("{}");
        resp.headers.insert(make_pair("Content-Length",
                                      to_string(resp.body.size())));
        auto s = serialize_http_response(resp);
        write_message(client_fd, s);
        return;
    }

    auto query_string = separate_string(req.route, "?")[1];
    auto qargs = separate_string(query_string, "&");
    string filename = separate_string(qargs[0], "=")[1];

    string file_data = get_file_data(username, filename, client);

    resp.top_line = "HTTP/1.1 200 OK";
    resp.headers.insert(make_pair("Date", get_timestamp()));
    resp.headers.insert(make_pair("Content-Type", "application/force-download"));

    string disposition("attachment; filename=\"");
    disposition.append(filename.substr(1, filename.length() + 1));
    disposition.append("\"");
    resp.headers.insert(make_pair("Content-Disposition", disposition));

    resp.headers.insert(make_pair("Content-Transfer-Encoding", "binary"));  // https://stackoverflow.com/questions/6520231/how-to-force-browser-to-download-file
    resp.headers.insert(make_pair(
        "Access-Control-Allow-Credentials",
        "true"));
    resp.headers.insert(make_pair(
        "Access-Control-Allow-Origin",
        req.headers["Origin"]));

    resp.body = file_data;
    resp.headers.insert(make_pair("Content-Length", to_string(resp.body.size())));

    if (req.request_type == HEAD) {
      resp.body = "";
    }

    // char send_buffer[BUFFER_SIZE + 10];
    auto s = serialize_http_response(resp);
    // memcpy(send_buffer, s.c_str(), s.size());
    write_message(client_fd, s);
}


void get_files(int client_fd, string root, http_message req) {
    http_message resp;
    KeyValueClient client;
    auto user_sid_pair = username_sid_from_request(req);
    string username = user_sid_pair.first;
    string sid = user_sid_pair.second;
    bool user_authorized = (username != "") && (sid != "");

    if (user_authorized) {
        MasterBackendClient master_client(
            grpc::CreateChannel(
                "localhost:5000",
                grpc::InsecureChannelCredentials()));

        string channel = master_client.which_node_call(username);
        client = KeyValueClient(
            grpc::CreateChannel(
                channel,
                grpc::InsecureChannelCredentials()));

        user_authorized = verify_user(username, sid, client);
    }

    if (!user_authorized) {
        resp.top_line = "HTTP/1.1 401 Unauthorized";
        resp.headers.insert(make_pair("Date", get_timestamp()));
        resp.headers.insert(make_pair("Content-Type", "application/json"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Credentials",
            "true"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Origin",
            req.headers["Origin"]));
        resp.body = string("{}");
        resp.headers.insert(make_pair("Content-Length",
                                      to_string(resp.body.size())));
        auto s = serialize_http_response(resp);
        write_message(client_fd, s);
        return;
    }

    resp.top_line = "HTTP/1.1 200 OK";
    resp.headers.insert(make_pair("Date", get_timestamp()));
    resp.headers.insert(make_pair("Content-Type", "application/json"));
    resp.headers.insert(make_pair(
        "Access-Control-Allow-Credentials",
        "true"));
    resp.headers.insert(make_pair(
        "Access-Control-Allow-Origin",
        req.headers["Origin"]));

    string files_metadata = get_files(username, client);

    Document outDoc;
    outDoc.SetObject();
    Document::AllocatorType& allocator = outDoc.GetAllocator();

    auto files_and_dirs_strings = get_all_files_json(username, root, allocator, client);
    string all_files_string = files_and_dirs_strings.first;
    string all_dirs_string = files_and_dirs_strings.second;

    // outDoc.AddMember("files", files_metadata, allocator);
    outDoc.AddMember("all_files", all_files_string, allocator);
    outDoc.AddMember("all_dirs", all_dirs_string, allocator);

    StringBuffer sb;
    Writer<StringBuffer> writer(sb);
    outDoc.Accept(writer);

    resp.body = string(sb.GetString());
    resp.headers.insert(make_pair("Content-Length", to_string(resp.body.size())));

    if (req.request_type == HEAD) {
      resp.body = "";
    }

    auto s = serialize_http_response(resp);
    write_message(client_fd, s);
}


void handle_share_file(int client_fd, http_message req) {

  http_message resp;
  KeyValueClient client;
  auto user_sid_pair = username_sid_from_request(req);
  string username = user_sid_pair.first;
  string sid = user_sid_pair.second;
  bool user_authorized = (username != "") && (sid != "");

  if (user_authorized) {
      MasterBackendClient master_client(
          grpc::CreateChannel(
              "localhost:5000",
              grpc::InsecureChannelCredentials()));

      string channel = master_client.which_node_call(username);
      client = KeyValueClient(
          grpc::CreateChannel(
              channel,
              grpc::InsecureChannelCredentials()));

      user_authorized = verify_user(username, sid, client);
  }

  if (!user_authorized) {
      resp.top_line = "HTTP/1.1 401 Unauthorized";
      resp.headers.insert(make_pair("Date", get_timestamp()));
      resp.headers.insert(make_pair("Content-Type", "application/json"));
      resp.headers.insert(make_pair(
          "Access-Control-Allow-Credentials",
          "true"));
      resp.headers.insert(make_pair(
          "Access-Control-Allow-Origin",
          req.headers["Origin"]));
      resp.body = string("{}");
      resp.headers.insert(make_pair("Content-Length",
                                    to_string(resp.body.size())));
      auto s = serialize_http_response(resp);
      write_message(client_fd, s);
      return;
  }

  auto query_string = separate_string(req.route, "?")[1];
  auto qargs = separate_string(query_string, "&");
  string recipient_and_filename_string = separate_string(qargs[0], "=")[1];
  auto recipient_and_filename = separate_string(recipient_and_filename_string, "@@@");
  string recipient = recipient_and_filename[0];
  string filename = recipient_and_filename[1];

  bool share_res = share_file(username, recipient, filename, client);
  cout << "HERE 424 " << share_res << endl;

  resp.top_line = "HTTP/1.1 200 OK";
  resp.headers.insert(make_pair("Date", get_timestamp()));
  resp.headers.insert(make_pair("Content-Type", "application/json"));
  resp.headers.insert(make_pair(
      "Access-Control-Allow-Credentials",
      "true"));
  resp.headers.insert(make_pair(
      "Access-Control-Allow-Origin",
      req.headers["Origin"]));
  resp.body = "{}";
  resp.headers.insert(make_pair("Content-Length", to_string(resp.body.size())));

  auto s = serialize_http_response(resp);
  write_message(client_fd, s);

}


void get_shared_file(int client_fd, http_message req) {
    http_message resp;
    KeyValueClient client;
    auto user_sid_pair = username_sid_from_request(req);
    string username = user_sid_pair.first;
    string sid = user_sid_pair.second;
    bool user_authorized = (username != "") && (sid != "");

    if (user_authorized) {
        MasterBackendClient master_client(
            grpc::CreateChannel(
                "localhost:5000",
                grpc::InsecureChannelCredentials()));

        string channel = master_client.which_node_call(username);
        client = KeyValueClient(
            grpc::CreateChannel(
                channel,
                grpc::InsecureChannelCredentials()));

        user_authorized = verify_user(username, sid, client);
    }

    if (!user_authorized) {
        resp.top_line = "HTTP/1.1 401 Unauthorized";
        resp.headers.insert(make_pair("Date", get_timestamp()));
        resp.headers.insert(make_pair("Content-Type", "application/json"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Credentials",
            "true"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Origin",
            req.headers["Origin"]));
        resp.body = string("{}");
        resp.headers.insert(make_pair("Content-Length",
                                      to_string(resp.body.size())));
        auto s = serialize_http_response(resp);
        write_message(client_fd, s);
        return;
    }

    auto query_string = separate_string(req.route, "?")[1];
    auto qargs = separate_string(query_string, "&");
    string sender_and_filename_string = separate_string(qargs[0], "=")[1];
    auto sender_and_filename = separate_string(sender_and_filename_string, "@@@");
    string sender = sender_and_filename[0];
    string filename = sender_and_filename[1];

    string file_data = get_file_data(sender, filename, client);

    resp.top_line = "HTTP/1.1 200 OK";
    resp.headers.insert(make_pair("Date", get_timestamp()));
    resp.headers.insert(make_pair("Content-Type", "application/force-download"));

    string disposition("attachment; filename=\"");
    disposition.append(filename.substr(1, filename.length() + 1));
    disposition.append("\"");
    resp.headers.insert(make_pair("Content-Disposition", disposition));

    resp.headers.insert(make_pair("Content-Transfer-Encoding", "binary"));  // https://stackoverflow.com/questions/6520231/how-to-force-browser-to-download-file
    resp.headers.insert(make_pair(
        "Access-Control-Allow-Credentials",
        "true"));
    resp.headers.insert(make_pair(
        "Access-Control-Allow-Origin",
        req.headers["Origin"]));

    resp.body = file_data;
    resp.headers.insert(make_pair("Content-Length", to_string(resp.body.size())));

    if (req.request_type == HEAD) {
      resp.body = "";
    }

    // char send_buffer[BUFFER_SIZE + 10];
    auto s = serialize_http_response(resp);
    // memcpy(send_buffer, s.c_str(), s.size());
    write_message(client_fd, s);
}


void get_shared_files(int client_fd, http_message req) {
    http_message resp;
    KeyValueClient client;
    auto user_sid_pair = username_sid_from_request(req);
    string username = user_sid_pair.first;
    string sid = user_sid_pair.second;
    bool user_authorized = (username != "") && (sid != "");

    if (user_authorized) {
        MasterBackendClient master_client(
            grpc::CreateChannel(
                "localhost:5000",
                grpc::InsecureChannelCredentials()));

        string channel = master_client.which_node_call(username);
        client = KeyValueClient(
            grpc::CreateChannel(
                channel,
                grpc::InsecureChannelCredentials()));

        user_authorized = verify_user(username, sid, client);
    }

    if (!user_authorized) {
        resp.top_line = "HTTP/1.1 401 Unauthorized";
        resp.headers.insert(make_pair("Date", get_timestamp()));
        resp.headers.insert(make_pair("Content-Type", "application/json"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Credentials",
            "true"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Origin",
            req.headers["Origin"]));
        resp.body = string("{}");
        resp.headers.insert(make_pair("Content-Length",
                                      to_string(resp.body.size())));
        auto s = serialize_http_response(resp);
        write_message(client_fd, s);
        return;
    }

    resp.top_line = "HTTP/1.1 200 OK";
    resp.headers.insert(make_pair("Date", get_timestamp()));
    resp.headers.insert(make_pair("Content-Type", "application/json"));
    resp.headers.insert(make_pair(
        "Access-Control-Allow-Credentials",
        "true"));
    resp.headers.insert(make_pair(
        "Access-Control-Allow-Origin",
        req.headers["Origin"]));

    Document outDoc;
    outDoc.SetObject();
    Document::AllocatorType& allocator = outDoc.GetAllocator();

    string shared_file_metadata_string = get_shared_files_metadata(username, client);
    outDoc.AddMember("shared_files_metadata", shared_file_metadata_string, allocator);

    StringBuffer sb;
    Writer<StringBuffer> writer(sb);
    outDoc.Accept(writer);

    resp.body = string(sb.GetString());
    resp.headers.insert(make_pair("Content-Length", to_string(resp.body.size())));

    if (req.request_type == HEAD) {
      resp.body = "";
    }

    auto s = serialize_http_response(resp);
    write_message(client_fd, s);
}


void get_emails(int client_fd, http_message req) {
    // Data received from the client
    auto query_string = separate_string(req.route, "?")[1];
    auto qargs = separate_string(query_string, "&");
    int email_idx = stoi(separate_string(qargs[0], "=")[1]);
    int emails_to_process = stoi(separate_string(qargs[1], "=")[1]);

    http_message resp;
    KeyValueClient client;
    auto user_sid_pair = username_sid_from_request(req);
    string username = user_sid_pair.first;
    string sid = user_sid_pair.second;
    bool user_authorized = (username != "") && (sid != "");

    if (user_authorized) {
        MasterBackendClient master_client(
            grpc::CreateChannel(
                "localhost:5000",
                grpc::InsecureChannelCredentials()));

        string channel = master_client.which_node_call(username);
        client = KeyValueClient(
            grpc::CreateChannel(
                channel,
                grpc::InsecureChannelCredentials()));

        user_authorized = verify_user(username, sid, client);
    }

    if (!user_authorized) {
        resp.top_line = "HTTP/1.1 401 Unauthorized";
        resp.headers.insert(make_pair("Date", get_timestamp()));
        resp.headers.insert(make_pair("Content-Type", "application/json"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Credentials",
            "true"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Origin",
            req.headers["Origin"]));
        resp.body = string("{}");
        resp.headers.insert(make_pair("Content-Length",
                                      to_string(resp.body.size())));
        auto s = serialize_http_response(resp);
        write_message(client_fd, s);
        return;
    }

    init_emails_metadata(username, client);
    string email_hashes = list_email_hashes(username, client);

    // Making the assumption that email hashes are semicolon separated
    auto hash_vect = separate_string(email_hashes, ";");

    Document outDoc;
    outDoc.SetObject();
    Value data_array(kArrayType);
    Document::AllocatorType& allocator = outDoc.GetAllocator();

    for (auto i = email_idx; i < min((int)hash_vect.size(), email_idx + emails_to_process); i++) {
        // Add relevant emails to the json body
        string hash = hash_vect.at(i);
        auto email_info = get_email_from_hash(username, hash, client);
        cerr << "email:" << email_info << endl;

        // Parse email info into header/from/body
        auto email_vec = separate_string(email_info, "\r\n");
        string sender_str = separate_string(email_vec[0], " ")[1];
        string date_str = separate_string(email_vec[0], "<")[2];  // this was causing segfault for me

        date_str = date_str.substr(0, date_str.size() - 1);

        int subject_index = email_info.find("Subject: ");
        string subject_and_more = email_info.substr(subject_index + 9, email_info.length());
        int line_index = subject_and_more.find("\r\n");
        string email_subj_str = subject_and_more.substr(0, line_index);

        int to_index = email_info.find("To: ");
        string to_and_more = email_info.substr(to_index + 4, email_info.length());
        line_index = to_and_more.find("\r\n");
        string email_to_str = to_and_more.substr(0, line_index);

        int body_index = email_info.find("\r\n\r\n");
        string email_body_str = email_info.substr(body_index, email_info.length());

        Value objValue;
        objValue.SetObject();
        Value sender(sender_str.c_str(), allocator);
        Value date(date_str.c_str(), allocator);
        Value email_subj(email_subj_str.c_str(), allocator);
        Value email_body(email_body_str.c_str(), allocator);
        Value email_to(email_to_str.c_str(), allocator);

        objValue.AddMember("sender", sender, allocator);
        objValue.AddMember("recipient", email_to, allocator);
        objValue.AddMember("date", date, allocator);
        objValue.AddMember("subject", email_subj, allocator);
        objValue.AddMember("body", email_body, allocator);
        objValue.AddMember("hash", hash, allocator);
        data_array.PushBack(objValue, allocator);
    }

    outDoc.AddMember("data", data_array, allocator);
    StringBuffer sb;
    Writer<StringBuffer> writer(sb);
    outDoc.Accept(writer);
    resp.body = string(sb.GetString());

    resp.top_line = "HTTP/1.1 200 OK";
    resp.headers.insert(make_pair("Date", get_timestamp()));
    resp.headers.insert(make_pair("Content-Type", "application/json"));
    resp.headers.insert(
        make_pair("Access-Control-Allow-Origin", req.headers["Origin"]));
    resp.headers.insert(make_pair("Access-Control-Allow-Credentials", "true"));
    resp.headers.insert(
        make_pair("Content-Length", to_string(resp.body.size())));


    if (req.request_type == HEAD) {
      resp.body = "";
    }

    auto s = serialize_http_response(resp);
    write_message(client_fd, s);
    // if (debug_mode) {
    //     fprintf(stderr, "[%d] S: %s", client_fd, s.c_str());
    // }
}

void get_sent_emails(int client_fd, http_message req) {
    // Data received from the client
    auto query_string = separate_string(req.route, "?")[1];
    auto qargs = separate_string(query_string, "&");
    int email_idx = stoi(separate_string(qargs[0], "=")[1]);
    int emails_to_process = stoi(separate_string(qargs[1], "=")[1]);

    http_message resp;
    KeyValueClient client;
    auto user_sid_pair = username_sid_from_request(req);
    string username = user_sid_pair.first;
    string sid = user_sid_pair.second;
    bool user_authorized = (username != "") && (sid != "");

    if (user_authorized) {
        MasterBackendClient master_client(
            grpc::CreateChannel(
                "localhost:5000",
                grpc::InsecureChannelCredentials()));

        string channel = master_client.which_node_call(username);
        client = KeyValueClient(
            grpc::CreateChannel(
                channel,
                grpc::InsecureChannelCredentials()));

        user_authorized = verify_user(username, sid, client);
    }

    if (!user_authorized) {
        resp.top_line = "HTTP/1.1 401 Unauthorized";
        resp.headers.insert(make_pair("Date", get_timestamp()));
        resp.headers.insert(make_pair("Content-Type", "application/json"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Credentials",
            "true"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Origin",
            req.headers["Origin"]));
        resp.body = string("{}");
        resp.headers.insert(make_pair("Content-Length",
                                      to_string(resp.body.size())));
        auto s = serialize_http_response(resp);
        write_message(client_fd, s);
        return;
    }

    init_emails_metadata(username, client);
    string email_hashes = list_sent_email_hashes(username, client);

    // Making the assumption that email hashes are semicolon separated
    auto hash_vect = separate_string(email_hashes, ";");

    Document outDoc;
    outDoc.SetObject();
    Value data_array(kArrayType);
    Document::AllocatorType& allocator = outDoc.GetAllocator();

    for (auto i = email_idx; i < min((int)hash_vect.size(), email_idx + emails_to_process); i++) {
        // Add relevant emails to the json body
        string hash = hash_vect.at(i);
        auto email_info = get_email_from_hash(username, hash, client);

        // Parse email info into header/from/body
        auto email_vec = separate_string(email_info, "\r\n");
        string sender_str = separate_string(email_vec[0], " ")[1];
        string date_str = separate_string(email_vec[0], "<")[2];  // this was causing segfault for me

        date_str = date_str.substr(0, date_str.size() - 1);

        int subject_index = email_info.find("Subject: ");
        string subject_and_more = email_info.substr(subject_index + 9, email_info.length());
        int line_index = subject_and_more.find("\r\n");
        string email_subj_str = subject_and_more.substr(0, line_index);

        int to_index = email_info.find("To: ");
        string to_and_more = email_info.substr(to_index + 4, email_info.length());
        line_index = to_and_more.find("\r\n");
        string email_to_str = to_and_more.substr(0, line_index);

        int body_index = email_info.find("\r\n\r\n");
        string email_body_str = email_info.substr(body_index, email_info.length());

        Value objValue;
        objValue.SetObject();
        Value sender(sender_str.c_str(), allocator);
        Value date(date_str.c_str(), allocator);
        Value email_subj(email_subj_str.c_str(), allocator);
        Value email_body(email_body_str.c_str(), allocator);
        Value email_to(email_to_str.c_str(), allocator);

        objValue.AddMember("sender", sender, allocator);
        objValue.AddMember("recipient", email_to, allocator);
        objValue.AddMember("date", date, allocator);
        objValue.AddMember("subject", email_subj, allocator);
        objValue.AddMember("body", email_body, allocator);
        objValue.AddMember("hash", hash, allocator);
        data_array.PushBack(objValue, allocator);
    }

    outDoc.AddMember("data", data_array, allocator);
    StringBuffer sb;
    Writer<StringBuffer> writer(sb);
    outDoc.Accept(writer);

    resp.body = string(sb.GetString());

    resp.top_line = "HTTP/1.1 200 OK";
    resp.headers.insert(make_pair("Date", get_timestamp()));
    resp.headers.insert(make_pair("Content-Type", "application/json"));
    resp.headers.insert(
        make_pair("Access-Control-Allow-Origin", req.headers["Origin"]));
    resp.headers.insert(make_pair("Access-Control-Allow-Credentials", "true"));
    resp.headers.insert(
        make_pair("Content-Length", to_string(resp.body.size())));

    if (req.request_type == HEAD) {
      resp.body = "";
    }

    auto s = serialize_http_response(resp);
    write_message(client_fd, s);
    // if (debug_mode) {
    //     fprintf(stderr, "[%d] S: %s", client_fd, s.c_str());
    // }
}

bool handle_move_file(int client_fd, http_message req) {
    http_message resp;
    KeyValueClient client;
    auto user_sid_pair = username_sid_from_request(req);
    string username = user_sid_pair.first;
    string sid = user_sid_pair.second;
    bool user_authorized = (username != "") && (sid != "");

    if (user_authorized) {
        MasterBackendClient master_client(
            grpc::CreateChannel(
                "localhost:5000",
                grpc::InsecureChannelCredentials()));

        string channel = master_client.which_node_call(username);
        client = KeyValueClient(
            grpc::CreateChannel(
                channel,
                grpc::InsecureChannelCredentials()));

        user_authorized = verify_user(username, sid, client);
    }

    if (!user_authorized) {
        resp.top_line = "HTTP/1.1 401 Unauthorized";
        resp.headers.insert(make_pair("Date", get_timestamp()));
        resp.headers.insert(make_pair("Content-Type", "application/json"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Credentials",
            "true"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Origin",
            req.headers["Origin"]));
        resp.body = string("{}");
        resp.headers.insert(make_pair("Content-Length",
                                      to_string(resp.body.size())));
        auto s = serialize_http_response(resp);
        write_message(client_fd, s);
        return false;
    }

    Document d;
    d.Parse(req.body.c_str());
    string old_absolute_filepath = d["old_absolute_filepath"].GetString();
    string new_absolute_filepath = d["new_absolute_filepath"].GetString();

    int move_result = move_file(username, old_absolute_filepath, new_absolute_filepath, client);

    string error_message = "";
    if (move_result == 1) {
        error_message = "No such file";  // shouldn't be possible
    } else if (move_result == 2) {
        error_message = "Can't rename to file that already exists";
    }

    resp.top_line = "HTTP/1.1 200 OK";
    resp.headers.insert(make_pair("Date", get_timestamp()));
    resp.headers.insert(make_pair("Access-Control-Allow-Origin", req.headers["Origin"]));
    resp.headers.insert(make_pair("Access-Control-Allow-Credentials", "true"));

    Document outDoc;
    outDoc.SetObject();
    Document::AllocatorType& allocator = outDoc.GetAllocator();
    outDoc.AddMember("moved_file", move_result == 0, allocator);
    outDoc.AddMember("error", error_message, allocator);
    StringBuffer sb;
    Writer<StringBuffer> writer(sb);
    outDoc.Accept(writer);
    resp.body = string(sb.GetString());

    resp.headers.insert(make_pair("Content-Length", to_string(resp.body.length())));

    auto s = serialize_http_response(resp);
    write_message(client_fd, s);
    return true;
}


bool handle_rename_dir(int client_fd, http_message req) {
    http_message resp;
    KeyValueClient client;
    auto user_sid_pair = username_sid_from_request(req);
    string username = user_sid_pair.first;
    string sid = user_sid_pair.second;
    bool user_authorized = (username != "") && (sid != "");

    if (user_authorized) {
        MasterBackendClient master_client(
            grpc::CreateChannel(
                "localhost:5000",
                grpc::InsecureChannelCredentials()));

        string channel = master_client.which_node_call(username);
        client = KeyValueClient(
            grpc::CreateChannel(
                channel,
                grpc::InsecureChannelCredentials()));

        user_authorized = verify_user(username, sid, client);
    }

    if (!user_authorized) {
        resp.top_line = "HTTP/1.1 401 Unauthorized";
        resp.headers.insert(make_pair("Date", get_timestamp()));
        resp.headers.insert(make_pair("Content-Type", "application/json"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Credentials",
            "true"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Origin",
            req.headers["Origin"]));
        resp.body = string("{}");
        resp.headers.insert(make_pair("Content-Length",
                                      to_string(resp.body.size())));
        auto s = serialize_http_response(resp);
        write_message(client_fd, s);
        return false;
    }

    cout << "HERE 1046 " << req.body << endl;

    Document d;
    d.Parse(req.body.c_str());

    cout << "HERE 1051" << endl;

    string old_dir_name = d["old_dir_name"].GetString();

    cout << "HERE 1055" << endl;

    string new_dir_name = d["new_dir_name"].GetString();

    int move_result = rename_dir(username, old_dir_name, new_dir_name, client);

    string error_message = "";
    // if (move_result == 1) {
    //     error_message = "No such file";  // shouldn't be possible
    // } else if (move_result == 2) {
    //     error_message = "Can't rename to file that already exists";
    // }

    resp.top_line = "HTTP/1.1 200 OK";
    resp.headers.insert(make_pair("Date", get_timestamp()));
    resp.headers.insert(make_pair("Access-Control-Allow-Origin", req.headers["Origin"]));
    resp.headers.insert(make_pair("Access-Control-Allow-Credentials", "true"));

    Document outDoc;
    outDoc.SetObject();
    Document::AllocatorType& allocator = outDoc.GetAllocator();
    outDoc.AddMember("moved_file", move_result == 0, allocator);
    outDoc.AddMember("error", error_message, allocator);
    StringBuffer sb;
    Writer<StringBuffer> writer(sb);
    outDoc.Accept(writer);
    resp.body = string(sb.GetString());

    resp.headers.insert(make_pair("Content-Length", to_string(resp.body.length())));

    auto s = serialize_http_response(resp);
    write_message(client_fd, s);
    return true;
}


bool handle_upload_file(int client_fd, http_message req, string root_dir) {
    http_message resp;
    auto user_sid_pair = username_sid_from_request(req);
    string username = user_sid_pair.first;
    string sid = user_sid_pair.second;

    MasterBackendClient master_client(
        grpc::CreateChannel(
            "localhost:5000",
            grpc::InsecureChannelCredentials()));

    string channel = master_client.which_node_call(username);
    KeyValueClient client(
        grpc::CreateChannel(
            channel,
            grpc::InsecureChannelCredentials()));

    bool user_authorized = verify_user(username, sid, client);
    if (!user_authorized) {
        resp.top_line = "HTTP/1.1 401 Unauthorized";
        resp.headers.insert(make_pair("Date", get_timestamp()));
        resp.headers.insert(make_pair("Content-Type", "application/json"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Credentials",
            "true"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Origin",
            req.headers["Origin"]));
        resp.body = string("{}");
        resp.headers.insert(make_pair("Content-Length",
                                      to_string(resp.body.size())));
        auto s = serialize_http_response(resp);
        write_message(client_fd, s);
        return false;
    }

    string fname = separate_string(separate_string(req.body, "filename=")[1], "\r\n")[0];
    fname = fname.substr(1, fname.length() - 2);
    string fpath(root_dir);

    if (fpath[fpath.length() - 1] != '/') {
        fpath.append("/");
    }

    fpath.append(fname);

    string boundary = separate_string(req.headers["Content-Type"], "boundary=")[1];
    string first_boundary = "\r\n\r\n";
    string second_boundary = boundary;

    int first_boundary_index = req.body.find(first_boundary);
    string b = req.body.substr(first_boundary_index + first_boundary.length(), req.body.length());
    int second_boundary_index = b.find(second_boundary);
    b = b.substr(0, second_boundary_index - 2);  // hack

    string file_data = b;
    bool result = write_file(username, fpath, file_data, client);

    if (result) {
        // send good response
        resp.top_line = "HTTP/1.1 200 OK";
        resp.headers.insert(make_pair("Date", get_timestamp()));
        resp.headers.insert(make_pair("Content-Type", "application/json"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Credentials",
            "true"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Origin",
            req.headers["Origin"]));
    } else {
        resp.top_line = "HTTP/1.1 404";
        resp.headers.insert(make_pair("Date", get_timestamp()));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Credentials",
            "true"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Origin",
            req.headers["Origin"]));
    }

    resp.body = string("{}");
    resp.headers.insert(make_pair("Content-Length", to_string(resp.body.size())));
    auto s = serialize_http_response(resp);
    write_message(client_fd, s);

    return result;
}

void handle_write_email(int client_fd, http_message req) {
    string timestamp = get_timestamp();
    http_message resp;
    auto user_sid_pair = username_sid_from_request(req);
    string username = user_sid_pair.first;
    string sid = user_sid_pair.second;

    MasterBackendClient master_client(
        grpc::CreateChannel(
            "localhost:5000",
            grpc::InsecureChannelCredentials()));

    string channel = master_client.which_node_call(username);
    KeyValueClient client(
        grpc::CreateChannel(
            channel,
            grpc::InsecureChannelCredentials()));

    bool user_authorized = verify_user(username, sid, client);
    if (!user_authorized) {
        resp.top_line = "HTTP/1.1 401 Unauthorized";
        resp.headers.insert(make_pair("Date", timestamp));
        resp.headers.insert(make_pair("Content-Type", "application/json"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Credentials",
            "true"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Origin",
            req.headers["Origin"]));
        resp.body = string("{}");
        resp.headers.insert(make_pair("Content-Length",
                                      to_string(resp.body.size())));
        auto s = serialize_http_response(resp);
        write_message(client_fd, s);
        return;
    }

    // Data received from the client
    Document d;
    d.Parse(req.body.c_str());
    string recipient = d["recipient"].GetString();
    string subject = d["subject"].GetString();

    // From <sender> <timestamp>
    char email_header[BUFFER_SIZE + 10];
    sprintf(email_header, "From <%s> <%s>\r\n", username.c_str(), mail_timestamp().c_str());
    string email_body = string(email_header);

    // To: sender
    sprintf(email_header, "To: <%s>\r\n", recipient.c_str());
    email_body.append(string(email_header));

    // From: name <sender>
    sprintf(email_header, "From: <%s>\r\n", username.c_str());
    email_body.append(string(email_header));

    // Subject: subject
    sprintf(email_header, "Subject: %s\r\n", subject.c_str());
    email_body.append(string(email_header));

    // Message-ID: <id>
    sprintf(email_header, "Message-ID: <%s%s>\r\n", username.c_str(), mail_timestamp().c_str());
    email_body.append(string(email_header));

    // Data: date
    sprintf(email_header, "Date: %s\r\n", mail_timestamp().c_str());
    email_body.append(string(email_header));

    // User-Agent: something
    sprintf(email_header, "User-Agent: penn-cloud-http-server\r\n");
    email_body.append(string(email_header));

    // MIME-Version: something
    sprintf(email_header, "MIME-Version: 1.0\r\n");
    email_body.append(string(email_header));

    sprintf(email_header, "Content-Type: text/plain; charset=utf-8; format=flowed\r\n");
    email_body.append(string(email_header));

    sprintf(email_header, "Content-Transfer-Encoding: 7bit\r\n");
    email_body.append(string(email_header));

    sprintf(email_header, "Content-Language: en-US\r\n\r\n");
    email_body.append(string(email_header));

    // email_body.append(d["subject"].GetString());
    // email_body.append("\r\n");
    email_body.append(d["body"].GetString());

    init_emails_metadata(username, client);
    bool result = write_email(recipient, username, email_body, client);

    if (result) {
        // send good response
        resp.top_line = "HTTP/1.1 200 OK";
        resp.headers.insert(make_pair("Date", timestamp));
        resp.headers.insert(make_pair("Access-Control-Allow-Origin", req.headers["Origin"]));
        resp.headers.insert(make_pair("Access-Control-Allow-Credentials", "true"));
        resp.headers.insert(make_pair("Content-Length", "0"));
    } else {
        resp.top_line = "HTTP/1.1 404 Not found";
        resp.headers.insert(make_pair("Date", timestamp));
        resp.headers.insert(make_pair("Access-Control-Allow-Credentials", "true"));
        resp.headers.insert(make_pair("Access-Control-Allow-Origin", req.headers["Origin"]));
        resp.headers.insert(make_pair("Content-Length", "0"));
        // Send bad response
    }

    resp.body = "{}";
    resp.headers.insert(make_pair("Content-Length", to_string(resp.body.size())));
    auto s = serialize_http_response(resp);
    write_message(client_fd, s);

    // if (debug_mode) {
    //     fprintf(stderr, "[%d] S: %s", client_fd, s.c_str());
    // }
}

void handle_delete_email(int client_fd, http_message req) {
    string timestamp = get_timestamp();
    http_message resp;
    auto user_sid_pair = username_sid_from_request(req);
    string username = user_sid_pair.first;
    string sid = user_sid_pair.second;

    MasterBackendClient master_client(
        grpc::CreateChannel(
            "localhost:5000",
            grpc::InsecureChannelCredentials()));

    string channel = master_client.which_node_call(username);
    KeyValueClient client(
        grpc::CreateChannel(
            channel,
            grpc::InsecureChannelCredentials()));

    bool user_authorized = verify_user(username, sid, client);
    if (!user_authorized) {
        resp.top_line = "HTTP/1.1 401 Unauthorized";
        resp.headers.insert(make_pair("Date", get_timestamp()));
        resp.headers.insert(make_pair("Content-Type", "application/json"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Credentials",
            "true"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Origin",
            req.headers["Origin"]));
        resp.body = string("{}");
        resp.headers.insert(make_pair("Content-Length",
                                      to_string(resp.body.size())));
        auto s = serialize_http_response(resp);
        write_message(client_fd, s);
        return;
    }

    init_emails_metadata(username, client);

    Document d;
    d.Parse(req.body.c_str());
    const Value& hashes = d["hashes"];
    for (SizeType i = 0; i < hashes.Size(); ++i) {
        auto hash = hashes[i].GetString();
        if (!delete_email_from_hash(username, hash, client)) {
            // TODO: Send bad response
            return;
        }
    }

    resp.top_line = "HTTP/1.1 200 OK";
    resp.headers.insert(make_pair("Date", timestamp));
    resp.headers.insert(make_pair("Content-Type", "application/json"));
    resp.headers.insert(make_pair("Access-Control-Allow-Origin", req.headers["Origin"]));
    resp.headers.insert(make_pair("Access-Control-Allow-Credentials", "true"));
    resp.body = "{\"hi\": \"sup\"}";
    resp.headers.insert(
        make_pair("Content-Length", to_string(resp.body.size())));

    auto s = serialize_http_response(resp);
    write_message(client_fd, s);
}

void* ping_nodes(void* argument_pointer) {
    map<string, int> node_to_misses;
    while (keep_running) {
        for (auto& arg : nodes_to_status) {
            string node_full_str = arg.first;
            int port = stoi(separate_string(node_full_str, ":")[2]);

            int sockfd = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in frontend_addr;
            bzero(&frontend_addr, sizeof(frontend_addr));
            frontend_addr.sin_family = AF_INET;
            frontend_addr.sin_port = htons(port);
            inet_pton(AF_INET, "127.0.0.1", &(frontend_addr.sin_addr));
            int error = connect(sockfd,
                                (struct sockaddr*)&frontend_addr,
                                sizeof(frontend_addr));

            if (error) {
                if (debug_mode) {
                    // fprintf(stderr, "[%d] Ping bad for %s\n",
                    //         sockfd, arg.first.c_str());
                }
                node_to_misses[arg.first] += 1;
                // TODO: get rid of magic number
                if (node_to_misses[arg.first] > 3) {
                    arg.second = false;
                }
                close(sockfd);
                continue;
            }

            write_message(sockfd, string("master\r\n\r\n"));

            int read_result = 1;
            char recv_buffer[BUFFER_SIZE];
            bzero(recv_buffer, BUFFER_SIZE);
            read_result = read_message(sockfd, recv_buffer);

            if (strlen(recv_buffer)) {
                if (debug_mode) {
                    //fprintf(stderr, "[%d] Ping OK\n", sockfd);
                }
                node_to_misses[arg.first] = 0;
                arg.second = true;
            } else {
                if (debug_mode) {
                    fprintf(stderr, "[%d] Ping bad for %s\n", sockfd, arg.first.c_str());
                    fprintf(stderr, "[%d] %s\n", sockfd, recv_buffer);
                }
                node_to_misses[arg.first] += 1;
                // TODO: get rid of magic number
                if (node_to_misses[arg.first] > 3) {
                    arg.second = false;
                }
            }
            if (read_result == 0) {
                if (debug_mode) {
                    fprintf(stderr, "[%d] Ping Connection closed\n", sockfd);
                }
                arg.second = false;
            }

            close(sockfd);
        }
        struct timespec ts;
        ts.tv_sec = 1;
        ts.tv_nsec = 500000;  // 500ms
        nanosleep(&ts, &ts);
    }
    return 0;
}

// dont need to be logged in to do this
void handle_change_password(int client_fd, http_message req) {
    string timestamp = get_timestamp();
    http_message resp;

    Document d;
    d.Parse(req.body.c_str());
    string username_for_change = d["username"].GetString();
    string password_to_change_to = d["password"].GetString();

    MasterBackendClient master_client(
        grpc::CreateChannel(
            "localhost:5000",
            grpc::InsecureChannelCredentials()));

    string channel = master_client.which_node_call(username_for_change);
    KeyValueClient client(
        grpc::CreateChannel(
            channel,
            grpc::InsecureChannelCredentials()));

    int change_password_result = change_password(username_for_change, password_to_change_to, client);

    Document outDoc;
    outDoc.SetObject();
    Document::AllocatorType& allocator = outDoc.GetAllocator();
    outDoc.AddMember("response_code", change_password_result, allocator);
    StringBuffer sb;
    Writer<StringBuffer> writer(sb);
    outDoc.Accept(writer);
    resp.body = string(sb.GetString());

    cout << "username: " << username_for_change << endl;
    cout << "password: " << password_to_change_to << endl;

    resp.top_line = "HTTP/1.1 200 OK";
    resp.headers.insert(make_pair("Date", timestamp));
    resp.headers.insert(make_pair("Content-Type", "application/json"));
    resp.headers.insert(make_pair("Access-Control-Allow-Origin", req.headers["Origin"]));
    resp.headers.insert(make_pair("Access-Control-Allow-Credentials", "true"));
    resp.headers.insert(make_pair("Content-Length", to_string(resp.body.size())));
    auto s = serialize_http_response(resp);
    write_message(client_fd, s);
}

void handle_get_username(int client_fd, http_message req) {
    string timestamp = get_timestamp();
    http_message resp;
    auto user_sid_pair = username_sid_from_request(req);
    string username = user_sid_pair.first;
    string sid = user_sid_pair.second;

    MasterBackendClient master_client(
        grpc::CreateChannel(
            "localhost:5000",
            grpc::InsecureChannelCredentials()));

    string channel = master_client.which_node_call(username);
    KeyValueClient client(
        grpc::CreateChannel(
            channel,
            grpc::InsecureChannelCredentials()));

    bool user_authorized = verify_user(username, sid, client);
    if (!user_authorized) {
        resp.top_line = "HTTP/1.1 401 Unauthorized";
        resp.headers.insert(make_pair("Date", get_timestamp()));
        resp.headers.insert(make_pair("Content-Type", "application/json"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Credentials",
            "true"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Origin",
            req.headers["Origin"]));
        resp.body = string("{}");
        resp.headers.insert(make_pair("Content-Length",
                                      to_string(resp.body.size())));
        auto s = serialize_http_response(resp);
        write_message(client_fd, s);
        return;
    }

    if (username == "") {
        resp.top_line = "HTTP/1.1 401 Unauthorized";
        resp.headers.insert(make_pair("Date", get_timestamp()));
        resp.headers.insert(make_pair("Content-Type", "application/json"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Credentials",
            "true"));
        resp.headers.insert(make_pair(
            "Access-Control-Allow-Origin",
            req.headers["Origin"]));
        resp.body = string("{}");
        resp.headers.insert(make_pair("Content-Length",
                                      to_string(resp.body.size())));
        auto s = serialize_http_response(resp);
        write_message(client_fd, s);
        return;
    }

    string body = "{\"username\": \"";
    body.append(username);
    body.append("\"}");

    resp.top_line = "HTTP/1.1 200 OK";
    resp.headers.insert(make_pair("Date", timestamp));
    resp.headers.insert(make_pair("Content-Type", "application/json"));
    resp.headers.insert(make_pair("Access-Control-Allow-Origin", req.headers["Origin"]));
    resp.headers.insert(make_pair("Access-Control-Allow-Credentials", "true"));
    resp.body = body;
    resp.headers.insert(make_pair("Content-Length", to_string(resp.body.size())));

    if (req.request_type == HEAD) {
      resp.body = "";
    }

    auto s = serialize_http_response(resp);
    write_message(client_fd, s);
}

void* redirect_client(void* argument_pointer) {
    http_message resp;
    redir_thread_args* targs = static_cast<redir_thread_args*>(argument_pointer);
    int client_fd = targs->client_fd;
    string connecting_ip_addr = targs->connecting_ip_addr;

    // Parse the header
    char recv_buffer[BUFFER_SIZE];
    bzero(recv_buffer, BUFFER_SIZE);

    bool check_nodes = true;
    bool should_toggle = false;
    int read_result = 1;
    http_message req;
    req.body_length = 0;
    // Read the request line-by-line
    while (read_result > 0 && keep_running) {
        bzero(recv_buffer, BUFFER_SIZE);
        read_result = read_message(client_fd, recv_buffer);
        string header = string(recv_buffer);
        if (read_result < 0) {
            // Assume the client is closed?
            close(client_fd);
            return 0;
        }

        if (read_result == 3 && strlen(recv_buffer) == 0) {
            // process the request body, body length is set when parsing

            if (!should_toggle) {
                boost::hash<std::string> string_hash;
                size_t h = string_hash(connecting_ip_addr);

                vector<string> usable_ports;
                for (auto list : nodes_to_status) {
                    if (list.second) {
                        usable_ports.emplace_back(list.first);
                    }
                }

                int location_index = h % usable_ports.size();
                string location = usable_ports.at(location_index);

                cout << "REQ ROUTE: " << req.route << endl;

                location.append(req.route);
                resp.top_line = "HTTP/1.1 307 Temporary Redirect";

                resp.headers.insert(make_pair("Date", get_timestamp()));
                resp.headers.insert(make_pair("Location", location));
                resp.headers.insert(make_pair("Cache-control", "max-age=20"));
                resp.headers.insert(make_pair("Access-Control-Allow-Origin",
                                              req.headers["Origin"]));
                resp.headers.insert(make_pair("Access-Control-Allow-Credentials", "true"));
                resp.body = location;
                resp.headers.insert(make_pair("Content-Length", to_string(location.size())));

                auto s = serialize_http_response(resp);
                write_message(client_fd, s.c_str());
                // if (debug_mode) {
                //     fprintf(stderr, "[%d] S: %s", client_fd, s.c_str());
                // }
            }

            string body = "";
            if (req.body_length > 0) {
                int amount_read = 0;
                while (amount_read < req.body_length) {
                    bzero(recv_buffer, BUFFER_SIZE);
                    int read_res_amount = read(client_fd, recv_buffer, 50000);
                    if (read_res_amount <= 0) break;
                    amount_read += read_res_amount;
                    string body_to_append = string(recv_buffer, read_res_amount);
                    body.append(body_to_append);
                }
            }
            req.body = body;
            break;
        }
        if (check_nodes) {
            // Check to see if this is a frontend node trying to get admin data
            if (header.find("node_status") != string::npos) {
                send_node_status(client_fd, nodes_to_status, req);
                close(client_fd);
                return 0;
            }
            // Check if this is a frontend node trying to toggle frontend node
            should_toggle = header.find("toggle_node") != string::npos;
            check_nodes = false;
        }

        parse_http_header(header, req);
    }
    if (read_result == 0) {
        if (debug_mode) {
            fprintf(stderr, "[%d] Connection closed\n", client_fd);
        }

        close(client_fd);
        return 0;
    }

    if (should_toggle) {
        send_change_status(client_fd, req);
        close(client_fd);
        return 0;
    }

    close(client_fd);

    return 0;
}

void* thread_handle_client(void* argument_pointer) {
    int client_fd = (long)argument_pointer;
    char recv_buffer[BUFFER_SIZE];

    // if (debug_mode) {
    //     fprintf(stderr, "[%d] New connection\n", client_fd);
    // }

    while (true) {
        bool checkPing = true;
        int read_result = 1;
        http_message req;
        req.body_length = 0;
        // Read the request line-by-line
        while (read_result && keep_running) {
            bzero(recv_buffer, BUFFER_SIZE);
            read_result = read_message(client_fd, recv_buffer);

            string recv = string(recv_buffer);
            if (checkPing) {
                if (recv.find("node_down") != string::npos) {
                    dormant = true;
                    if (debug_mode) {
                        fprintf(stderr, "[%d] Received 'node_down' signal. Dormant\n", client_fd);
                    }
                    close(client_fd);
                } else if (recv.find("node_up") != string::npos) {
                    dormant = false;
                    if (debug_mode) {
                        fprintf(stderr, "[%d] Received 'node_up' signal. Not dormant\n", client_fd);
                    }
                    close(client_fd);
                } else if (!dormant && recv.find("master") != string::npos) {
                    string s = "200\r\n\r\n";
                    write_message(client_fd, s);
                    if (debug_mode) {
                        // fprintf(stderr, "[%d] S: %s", client_fd, s.c_str());
                    }
                    close(client_fd);
                    return 0;
                }
                checkPing = false;
            }

            if (read_result < 0) {
                // Assume the client is closed?
                close(client_fd);
                return 0;
            }

            if (read_result == 3 && strlen(recv_buffer) == 0) {
                // process the request body, body length is set when parsing
                string body;
                if (req.body_length > 0) {
                    int amount_read = 0;
                    while (amount_read < req.body_length) {
                        bzero(recv_buffer, BUFFER_SIZE);
                        // int read_res_amount = read(client_fd, recv_buffer, req.body_length - amount_read);
                        int read_res_amount = read(client_fd, recv_buffer, 50000);
                        if (read_res_amount == 0) break;
                        amount_read += read_res_amount;
                        string body_to_append = string(recv_buffer, read_res_amount);
                        body.append(body_to_append);
                    }
                }
                req.body = body;
                break;
            }

            string header = recv;
            parse_http_header(header, req);
            // fprintf(stderr, "[%d] C: %s\n", client_fd, recv_buffer);
        }

        if (dormant) {
            close(client_fd);
            return 0;
        }

        if (read_result == 0) {
            if (debug_mode) {
                fprintf(stderr, "[%d] Connection closed\n", client_fd);
            }

            close(client_fd);
            return 0;
        }

        if (!keep_running) {
            string message("Server shutting down\r\n");
            write_message(client_fd, message);
            if (debug_mode) {
                // fprintf(stderr, "[%d] S: %s", client_fd, message.c_str());
            }
            close(client_fd);
            return 0;
        }

        http_message resp;
        if (req.request_type == GET || req.request_type == HEAD) {
            auto split_on_question_mark = separate_string(req.route, "?");
            string route_prefix = separate_string(req.route, "?")[0];

            cout << "ROUTE PREFIX: " << route_prefix << endl;

            if (route_prefix == "/") {
                // Set the sid cookie
                handle_new_connection(client_fd, req);
                continue;
            } else if (route_prefix == "/emails") {
                // Get hash list and then get range of emails
                get_emails(client_fd, req);
                continue;
            } else if (route_prefix == "/sent_emails") {
                get_sent_emails(client_fd, req);
                continue;
            } else if (req.route == "/email") {
                // Get a single email from hash in case cache is incorrect
            } else if (split_on_question_mark[0] == "/files") {
                // Get list of subdirs and files in a directory
                string root = split_on_question_mark.size() > 1 ? split_on_question_mark[1] : "/";
                get_files(client_fd, root, req);
            } else if (route_prefix == "/file") {
                // Download the content of a single file
                get_file(client_fd, req);
                continue;
            } else if (split_on_question_mark[0] == "/shared_files") {
                // Get list of subdirs and files in a directory
                get_shared_files(client_fd, req);
            } else if (route_prefix == "/shared_file") {
                // Download the content of a single file
                get_shared_file(client_fd, req);
                continue;
            } else if (route_prefix == "/share_file") {
                // Download the content of a single file
                handle_share_file(client_fd, req);
                continue;
            } else if (route_prefix == "/admin") {
                request_node_status(client_fd, req);
                continue;
            } else if (route_prefix == "/rowsData") {
                get_admin_rows(client_fd, req);
                continue;
            } else if (route_prefix == "/cellData") {
                get_cell_data(client_fd, req);
                continue;
            } else if (route_prefix == "/create_dir") {
                handle_create_dir(client_fd, req, split_on_question_mark.size() > 1 ? split_on_question_mark[1] : "/");
            } else if (route_prefix == "/delete_file") {
                if (split_on_question_mark.size()) {
                    handle_delete_file(client_fd, req, split_on_question_mark[1]);
                } else {
                    cout << "no file specified to delete\n";
                }
            } else if (route_prefix == "/delete_dir") {
              handle_delete_dir(client_fd, req, split_on_question_mark[1]);
            } else if (route_prefix == "/get_username") {
                handle_get_username(client_fd, req);
            }
        } else if (req.request_type == POST) {
            auto split_on_question_mark = separate_string(req.route, "?");
            string route_prefix = split_on_question_mark[0];

            cout << "POST: " << route_prefix << endl;

            if (req.route == "/signin") {
                // Sign a user in
                handle_login(client_fd, req);
                continue;
            } else if (req.route == "/account") {
                // Create a user account
                handle_registration(client_fd, req);
                continue;
            } else if (req.route == "/compose") {
                // Write email
                handle_write_email(client_fd, req);
                continue;
            } else if (req.route == "/delete_email") {
                // Delete email
                handle_delete_email(client_fd, req);
                continue;
            } else if (req.route == "/node_toggle") {
                change_node_status(client_fd, req);
                continue;
            } else if (route_prefix == "/upload_file") {
                // Upload file
                cerr << "received by 8001" << endl;
                cerr << "body: " << req.body << endl;
                handle_upload_file(client_fd,
                                   req,
                                   split_on_question_mark.size() > 1 ? split_on_question_mark[1] : "/");
                continue;
            } else if (req.route == "/move_file") {
                handle_move_file(client_fd, req);
            } else if (route_prefix == "/rename_dir") {
                cout << "HERE 1848" << endl;
                handle_rename_dir(client_fd, req);
            } else if (req.route == "/change_password") {
                handle_change_password(client_fd, req);
            }
        }
    }
}

int main(int argc, char* argv[]) {
    // Clear signal mask so blocking system calls don't restart when interrupted
    struct sigaction sa = {0};
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    // argument parsing
    int port_number, smtp_port_number, pop3_port_number;
    string known_nodes_file = parse_args(argc, argv, &port_number, &smtp_port_number, &pop3_port_number);

    pthread_t smtp_thread, pop3_thread;
    if (smtp_port_number) {
        pthread_create(&smtp_thread, NULL, run_smtp_server, (void*)(long)smtp_port_number);
    }

    if (pop3_port_number) {
        pthread_create(&pop3_thread, NULL, run_pop3_server, (void*)(long)pop3_port_number);
    }

    if (master_node) {
        // Read the redirect ports available
        std::ifstream ifs(known_nodes_file.c_str(), ifstream::in);
        string line;

        // Read the config file
        getline(ifs, line);
        while (ifs) {
            string known_ip_port = line;
            nodes_to_status[line] = false;

            getline(ifs, line);
        }
    }

    // set up socket
    int socket_fd, client_fd;
    struct sockaddr_in address;
    int addrsize = sizeof(address);
    address.sin_family = AF_INET;
    address.sin_port = htons(port_number);
    address.sin_addr.s_addr = INADDR_ANY;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));  // make socket reusable

    int bind_result = bind(socket_fd, (struct sockaddr*)&address, sizeof(address));
    if (bind_result < 0) {
        perror("bind");
        exit(-1);
    }

    int listen_result = listen(socket_fd, MAX_NUM_CONNECTIONS);
    if (listen_result < 0) {
        perror("listen");
        exit(-1);
    }

    if (debug_mode) {
        fprintf(stderr, "Listening on port %d\n", port_number);
    }

    // set up threading
    vector<pthread_t*> threads_vector;
    pthread_t* thread;

    if (master_node) {
        thread = (pthread_t*)malloc(sizeof(pthread_t*));
        threads_vector.push_back(thread);
        pthread_create(thread, NULL, ping_nodes, NULL);
    }

    while (keep_running) {
        bzero(&address, sizeof(address));
        client_fd = accept(socket_fd, (struct sockaddr*)&address, (socklen_t*)&addrsize);
        if (client_fd < 0 && errno == 4) {
            break;  // interuption
        } else if (client_fd < 0) {
            perror("accept");
            exit(-1);
        }

        thread = (pthread_t*)malloc(sizeof(pthread_t*));
        threads_vector.push_back(thread);

        if (master_node) {
            char port_char[100];
            int connecting_port = ntohs(address.sin_port);
            sprintf(port_char,
                    "%s:%d",
                    inet_ntoa(address.sin_addr),
                    connecting_port);
            string connecting_ip_addr = string(port_char);

            redir_thread_args targs;
            targs.client_fd = client_fd;
            targs.connecting_ip_addr = connecting_ip_addr;
            pthread_create(thread, NULL, redirect_client, &targs);
            continue;
        } else {
            pthread_create(thread, NULL, thread_handle_client, (void*)(long)client_fd);
        }
    }

    // clear memory
    for (unsigned long i = 0; i < threads_vector.size(); i++) {
        pthread_kill(*(threads_vector.at(i)), SIGINT);
        pthread_join(*(threads_vector.at(i)), NULL);
        free(threads_vector.at(i));
    }
    threads_vector.clear();

    if (smtp_port_number) {
        pthread_kill(smtp_thread, SIGINT);
        pthread_join(smtp_thread, NULL);
    }

    if (pop3_port_number) {
        pthread_kill(pop3_thread, SIGINT);
        pthread_join(pop3_thread, NULL);
    }

    return 0;
}
