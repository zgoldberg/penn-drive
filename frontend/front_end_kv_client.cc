#include "front_end_kv_client.h"

using namespace std;

KeyValueClient::KeyValueClient() {}
KeyValueClient::KeyValueClient(std::shared_ptr<Channel> channel)
      : stub_(KeyValueStore::NewStub(channel)) {}
MasterBackendClient::MasterBackendClient(std::shared_ptr<Channel> channel)
      : stub_(MasterBackend::NewStub(channel)) {}

GetReply KeyValueClient::get_call(string row, string column) {
    // Data we are sending to the server.
    GetRequest request;
    request.set_row(row);
    request.set_column(column);

    GetReply reply;
    ClientContext context;

    // The actual RPC.
    Status status = stub_->Get(&context, request, &reply);

    if (!status.ok()) {
        std::cout << status.error_code() << ": " << status.error_message()
                  << std::endl;
    }
    return reply;
}

PutReply KeyValueClient::put_call(string row, string column, string value) {
    // Data we are sending to the server.
    PutRequest request;
    request.set_row(row);
    request.set_column(column);
    request.set_value(value);
    request.set_is_client_message(true);

    PutReply reply;
    ClientContext context;

    // The actual RPC.
    Status status = stub_->Put(&context, request, &reply);

    if (!status.ok()) {
        std::cout << status.error_code() << ": " << status.error_message()
                  << std::endl;
    }
    return reply;
}

CPutReply KeyValueClient::cput_call(string row,
                    string column,
                    string old_value,
                    string new_value)
{
    // Data we are sending to the server.
    CPutRequest request;
    request.set_row(row);
    request.set_column(column);
    request.set_old_value(old_value);
    request.set_new_value(new_value);
    request.set_is_client_message(true);

    CPutReply reply;
    ClientContext context;

    // The actual RPC.
    Status status = stub_->CPut(&context, request, &reply);


    if (!status.ok()) {
        std::cout << status.error_code() << ": " << status.error_message()
                  << std::endl;
    }
    return reply;
}

DeleteReply KeyValueClient::delete_call(string row, string column) {
    // Data we are sending to the server.
    DeleteRequest request;
    request.set_row(row);
    request.set_column(column);
    request.set_is_client_message(true);

    DeleteReply reply;
    ClientContext context;

    // The actual RPC.
    Status status = stub_->Delete(&context, request, &reply);

    if (!status.ok()) {
        std::cout << status.error_code() << ": " << status.error_message()
                  << std::endl;
    }
    return reply;
}

ColumnsReply KeyValueClient::columns_for_row_call(string row) {
    ColumnsRequest request;
    request.set_row(row);

    ColumnsReply reply;
    ClientContext context;

    Status status = stub_->ColumnsForRow(&context, request, &reply);

    if (reply.response() != ResponseCode::SUCCESS) {
        std::cout << status.error_code() << ": " << status.error_message()
                  << std::endl;
    }

    return reply;
}

string MasterBackendClient::which_node_call(string user) {
    WhichRequest request;
    request.set_user(user);

    WhichReply reply;
    ClientContext context;

    Status status = stub_->Which(&context, request, &reply);

    if (reply.response() == ResponseCode::FAILURE) {
        std::cout << status.error_code() << ": " << status.error_message()
                  << std::endl;
    }
    return reply.address();
}

NodeStatusReply MasterBackendClient::node_status_call() {
    NodeStatusRequest request;

    NodeStatusReply reply;
    ClientContext context;

    Status status = stub_->NodeStatus(&context, request, &reply);
    if (reply.response() == ResponseCode::FAILURE) {
        std::cout << status.error_code() << ": " << status.error_message()
                  << std::endl;
    }

    return reply;
}

ChangeStatusReply MasterBackendClient::start_node_call(string node_id) {
    ChangeStatusRequest request;
    request.set_node_name(node_id);

    ChangeStatusReply reply;
    ClientContext context;

    Status status = stub_->NodeTurnOn(&context, request, &reply);
    if (reply.response() == ResponseCode::FAILURE) {
        std::cout << status.error_code() << ": " << status.error_message()
                  << std::endl;
    }

    return reply;
}

ChangeStatusReply MasterBackendClient::stop_node_call(string node_id) {
    ChangeStatusRequest request;
    request.set_node_name(node_id);

    ChangeStatusReply reply;
    ClientContext context;

    Status status = stub_->NodeTurnOff(&context, request, &reply);
    if (reply.response() == ResponseCode::FAILURE) {
        std::cout << status.error_code() << ": " << status.error_message()
                  << std::endl;
    }

    return reply;
}

RowsReply MasterBackendClient::get_rows_call() {
    RowsRequest request;
    RowsReply reply;
    ClientContext context;

    Status status = stub_->GetRows(&context, request, &reply);
    if (reply.response() != ResponseCode::SUCCESS) {
        std::cout << status.error_code() << ": " << status.error_message()
                  << std::endl;
    }

    return reply;
}
