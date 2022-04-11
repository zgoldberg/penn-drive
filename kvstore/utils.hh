#ifndef utils
#define utils

#include <ftw.h>
#include <stdlib.h>

#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

using namespace std;

class ordered_lock {
    queue<condition_variable *> cvar;
    mutex cvar_lock;
    bool locked;

   public:
    ordered_lock();
    void lock();
    void unlock();
    bool empty();
};

void write_string(int fd, string string_msg);
bool do_write(int fd, const char *buf, int len);
int rm_files(const char *pathname, const struct stat *sbuf, int type, struct FTW *ftwb);
vector<string> tokenize(string str, string delim);

#endif