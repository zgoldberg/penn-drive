#include <grpcpp/grpcpp.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <streambuf>
#include <string>

#include "kv_cache.hh"

#ifdef BAZEL_BUILD
#include "kvstore/protos/kvstore.grpc.pb.h"
#else
#include "kvstore.grpc.pb.h"
#endif

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using kvstore::CPutReply;
using kvstore::CPutRequest;
using kvstore::DeleteReply;
using kvstore::DeleteRequest;
using kvstore::GetReply;
using kvstore::GetRequest;
using kvstore::KeyValueStore;
using kvstore::PutReply;
using kvstore::PutRequest;
using kvstore::ResponseCode;

using namespace std;

class KeyValueStoreClient {
   public:
    KeyValueStoreClient(shared_ptr<Channel> channel)
        : stub_(KeyValueStore::NewStub(channel)) {}

    // Assembles the client's payload, sends it and presents the response back
    // from the server.

    string Put(string row, string column, string value, bool is_client_msg) {
        PutRequest request;
        request.set_row(row);
        request.set_column(column);
        request.set_value(value);
        request.set_is_client_message(is_client_msg);

        PutReply reply;
        ClientContext context;
        Status status = stub_->Put(&context, request, &reply);

        if (status.ok()) {
            ResponseCode rc = reply.response();
            if (rc == ResponseCode::SUCCESS) {
                return "Successful Put\n";
            } else {
                cerr << reply.error();
                return "Failed Put\n";
            }
        }
        return "Timed out Put\n";
    }

    string Get(string row, string column) {
        GetRequest request;
        request.set_row(row);
        request.set_column(column);

        GetReply reply;
        ClientContext context;
        Status status = stub_->Get(&context, request, &reply);
        if (status.ok()) {
            ResponseCode rc = reply.response();
            if (rc == ResponseCode::SUCCESS) {
                return reply.value();
            } else if (rc == ResponseCode::MISSING_KEY) {
                cerr << reply.error();
                return "Missing key\n";
            } else {
                cerr << reply.error();
                return "Failed Get\n";
            }
        }
        return "Failure Get\n";
    }

    string CPut(string row, string column, string old_value, string new_value, bool is_client_msg) {
        CPutRequest request;
        request.set_row(row);
        request.set_column(column);
        request.set_old_value(old_value);
        request.set_new_value(new_value);
        request.set_is_client_message(is_client_msg);

        CPutReply reply;
        ClientContext context;
        Status status = stub_->CPut(&context, request, &reply);

        if (status.ok()) {
            ResponseCode rc = reply.response();
            if (rc == ResponseCode::SUCCESS) {
                return "Sucessful CPut\n";
            } else if (rc == ResponseCode::CPUT_VAL_MISMATCH) {
                cerr << reply.error();
                return "Old value mismatch\n";
            } else if (rc == ResponseCode::MISSING_KEY) {
                cerr << reply.error();
                return "Missing key\n";
            } else {
                cerr << reply.error();
                return "Failed CPut\n";
            }
        }
        return "Failure CPut\n";
    }

    string Delete(string row, string column, bool is_client_msg) {
        DeleteRequest request;
        request.set_row(row);
        request.set_column(column);
        request.set_is_client_message(is_client_msg);

        DeleteReply reply;
        ClientContext context;
        Status status = stub_->Delete(&context, request, &reply);

        if (status.ok()) {
            ResponseCode rc = reply.response();
            if (rc == ResponseCode::SUCCESS) {
                return "Sucessful Delete\n";
            } else if (rc == ResponseCode::MISSING_KEY) {
                cerr << reply.error();
                return "Missing key\n";
            } else {
                cerr << reply.error();
                return "Failed Delete\n";
            }
        }
        return "Failure Delete\n";
    }

   private:
    std::unique_ptr<KeyValueStore::Stub> stub_;
};

int main(int argc, char** argv) {
    // Instantiate the client. It requires a channel, out of which the actual RPCs
    // are created. This channel models a connection to an endpoint specified by
    // the argument "--target=" which is the only expected argument.
    // We indicate that the channel isn't authenticated (use of
    // InsecureChannelCredentials()).
    if (argc < 2) {
        cerr << "missing address" << endl;
        return 0;
    }
    string target_str = argv[1];
    grpc::ChannelArguments ch_args;
    ch_args.SetMaxReceiveMessageSize(-1);
    KeyValueStoreClient client(
        grpc::CreateCustomChannel(target_str, grpc::InsecureChannelCredentials(), ch_args));

    cout << client.Put("row_test", "col_test", "testing", true) << endl;
    cout << client.Get("row_test", "col_test") << endl;

    ifstream is("/home/cis505/Downloads/img");
    stringstream buf;
    buf << is.rdbuf();
    for (int i = 0; i < 2; ++i)
        buf << buf.str();
    cout << buf.str().size() << endl;
    cout << client.Put("row_test", "col_test", buf.str(), true) << endl;
    cout << client.Get("row_test", "col_test") << endl;

    // // PUT
    // int j = 0;
    // for (int i = 1; i <= 1000; i++) {
    //     client.Put(to_string(j), to_string(i), "value" + to_string(i), true);
    //     if (i % 5 == 0) {
    //         auto res = client.Get(to_string(j), to_string(i));
    //         if (res == "value" + to_string(i)) {
    //             cout << "GET is ok" << endl;
    //         } else {
    //             cout << "GET IS WRONGGGGGGGGGGGGGGGGGGGGGGGG" << endl;
    //             cout << res << " instead of " << (i + 1000) << endl;
    //         }
    //     }
    //     j++;
    //     if (j > 10) {
    //         j = 0;
    //     }
    // }

    // // CPUT
    // j = 0;
    // for (int i = 1; i <= 1000; i++) {
    //     client.CPut(to_string(j), to_string(i), "value" + to_string(i), "value" + to_string(i + 1000), true);
    //     if (i % 5 == 0) {
    //         auto res = client.Get(to_string(j), to_string(i));
    //         if (res == "value" + to_string(i + 1000)) {
    //             cout << "GET is ok" << endl;
    //         } else {
    //             cout << "GET IS WRONGGGGGGGGGGGGGGGGGGGGGGGG" << endl;
    //             cout << res << " instead of " << (i + 1000) << endl;
    //         }
    //     }
    //     j++;
    //     if (j > 10) {
    //         j = 0;
    //     }
    // }

    // j = 0;
    // for (int i = 1; i <= 500; i++) {
    //     client.Delete(to_string(j), to_string(i), true);
    //     j++;
    //     if (j > 10) {
    //         j = 0;
    //     }
    // }
    return 0;
}
