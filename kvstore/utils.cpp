#include "utils.hh"

#include <string.h>
#include <unistd.h>

ordered_lock::ordered_lock() : locked(false) {}

void ordered_lock::lock() {
    unique_lock<mutex> acquire(cvar_lock);
    if (locked) {
        condition_variable signal;
        cvar.emplace(&signal);
        signal.wait(acquire);
    } else {
        locked = true;
    }
}

void ordered_lock::unlock() {
    unique_lock<mutex> acquire(cvar_lock);
    if (cvar.empty()) {
        locked = false;
    } else {
        cvar.front()->notify_one();
        cvar.pop();
    }
}

bool ordered_lock::empty() {
    return cvar.empty();
}

void write_string(int fd, std::string string_msg) {
    const char *msg = string_msg.c_str();
    do_write(fd, msg, strlen(msg));
}

bool do_write(int fd, const char *buf, int len) {
    int sent = 0;
    while (sent < len) {
        int n = write(fd, &buf[sent], len - sent);
        if (n < 0)
            return false;
        sent += n;
    }
    return true;
}

int rm_files(const char *pathname, const struct stat *sbuf, int type, struct FTW *ftwb) {
    if (remove(pathname) < 0) {
        perror("ERROR: remove");
        return -1;
    }
    return 0;
}

vector<string> tokenize(string str, string delim) {
    // tokenize given string using given delimiter
    vector<string> tokens;
    size_t start = str.find_first_not_of(delim.c_str());
    str = str.substr(start);
    while (!str.empty()) {
        size_t end = str.find_first_of(delim.c_str());
        if (end == 0) {
            break;
        }
        string token = str.substr(0, end);
        tokens.push_back(token);
        if (end == string::npos) {
            break;
        }
        str = str.substr(end);
        start = str.find_first_not_of(delim.c_str());
        if (start != string::npos) {
            str = str.substr(start);
        }
    }
    return tokens;
}