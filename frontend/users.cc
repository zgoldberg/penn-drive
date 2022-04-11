#include "users.h"

using namespace std;
using namespace rapidjson;

void handle_new_connection(int client_fd, http_message &req) {
    http_message resp;

    resp.top_line = "HTTP/1.1 200 OK";
    resp.headers.insert(make_pair("Date", get_timestamp()));
    resp.headers.insert(make_pair("Content-Type", "application/json"));
    resp.headers.insert(make_pair(
        "Access-Control-Allow-Credentials",
        "true"));
    resp.headers.insert(make_pair(
        "Access-Control-Allow-Origin",
        req.headers["Origin"]));

    resp.body = "{ \"auth\": \"true\"}";

    auto sid_iter = req.cookies.find("sid");
    if (sid_iter == req.cookies.end()) {
        // Put something in the body to indicate user isn't logged in
        resp.body = "{ \"auth\": \"false\"}";
    }

    auto username_iter = req.cookies.find("username");
    if (username_iter == req.cookies.end()) {
        resp.body = "{ \"auth\": \"false\"}";
    } else {
        string username = req.cookies["username"];
        MasterBackendClient master_client(
            grpc::CreateChannel(
                "localhost:5000",
                grpc::InsecureChannelCredentials()));
        string channel = master_client.which_node_call(username);
        KeyValueClient client(
            grpc::CreateChannel(
                channel,
                grpc::InsecureChannelCredentials()));

        GetReply get_response = client.get_call(username, string("sids"));
        if (get_response.response() != ResponseCode::SUCCESS) {
            resp.body = "{ \"auth\": \"false\"}";
        } else if (get_response.value().find("sid") == string::npos) {
            resp.body = "{ \"auth\": \"false\"}";
        }
    }

    resp.headers.insert(
        make_pair("Content-Length", to_string(resp.body.size())));

    auto s = serialize_http_response(resp);
    fprintf(stderr, "S[%d]: %s\n", client_fd, s.c_str());
    write_message(client_fd, s);
}

void handle_registration(int client_fd, http_message &req) {
    Document d;
    d.Parse(req.body.c_str());
    string username = d["username"].GetString();
    string password = d["password"].GetString();
    MasterBackendClient master_client(
        grpc::CreateChannel(
            "localhost:5000",
            grpc::InsecureChannelCredentials()));
    string channel = master_client.which_node_call(username);

    KeyValueClient client(
        grpc::CreateChannel(
            channel,
            grpc::InsecureChannelCredentials()));

    http_message resp;

    GetReply get_response = client.get_call(username, string("password"));
    int exists = (get_response.response() == ResponseCode::SUCCESS);

    if (!exists) {
        boost::uuids::basic_random_generator<boost::mt19937> gen;
        string sid = boost::uuids::to_string(gen());
        resp.cookies.insert(make_pair("username", username));
        resp.cookies.insert(make_pair("sid", sid));

        PutReply put_pwd = client.put_call(username, "password", password);
        PutReply put_metadata = client.put_call(username,
                                                EMAIL_METADATA_COLUMN_NAME,
                                                ";");

        sid.append(";");
        client.put_call(username, "sids", sid);
    }

    resp.top_line = exists ? "HTTP/1.1 401 Unauthorized" : "HTTP/1.1 200 OK";
    resp.headers.insert(make_pair("Date", get_timestamp()));
    resp.headers.insert(make_pair("Content-Type", "application/json"));
    resp.headers.insert(make_pair(
        "Access-Control-Allow-Credentials",
        "true"));
    resp.headers.insert(make_pair(
        "Access-Control-Allow-Origin",
        req.headers["Origin"]));

    resp.body = exists ? "{\"auth\": \"false\"}" : "{\"auth\": \"true\"}";
    resp.headers.insert(
        make_pair("Content-Length", to_string(resp.body.size())));

    auto s = serialize_http_response(resp);
    fprintf(stderr, "S[%d]: %s\n", client_fd, s.c_str());
    write_message(client_fd, s);
}

void handle_login(int client_fd, http_message &req) {
    Document d;
    d.Parse(req.body.c_str());
    string username = d["username"].GetString();
    string password = d["password"].GetString();
    MasterBackendClient master_client(
        grpc::CreateChannel(
            "localhost:5000",
            grpc::InsecureChannelCredentials()));
    string channel = master_client.which_node_call(username);

    KeyValueClient client(
        grpc::CreateChannel(
            channel,
            grpc::InsecureChannelCredentials()));

    GetReply reply_password = client.get_call(username, "password");

    http_message resp;
    resp.top_line = "HTTP/1.1 200 OK";
    resp.body = "{ \"auth\": \"false\"}";

    if (reply_password.value() != password) {
        resp.top_line = "HTTP/1.1 401 Unauthorized";
    } else {
        boost::uuids::basic_random_generator<boost::mt19937> gen;
        string sid = boost::uuids::to_string(gen());
        GetReply get_metadata = client.get_call(username, "sids");
        string sids = get_metadata.value();
        sids.append(";");
        sids.append(sid);

        if (cput_loop(username, "sids", sids, client)) {
            resp.cookies.insert(make_pair("username", username));
            resp.cookies.insert(make_pair("sid", sid));
            resp.body = "{ \"auth\": \"true\"}";
        }
    }

    resp.headers.insert(
        make_pair("Content-Length",
                  to_string(resp.body.size())));
    resp.headers.insert(make_pair("Date", get_timestamp()));
    resp.headers.insert(make_pair("Content-Type", "application/json"));
    resp.headers.insert(make_pair(
        "Access-Control-Allow-Credentials",
        "true"));
    resp.headers.insert(make_pair(
        "Access-Control-Allow-Origin",
        req.headers["Origin"]));

    char send_buffer[BUFFER_SIZE + 10];
    bzero(send_buffer, sizeof(send_buffer));
    auto s = serialize_http_response(resp);
    memcpy(send_buffer, s.c_str(), s.size());
    fprintf(stderr, "S[%d]: %s\n", client_fd, send_buffer);
    write_message(client_fd, send_buffer);
}


int change_password(string username, string new_password, KeyValueClient &client) {
  GetReply reply_password = client.get_call(username, "password");
  if (reply_password.response() == kvstore::ResponseCode::MISSING_KEY) {
    return 1; // no user
  } else if (reply_password.value() == new_password) {
    return 2; // new same as old
  }

  PutReply put_response = client.put_call(username, "password", new_password);
  if (put_response.response() == kvstore::ResponseCode::SUCCESS) {
    return 0; // success
  } else {
    return 3; // error
  }
}
