#include <grpcpp/grpcpp.h>

#ifdef BAZEL_BUILD
#include "kvstore/protos/kvstore.grpc.pb.h"
#else
#include "kvstore.grpc.pb.h"
#endif

#include <fcntl.h>
#include <stdio.h>

#include <fstream>
#include <set>
#include <thread>
#include <unordered_set>

#include "kv_cache.hh"
#include "utils.hh"

using grpc::Channel;
using kvstore::KeyValueStore;

using namespace std;

//  Client class
class KeyValueStoreClient {
   public:
    KeyValueStoreClient();
    KeyValueStoreClient(shared_ptr<Channel> channel);
    KeyValueStoreClient(KeyValueStoreClient&&);
    KeyValueStoreClient& operator=(KeyValueStoreClient&& c) {
        stub_ = move(c.stub_);
        return *this;
    }

    string Put(string row, string column, string value, bool, int);
    string Get(string row, string column);
    string CPut(string row, string column, string old_value, string new_value, bool, int);
    string Delete(string row, string column, bool is_client, int);
    void Recover(string version, int block_id, int num_iter);
    void Disk(int block_id);
    void Metadata();
    void Checkpoint();

   private:
    std::unique_ptr<KeyValueStore::Stub> stub_;
};