#include "mail.h"

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <grpcpp/grpcpp.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <resolv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <memory>

#include "common_mail_files.h"

#ifdef BAZEL_BUILD
#include "kvstore/protos/kvstore.grpc.pb.h"
#else
#include "kvstore.grpc.pb.h"
#endif

using grpc::Channel;
using grpc::ClientContext;
using grpc::InsecureChannelCredentials;
using grpc::Status;

using kvstore::CPutRequest;
using kvstore::DeleteRequest;
using kvstore::GetRequest;
using kvstore::KeyValueStore;
using kvstore::PutRequest;

using kvstore::CPutReply;
using kvstore::DeleteReply;
using kvstore::GetReply;
using kvstore::PutReply;
using kvstore::ResponseCode;

using namespace std;

bool init_emails_metadata(string username, KeyValueClient &client) {
    PutReply put_response;
    GetReply get_response = client.get_call(username, EMAIL_METADATA_COLUMN_NAME);
    GetReply get_response_sent = client.get_call(username, EMAIL_SENT_METADATA_COLUMN_NAME);

    if (get_response.response() == kvstore::ResponseCode::MISSING_KEY) {
        put_response = client.put_call(username, EMAIL_METADATA_COLUMN_NAME, ";");
    }

    if (get_response_sent.response() == kvstore::ResponseCode::MISSING_KEY) {
        put_response = client.put_call(username, EMAIL_SENT_METADATA_COLUMN_NAME, ";");
    }

    bool ret_val = get_response.response() != kvstore::ResponseCode::MISSING_KEY && get_response_sent.response() != kvstore::ResponseCode::MISSING_KEY;
    return ret_val;
}

string mail_timestamp() {
    char time_buffer[1000] = {0};
    time_t t = time(NULL);
    struct tm time_struct = *localtime(&t);

    sprintf(time_buffer, "<%02d/%02d/%02d-%02d:%02d:%02d>",
            time_struct.tm_mon + 1,
            time_struct.tm_mday,
            time_struct.tm_year + 1900,
            time_struct.tm_hour,
            time_struct.tm_min,
            time_struct.tm_sec);

    string s = string(time_buffer);
    return s;
}

// common between front-end and smtp
int write_email(string recipient,
                string sender,
                string email_body,
                KeyValueClient &client) {
    string recipient_domain = get_domain(recipient);

    if (recipient_domain == LOCAL_DOMAIN) {
        init_emails_metadata(recipient, client);
    }

    init_emails_metadata(sender, client);

    boost::uuids::basic_random_generator<boost::mt19937> gen;
    string message_hash_column = boost::uuids::to_string(gen());
    PutReply put_response;

    if (recipient_domain == LOCAL_DOMAIN) {
        auto master_client = MasterBackendClient(grpc::CreateChannel("localhost:5000", grpc::InsecureChannelCredentials()));
        string channel = master_client.which_node_call(recipient);
        KeyValueClient recipient_client(grpc::CreateChannel(channel, grpc::InsecureChannelCredentials()));
        put_response = recipient_client.put_call(recipient, message_hash_column, email_body);
        if (put_response.response() == ResponseCode::FAILURE) {
            fprintf(stderr, "1st put failed\n");
            return 0;
        }
        append_to_metadata(recipient, EMAIL_METADATA_COLUMN_NAME, message_hash_column, recipient_client);
    } else {
        // smtp send external
        cout << "SENDING EXTERNAL: " << send_email_external(sender, recipient, email_body) << endl;
    }

    // log for sender
    put_response = client.put_call(sender, message_hash_column, email_body);
    if (put_response.response() == ResponseCode::FAILURE) {
        fprintf(stderr, "1st put failed\n");
        return 0;
    }
    append_to_metadata(sender, EMAIL_SENT_METADATA_COLUMN_NAME, message_hash_column, client);

    return 1;
}

string list_email_hashes(string username, KeyValueClient &client) {
    init_emails_metadata(username, client);
    string metadata_column_name(EMAIL_METADATA_COLUMN_NAME);

    GetReply get_response = client.get_call(username, metadata_column_name);
    if (get_response.error().size()) {
        return "";
    }

    string metadata = get_response.value();
    return metadata;
}

string list_sent_email_hashes(string username, KeyValueClient &client) {
    init_emails_metadata(username, client);
    string metadata_column_name(EMAIL_SENT_METADATA_COLUMN_NAME);

    GetReply get_response = client.get_call(username, metadata_column_name);
    if (get_response.error().size()) {
        return "";
    }

    string metadata = get_response.value();
    return metadata;
}

string get_email_from_hash(string username, string hash, KeyValueClient &client) {
    init_emails_metadata(username, client);
    GetReply get_response = client.get_call(username, hash);
    if (get_response.error().size()) {
        return "";
    }

    string email = get_response.value();
    return email;
}

int delete_email_from_hash(string username, string hash, KeyValueClient &client) {
    init_emails_metadata(username, client);
    string metadata_column_name(EMAIL_METADATA_COLUMN_NAME);

    GetReply get_response = client.get_call(username, metadata_column_name);
    if (get_response.error().size()) {
        return 0;
    }

    string metadata = get_response.value();
    string new_metadata = delete_hash_from_metadata_string(metadata, hash);
    cput_loop(username, metadata_column_name, new_metadata, client);

    DeleteReply delete_response = client.delete_call(username, hash);
    if (delete_response.error().size()) {
        return 0;
    }

    return 1;
}

int get_num_messages(string username, KeyValueClient &client) {
    string email_hashes = list_email_hashes(username, client);
    auto hashes_vec = separate_string(email_hashes, ";");
    return hashes_vec.size();
}

