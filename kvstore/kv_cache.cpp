#include "kv_cache.hh"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>

KVCache::KVCache(int capacity) : capacity(capacity), available_memory(capacity) {}

KVCache::KVCache(int capacity, string dir) : capacity(capacity), available_memory(capacity), dir(dir) {}

void KVCache::print_cache() {
    cout << "CACHE" << endl;
    for (auto const& x : cache) {
        cout << "ROW " << x.first << endl;
        for (auto const& file : x.second) {
            cout << file.first << " " << file.second << endl;
        }
    }
    cout << endl;
}

// returns true if the user is in the cache
bool KVCache::contains(string user) {
    return cache.find(user) != cache.end();
}

bool KVCache::evict_users(string user, size_t size) {
    deque<pair<string, int>> q(row_sizes.begin(), row_sizes.end());
    stable_sort(q.begin(), q.end(), [](const pair<string, int>& left, const pair<string, int>& right) {
        return left.second < right.second;
    });  // should be using LRU cache
    bool flag = false;
    while (size > available_memory) {
        auto p = q.front();
        q.pop_front();
        if (p.first == user) continue;  // skip yourself
        flag = true;
        cache.erase(p.first);
        available_memory += p.second;
    }
    return flag;
}

// if the user is not in cache, load its row from disk
bool KVCache::load_user_from_disk(string user) {
    if (!contains(user)) {
        for (auto const& x : disk_map[user]) {  // load in user's row
            auto filename = x.first;
            auto blocks = x.second;
            string s;
            for (auto block : blocks) {
                fstream fs(dir + "/block" + to_string(block.block_id));
                fs.seekg(block.offset);
                int start = s.size();
                s.resize(start + block.size);
                fs.read(&s[start], block.size);
                available_memory -= block.size;
                fs.close();
            }
            cache[user][filename] = s;
        }
        return true;
    }
    return false;
}

void KVCache::update_disk_map(string user, string column, string value) {
    disk_map[user][column].clear();
    int size = value.size();
    while (size > 0) {
        if (block_offset + size > BLOCK_SIZE) {  // doesn't fit into current block
            int rem = BLOCK_SIZE - block_offset;
            disk_map[user][column].push_back(BlockPtr(block_id, block_offset, rem));
            size -= rem;
            ++block_id;
            block_offset = 0;
            remove((dir + "/block" + to_string(block_id)).c_str());
            int fd = open((dir + "/block" + to_string(block_id)).c_str(), O_CREAT | O_RDWR, 0666);
            posix_fallocate(fd, 0, BLOCK_SIZE);
            close(fd);
        } else {
            disk_map[user][column].push_back(BlockPtr(block_id, block_offset, size));
            block_offset += size;
            size = 0;
        }
    }
}

void KVCache::put(string user, string column, string value) {
    // print_cache();
    load_user_from_disk(user);
    if (cache[user].count(column)) {  // already in cache, so delete first
        dele(user, column);
    }
    lock_guard<mutex> guard(lock);
    cache[user][column] = value;
    available_memory -= value.size();
    row_sizes[user] += value.size();
    update_disk_map(user, column, value);
}

int KVCache::get(string user, string column, string* value) {
    lock_guard<mutex> guard(lock);
    load_user_from_disk(user);
    auto it = cache[user].find(column);
    if (it != cache[user].end()) {
        *value = it->second;
    } else {  // missing key
        return 1;
    }
    return 0;
}

int KVCache::cput(string user, string column, string value1, string value2) {
    lock_guard<mutex> guard(lock);
    load_user_from_disk(user);
    auto it = cache[user].find(column);
    if (it != cache[user].end()) {
        if (cache[user][column] != value1) {  // incorrect old val
            return 2;
        }
        cache[user][column] = value2;
        available_memory -= value1.size() - value2.size();
        row_sizes[user] += value2.size() - value1.size();
        update_disk_map(user, column, value2);
    } else {  // missing key
        return 1;
    }
    return 0;
}

int KVCache::dele(string user, string column) {
    lock_guard<mutex> guard(lock);
    auto it = disk_map[user].find(column);
    if (it == disk_map[user].end()) {  // missing key
        return 1;
    }
    if (contains(user)) {  // in cache
        auto it = cache[user].find(column);
        if (it != cache[user].end()) {
            available_memory += cache[user][column].size();
            row_sizes[user] -= cache[user][column].size();
            cache[user].erase(column);
        } else {  // missing key
            return 1;
        }
    }
    disk_map[user].erase(column);
    return 0;
}

void KVCache::clear() {
    lock_guard<mutex> guard(lock);
    cache.clear();
}
