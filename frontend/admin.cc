#include "admin.h"
#define BUFFER_SIZE 102400

using namespace std;
using namespace rapidjson;

void send_change_status(int client_fd, http_message &req) {
    // Request headers are empty
    http_message resp;
    Document d;
    d.Parse(req.body.c_str());
    string node_id = d["nodeID"].GetString();
    string node_is_alive = d["is_alive"].GetString();
    string node_is_frontend = d["is_frontend"].GetString();

    int port = stoi(separate_string(node_id, ":")[2]);
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in frontend_addr;
    bzero(&frontend_addr, sizeof(frontend_addr));
    frontend_addr.sin_family = AF_INET;
    frontend_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &(frontend_addr.sin_addr));

    connect(sockfd, (struct sockaddr*) &frontend_addr, sizeof(frontend_addr));
    if (node_is_alive == "true") {
        write_message(sockfd, string("node_down\r\n\r\n"));
    } else {
        write_message(sockfd, string("node_up\r\n\r\n"));
    }
}

void send_node_status(int client_fd,
                      const map<string, bool> &nodes_to_status,
                      http_message &req)
{
    http_message resp;
    Document outDoc;
    outDoc.SetObject();
    Value data_array(kArrayType);
    Document::AllocatorType& allocator = outDoc.GetAllocator();

    for (const auto &pair : nodes_to_status) {
        Value objValue;
        objValue.SetObject();
        const char* nodeid = pair.first.c_str();
        const char* alive = pair.second ? "true" : "false";
        const char* frontend = "true";

        Value node_id_val(nodeid, allocator);
        Value frontend_val(frontend, allocator);
        Value alive_val(alive, allocator);

        objValue.AddMember("nodeID", node_id_val, allocator);
        objValue.AddMember("is_frontend", frontend_val, allocator);
        objValue.AddMember("is_alive", alive_val, allocator);
        data_array.PushBack(objValue, allocator);
    }

    MasterBackendClient master_client(
        grpc::CreateChannel(
            "localhost:5000",
            grpc::InsecureChannelCredentials()));
    NodeStatusReply reply = master_client.node_status_call();
    for (int i = 0; i < reply.node_names_size(); i++) {
        Value objValue;
        objValue.SetObject();

        const char* nodeid = reply.node_names(i).c_str();
        const char* alive = reply.nodes_alive(i) ? "true" : "false";
        const char* frontend = "false";

        Value node_id_val(nodeid, allocator);
        Value frontend_val(frontend, allocator);
        Value alive_val(alive, allocator);

        objValue.AddMember("nodeID", node_id_val, allocator);
        objValue.AddMember("is_frontend", frontend_val, allocator);
        objValue.AddMember("is_alive", alive_val, allocator);
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
    resp.headers.insert(make_pair(
        "Access-Control-Allow-Credentials",
        "true"));
    resp.headers.insert(make_pair(
        "Access-Control-Allow-Origin",
        req.headers["Origin"]));
    resp.headers.insert(
        make_pair("Content-Length", to_string(resp.body.size())));

    auto s = serialize_http_response(resp);
    write_message(client_fd, s);
    fprintf(stderr, "S[%d]: %s\n", client_fd, s.c_str());
}

void change_node_status(int client_fd, http_message &req) {
    http_message resp;
    Document d;
    d.Parse(req.body.c_str());
    string node_id = d["nodeID"].GetString();
    string node_is_alive = d["is_alive"].GetString();
    string node_is_frontend = d["is_frontend"].GetString();

    if (node_is_frontend == "true") {
        // Send signal to the master node
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in frontend_addr;
        bzero(&frontend_addr, sizeof(frontend_addr));
        frontend_addr.sin_family = AF_INET;
        frontend_addr.sin_port = htons(8000);
        inet_pton(AF_INET, "127.0.0.1", &(frontend_addr.sin_addr));

        connect(sockfd, (struct sockaddr*) &frontend_addr, sizeof(frontend_addr));
        write_message(sockfd, "toggle_node\r\n");
        write_message(sockfd, serialize_http_response(req));
    } else {
        MasterBackendClient master_client(
            grpc::CreateChannel(
                "localhost:5000",
                grpc::InsecureChannelCredentials()));

        ChangeStatusReply reply;
        if (node_is_alive == "true") {
            reply = master_client.stop_node_call(node_id);
        } else {
            reply = master_client.start_node_call(node_id);
        }
        if (reply.response() != ResponseCode::SUCCESS) {
            resp.top_line = "HTTP/1.1 404 Not Found";
            resp.headers.insert(make_pair("Date", get_timestamp()));
            resp.headers["Access-Control-Allow-Origin"] = req.headers["Origin"];
            resp.headers["Access-Control-Allow-Credentials"] = "true";
            resp.body = string("{}");
            resp.headers.insert(make_pair("Content-Length",
                                          to_string(resp.body.size())));
            auto s = serialize_http_response(resp);
            write_message(client_fd, s);
            return;
        }
    }

    resp.top_line = "HTTP/1.1 200 OK";
    resp.headers.insert(make_pair("Date", get_timestamp()));
    resp.headers["Access-Control-Allow-Origin"] = req.headers["Origin"];
    resp.headers["Access-Control-Allow-Credentials"] = "true";
    resp.body = string("{}");
    resp.headers.insert(make_pair("Content-Length",
                                  to_string(resp.body.size())));
    auto s = serialize_http_response(resp);
    write_message(client_fd, s);
}

void request_node_status(int client_fd, http_message &msg) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in frontend_addr;
    bzero(&frontend_addr, sizeof(frontend_addr));
    frontend_addr.sin_family = AF_INET;
    frontend_addr.sin_port = htons(8000);
    inet_pton(AF_INET, "127.0.0.1", &(frontend_addr.sin_addr));

    connect(sockfd, (struct sockaddr*) &frontend_addr, sizeof(frontend_addr));
    // Ask for the node statuses
    write_message(sockfd, string("node_status\r\n\r\n"));

    // parse into resp
    char recv_buffer[BUFFER_SIZE];
    bzero(recv_buffer, BUFFER_SIZE);
    int read_result = 1;
    http_message resp;
    resp.body_length = 0;
    while (read_result) {
        bzero(recv_buffer, BUFFER_SIZE);
        read_result = read_message(sockfd, recv_buffer);

        if (read_result == 3 && strlen(recv_buffer) == 0) {
            // process the request body, body length is set when parsing
            string body;
            if (resp.body_length) {
                read(sockfd, recv_buffer, resp.body_length);
                body = string(recv_buffer);
            }
            resp.body = body;
            break;
        }

        string header = string(recv_buffer);
        parse_http_header(header, resp);
    }
    resp.headers["Access-Control-Allow-Origin"] = msg.headers["Origin"];
    resp.headers["Access-Control-Allow-Credentials"] = "true";

    if (msg.request_type == HEAD) {
      resp.body = "";
    }

    auto s = serialize_http_response(resp);
    write_message(client_fd, s);
    fprintf(stderr, "S[%d]: %s\n", client_fd, s.c_str());
}

