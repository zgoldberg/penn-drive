#ifndef kv_cache
#define kv_cache

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#define BLOCK_SIZE 10 * 1024 * 1024

using namespace std;

struct BlockPtr {
    int block_id;
    int offset;
    int size;

    BlockPtr(){};
    BlockPtr(int block_id, int offset, int size) : block_id(block_id), offset(offset), size(size){};
};

struct KVCache {
    unordered_map<string, unordered_map<string, string>> cache;               // <user, <column, value>>
    unordered_map<string, int> row_sizes;                                     // <user, row size>
    unordered_map<string, unordered_map<string, vector<BlockPtr>>> disk_map;  // <user, <column, {block_id, offset, size}>>
    mutex lock;
    int capacity;
    size_t available_memory;
    string dir;
    int block_id;
    int block_offset;

    KVCache(int capacity);
    KVCache(int capacity, string dir);
    void put(string user, string column, string value);
    int cput(string user, string column, string value1, string value2);
    void put_helper(string user, string column, string value);
    int get(string user, string column, string* value);
    int dele(string user, string column);

    void print_cache();
    void update_disk_map(string user, string column, string value);
    bool load_user_from_disk(string user);
    bool contains(string user);
    bool evict_users(string user, size_t size);
    void log(vector<string> outputs);
    void checkpoint();
    void clear();
};

#endif