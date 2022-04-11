#include "http.h"
#define BUFFER_SIZE 102400

using grpc::Channel;
using namespace std;

std::string get_timestamp() {
    using namespace std::chrono;

    system_clock::time_point now = high_resolution_clock::now();
    std::time_t now_c = system_clock::to_time_t(now);
    std::tm now_tm = *std::gmtime(&now_c);

    const string DAY[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    const string MONTHS[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

    char s[100];
    char s2[1024];
    strftime(s, 100, "%y %H:%M:%S GMT", &now_tm);

    sprintf(s2, "%s, %s %d %s",
        DAY[now_tm.tm_wday].c_str(), MONTHS[now_tm.tm_mon].c_str(),
        now_tm.tm_mday, s);
    string ret = string(s2);

    return ret;
}

string serialize_http_response(const http_message& resp) {
    string out;

    out.append(resp.top_line);
    out.append("\r\n");
    for (auto i = resp.headers.begin(); i != resp.headers.end(); ++i) {
        auto field = i->first;
        auto val = i->second;
        out.append(field);
        out.append(":");
        out.append(val);
        out.append("\r\n");
    }

    string set_cookie = "Set-Cookie: ";
    for (auto cookie : resp.cookies) {
        auto field = cookie.first;
        auto val = cookie.second;
        out.append(set_cookie);
        out.append(field);
        out.append("=");
        out.append(val);
        out.append("; SameSite=None\r\n");
    }

    out.append("\r\n");
    out.append(resp.body);
    return out;
}

int write_message(int fd, string buffer) {
    return send(fd, buffer.c_str(), buffer.length(), 0);
}

pair<string, string> username_sid_from_request(http_message &req) {
    auto sid_iter = req.cookies.find("sid");
    auto username_iter = req.cookies.find("username");

    if (sid_iter == req.cookies.end() || username_iter == req.cookies.end()) {
        return make_pair("", "");
    }
    return make_pair(username_iter->second, sid_iter->second);
}

bool verify_user(string &username, string &sid, KeyValueClient &client) {
    // Ensure that the session id is valid
    GetReply get_response = client.get_call(username, "sids");

    string sids = get_response.value();
    cerr << get_response.response() << endl;
    fprintf(stderr, "%s\n", sids.c_str());
    if (sids.find(sid) == string::npos) {
        return false;
    }

    return true;
}

int read_message(int fd, char* buffer) {
    int read_result;
    for (int j = 0; j < BUFFER_SIZE; buffer[j++] = 0);

    for (int i = 0; i < BUFFER_SIZE; i++) {
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

void parse_http_header(string line, http_message& m) {
    if (line.find("HTTP") != string::npos) {
        // Contains the endpoint and method type
        auto linevec = separate_string(line, " ");
        if (linevec[0] == "GET") {
            m.request_type = GET;
        } else if (linevec[0] == "POST") {
            m.request_type = POST;
        } else if (linevec[0] == "HEAD") {
            m.request_type = HEAD;
        } else {
        }

        m.route = linevec[1];
        m.top_line = line;
        return;
    }
    auto pos = line.find(":");
    if (pos != string::npos) {
        string header_field = trim_str(line.substr(0, pos));
        string value = line.substr(pos + 1);

        if (header_field.find("Content-Length") != string::npos) {
            m.body_length = stoi(value);
        } else if (header_field.find("Cookie") != string::npos) {
            auto cookies_vec = separate_string(value, ";");
            for (auto cookie : cookies_vec) {
                auto cookie_pair = separate_string(cookie, "=");
                m.cookies.insert(
                    make_pair(
                        trim_str(cookie_pair[0]),
                        trim_str(cookie_pair[1])
                    )
                );
            }
        }

        m.headers.insert(make_pair(header_field, value));
    }
}
