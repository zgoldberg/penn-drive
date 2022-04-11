#ifndef FE_KVCLIENT_H
#define FE_KVCLIENT_H

#define MAX_CPUT_ITERATIONS 100
#define DELIMITER_STRING ";"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <signal.h>
#include <sys/select.h>
#include <errno.h>
#include <dirent.h>
#include <sys/file.h>

#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>

#ifdef BAZEL_BUILD
#include "kvstore/protos/kvstore.grpc.pb.h"
#else
#include "kvstore.grpc.pb.h"
#endif

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using grpc::InsecureChannelCredentials;

using kvstore::KeyValueStore;
using kvstore::MasterBackend;
using kvstore::PutRequest;
using kvstore::CPutRequest;
using kvstore::DeleteRequest;
using kvstore::GetRequest;
using kvstore::WhichRequest;
using kvstore::NodeStatusRequest;
using kvstore::ChangeStatusRequest;
using kvstore::ColumnsRequest;
using kvstore::RowsRequest;

using kvstore::GetReply;
using kvstore::CPutReply;
using kvstore::PutReply;
using kvstore::DeleteReply;
using kvstore::WhichReply;
using kvstore::NodeStatusReply;
using kvstore::ChangeStatusReply;
using kvstore::RowsReply;
using kvstore::ColumnsReply;
using kvstore::ResponseCode;

using namespace std;

class MasterBackendClient {
    public:
        MasterBackendClient(std::shared_ptr<Channel> channel);

        string which_node_call(string user);
        NodeStatusReply node_status_call();
        ChangeStatusReply start_node_call(string node_id);
        ChangeStatusReply stop_node_call(string node_id);
        RowsReply get_rows_call();

    private:
        std::unique_ptr<MasterBackend::Stub> stub_;
};

class KeyValueClient {
    public:
        KeyValueClient();
        KeyValueClient(std::shared_ptr<Channel> channel);

        GetReply get_call(string row, string column);

        PutReply put_call(string row, string column, string value);

        CPutReply cput_call(string row,
                            string column,
                            string old_value,
                            string new_value);

        DeleteReply delete_call(string row, string column);

        ColumnsReply columns_for_row_call(string row);

    private:
        std::unique_ptr<KeyValueStore::Stub> stub_;
};

#endif /* FE_KVCLIENT_H */