int get_query_params(string route, map<string, string> &map) {
    auto qmark_vec = separate_string(route, "?");
    if (qmark_vec.size() < 2) {
        return -1;
    }
    string key_value_string = qmark_vec[1];

    // break up by '&' symbol
    auto pairs_vec = separate_string(key_value_string, "&");
    for (string pair_str : pairs_vec) {
        auto pair_vec = separate_string(pair_str, "=");
        if (pair_vec.size() < 2) {
            return -1;
        }
        map[pair_vec[0]] = pair_vec[1];
    }

    return 0;
}

void get_admin_rows(int client_fd, http_message &req) {
    http_message resp;
    map<string, string> query_key_to_value;
    int res = get_query_params(req.route, query_key_to_value);
    if (res < 0) {
        // TODO: send 404
    }
    int page_index = stoi(query_key_to_value["pageIndex"]);

    Document outDoc;
    outDoc.SetObject();
    Value data_array(kArrayType);
    Document::AllocatorType& allocator = outDoc.GetAllocator();

    MasterBackendClient master_client(
        grpc::CreateChannel(
            "localhost:5000",
            grpc::InsecureChannelCredentials()));
    RowsReply rows_reply = master_client.get_rows_call();

    for (int i = page_index * 10;
         i < min(page_index * 10 + 10, rows_reply.row_names_size());
         ++i)
    {
        string row_str = rows_reply.row_names(i);
        string channel = master_client.which_node_call(row_str);
        KeyValueClient client (
            grpc::CreateChannel(channel, grpc::InsecureChannelCredentials()));
        ColumnsReply reply = client.columns_for_row_call(row_str);

        Value objValue;
        objValue.SetObject();
        Value row_val(row_str.c_str(), allocator);
        Value col_arr(kArrayType);

        for (int j = 0; j < reply.column_names_size() ; ++j) {
            Value col(reply.column_names(j), allocator);
            col_arr.PushBack(col, allocator);
        }

        objValue.AddMember("user", row_val, allocator);
        objValue.AddMember("cols", col_arr, allocator);

        data_array.PushBack(objValue, allocator);
    }

    const char* nextPageExists = (page_index * 10 + 10 < rows_reply.row_names_size()) ? "true" : "false";
    Value nextPageVal(nextPageExists, allocator);

    outDoc.AddMember("next_page", nextPageVal, allocator);
    outDoc.AddMember("data", data_array, allocator);
    StringBuffer sb;
    Writer<StringBuffer> writer(sb);
    outDoc.Accept(writer);
    resp.body = string(sb.GetString());

    resp.top_line = "HTTP/1.1 200 OK";
    resp.headers.insert(make_pair("Date", get_timestamp()));
    resp.headers.insert(make_pair("Content-Type", "application/json"));
    resp.headers["Access-Control-Allow-Origin"] = req.headers["Origin"];
    resp.headers.insert(
        make_pair("Content-Length", to_string(resp.body.size())));

    auto s = serialize_http_response(resp);
    write_message(client_fd, s);
    fprintf(stderr, "[%d] S: %s", client_fd, s.c_str());
}