int get_maildrop_size(string username, KeyValueClient &client) {
    int size = 0;
    string email_hashes = list_email_hashes(username, client);
    auto hashes_vec = separate_string(email_hashes, ";");

    for (unsigned long i = 0; i < hashes_vec.size(); i++) {
        string hash = hashes_vec[i];
        GetReply get_response = client.get_call(username, hash);
        if (get_response.response() == kvstore::ResponseCode::SUCCESS) {
            string email_data = get_response.value();
            size += email_data.length();
        }
    }

    return size;
}

// external email code:

string get_domain(string address) {
    int at_index = address.find("@");
    if (at_index == -1) {
        return "";
    }
    return address.substr(at_index + 1, address.length());
}

string get_before_domain(string address) {
    int at_index = address.find("@");
    if (at_index == -1) {
        return address;
    }
    return address.substr(0, at_index);
}

int write_message_external_email(int fd, string buffer) {
    return send(fd, buffer.c_str(), buffer.length(), 0);
}

int read_message_external_email(int fd, char *buffer) {
    int read_result;
    for (int j = 0; j < EXTERNAL_EMAIL_BUFFER_SIZE; buffer[j++] = 0)
        ;

    for (int i = 0; i < EXTERNAL_EMAIL_BUFFER_SIZE; i++) {
        read_result = read(fd, buffer + i, 1);
        if (read_result == 0) {
            return 0;
        } else if (read_result == -1 && errno == 4) {
            return 2;
        } else if (i > 1 && buffer[i - 1] == '\r' && buffer[i] == '\n') {
            buffer[i - 1] = 0;
            return 1;
        } else if (i == 1 && buffer[i - 1] == '\r' && buffer[i] == '\n') {
            bzero(buffer, sizeof(buffer));
            return 3;
        }
    }
    return -1;
}

bool send_email_external(string sender_username, string recipient_address,
                         string email_body) {
    u_char nsbuf[4096];
    char dispbuf[4096];
    ns_msg msg;
    ns_rr rr;
    int i, j, l;
    string mx_host, smtp_ip;

    string domain = get_domain(recipient_address);
    l = res_query(domain.c_str(), ns_c_in, ns_t_mx, nsbuf, sizeof(nsbuf));

    if (l < 0) {
        perror("res_query");
        return false;
    }

    ns_initparse(nsbuf, l, &msg);
    l = ns_msg_count(msg, ns_s_an);

    for (j = 0; j < l; j++) {
        ns_parserr(&msg, ns_s_an, j, &rr);
        ns_sprintrr(&msg, &rr, NULL, NULL, dispbuf, sizeof(dispbuf));
        auto s = string(dispbuf);
        vector<string> v = separate_string(s, " ");
        mx_host = *(v.rbegin());
        break;
    }

    l = res_query(mx_host.c_str(), ns_c_in, ns_t_a, nsbuf, sizeof(nsbuf));
    ns_initparse(nsbuf, l, &msg);
    l = ns_msg_count(msg, ns_s_an);
    for (j = 0; j < l; j++) {
        ns_parserr(&msg, ns_s_an, j, &rr);
        ns_sprintrr(&msg, &rr, NULL, NULL, dispbuf, sizeof(dispbuf));
        auto s = string(dispbuf);
        vector<string> v = separate_string(s, " ");
        smtp_ip = v.rbegin()->c_str();
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in frontend_addr;
    bzero(&frontend_addr, sizeof(frontend_addr));
    frontend_addr.sin_family = AF_INET;
    frontend_addr.sin_port = htons(25);
    inet_pton(AF_INET, smtp_ip.c_str(), &(frontend_addr.sin_addr));
    int con_res = connect(sockfd, (struct sockaddr *)&frontend_addr, sizeof(frontend_addr));

    if (con_res < 0) {
        perror("connect");
        return false;
    }

    char buf[EXTERNAL_EMAIL_BUFFER_SIZE];
    bzero(buf, EXTERNAL_EMAIL_BUFFER_SIZE);
    write_message_external_email(sockfd, "HELO penncloud.com\r\n");

    bzero(buf, EXTERNAL_EMAIL_BUFFER_SIZE);
    read_message_external_email(sockfd, buf);
    fprintf(stderr, "buf: %s\n", buf);

    bzero(buf, EXTERNAL_EMAIL_BUFFER_SIZE);
    sprintf(buf, "MAIL FROM:<%s@penncloud.com>\r\n", get_before_domain(sender_username).c_str());
    write_message_external_email(sockfd, buf);

    bzero(buf, EXTERNAL_EMAIL_BUFFER_SIZE);
    read_message_external_email(sockfd, buf);
    fprintf(stderr, "buf: %s\n", buf);

    bzero(buf, EXTERNAL_EMAIL_BUFFER_SIZE);
    sprintf(buf, "RCPT TO:<%s>\r\n", recipient_address.c_str());
    write_message_external_email(sockfd, buf);

    bzero(buf, EXTERNAL_EMAIL_BUFFER_SIZE);
    read_message_external_email(sockfd, buf);
    fprintf(stderr, "buf: %s\n", buf);

    write_message_external_email(sockfd, "DATA\r\n");

    bzero(buf, EXTERNAL_EMAIL_BUFFER_SIZE);
    read_message_external_email(sockfd, buf);
    fprintf(stderr, "buf: %s\n", buf);

    bzero(buf, EXTERNAL_EMAIL_BUFFER_SIZE);
    sprintf(buf, "%s\r\n.\r\n", email_body.c_str());
    write_message_external_email(sockfd, buf);

    fprintf(stderr, "buf: %s\n", buf);

    return true;
}

//