void get_cell_data(int client_fd, http_message &req) {
    http_message resp;
    map<string, string> query_key_to_value;
    int query_res = get_query_params(req.route, query_key_to_value);
    if (query_res < 0) {
        // Send 404
        resp.top_line = "HTTP/1.1 404 Not Found";
        resp.body = "{}";
        resp.headers.insert(make_pair("Date", get_timestamp()));
        resp.headers.insert(make_pair("Content-Type", "application/json"));
        resp.headers["Access-Control-Allow-Origin"] = req.headers["Origin"];
        resp.headers.insert(
            make_pair("Content-Length", to_string(resp.body.size())));

        auto s = serialize_http_response(resp);
        write_message(client_fd, s);
        fprintf(stderr, "[%d] S: %s", client_fd, s.c_str());
    }
    auto row_name = query_key_to_value["rowName"];
    auto col_name = query_key_to_value["colName"];

    Document outDoc;
    outDoc.SetObject();
    Document::AllocatorType& allocator = outDoc.GetAllocator();

    MasterBackendClient master_client(
        grpc::CreateChannel(
            "localhost:5000",
            grpc::InsecureChannelCredentials()));
    string channel = master_client.which_node_call(row_name);
    KeyValueClient client(
        grpc::CreateChannel(channel, grpc::InsecureChannelCredentials()));

    GetReply reply = client.get_call(row_name, col_name);
    if (reply.response() != ResponseCode::SUCCESS) {
        // send 404
        resp.top_line = "HTTP/1.1 404 Not Found";
        resp.body = "{}";
        resp.headers.insert(make_pair("Date", get_timestamp()));
        resp.headers.insert(make_pair("Content-Type", "application/json"));
        resp.headers["Access-Control-Allow-Origin"] = req.headers["Origin"];
        resp.headers.insert(
            make_pair("Content-Length", to_string(resp.body.size())));

        auto s = serialize_http_response(resp);
        write_message(client_fd, s);
        fprintf(stderr, "[%d] S: %s", client_fd, s.c_str());
    }

    Value table_data(reply.value(), allocator);
    outDoc.AddMember("data", table_data, allocator);

    StringBuffer sb;
    Writer<StringBuffer> writer(sb);
    outDoc.Accept(writer);
    resp.body = string(sb.GetString());

    resp.top_line = "HTTP/1.1 200 OK";
    resp.headers.insert(make_pair("Date", get_timestamp()));
    resp.headers.insert(make_pair("Content-Type", "application/json"));
    resp.headers["Access-Control-Allow-Origin"] = req.headers["Origin"];
    resp.headers.insert(
        make_pair("Content-Length", to_string(resp.body.size())));

    auto s = serialize_http_response(resp);
    write_message(client_fd, s);
    fprintf(stderr, "[%d] S: %s", client_fd, s.c_str());
}
