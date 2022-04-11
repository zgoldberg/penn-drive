#include "kv_store.hh"

#include <dirent.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <chrono>

using grpc::ClientContext;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using kvstore::AliveReply;
using kvstore::AliveRequest;
using kvstore::CheckpointReply;
using kvstore::CheckpointRequest;
using kvstore::CPutReply;
using kvstore::CPutRequest;
using kvstore::DeleteReply;
using kvstore::DeleteRequest;
using kvstore::DiskReply;
using kvstore::DiskRequest;
using kvstore::GetReply;
using kvstore::GetRequest;
using kvstore::Health;
using kvstore::MetadataReply;
using kvstore::MetadataRequest;
using kvstore::PutReply;
using kvstore::PutRequest;
using kvstore::RecoverReply;
using kvstore::RecoverRequest;
using kvstore::ColumnsReply;
using kvstore::ColumnsRequest;
using kvstore::ResponseCode;

#define CHECKPOINT_THRESHOLD 100
#define RPC_MSG_SIZE 10 * 1024 * 1024

string server_address;
string heartbeat_addr;
string path;
unordered_set<int> file_blocks;

//  replication vars
bool is_primary = false;
volatile sig_atomic_t recovering = 0;
bool startup = true;
KeyValueStoreClient client_to_primary;
vector<string> replica_addresses;
vector<KeyValueStoreClient> replicas;

KVCache memtable(100 * 1024 * 1024);  // <user, <column, value>>
auto cmp = [](pair<int, int> a, pair<int, int> b) {
    return a.first - b.first;
};
unordered_map<string, unordered_set<string>> modified;
set<pair<int, int>, decltype(cmp)> empty_spaces(cmp);
volatile sig_atomic_t num_operations = 0;
volatile sig_atomic_t num_checkpoints = 1;
mutex request_lock;
ordered_lock all_lock;

KeyValueStoreClient::KeyValueStoreClient() {}

KeyValueStoreClient::KeyValueStoreClient(shared_ptr<Channel> channel)
    : stub_(KeyValueStore::NewStub(channel)) {}

KeyValueStoreClient::KeyValueStoreClient(KeyValueStoreClient&& c)
    : stub_(move(c.stub_)) {}

string KeyValueStoreClient::Put(string row, string column, string value, bool is_client, int old_version) {
    PutRequest request;
    request.set_row(row);
    request.set_column(column);
    request.set_value(value);
    request.set_is_client_message(is_client);
    request.set_src_addr(server_address);
    request.set_old_version(old_version);

    PutReply reply;
    ClientContext context;
    Status status = stub_->Put(&context, request, &reply);
    if (status.ok()) {
        ResponseCode rc = reply.response();
        if (rc == ResponseCode::SUCCESS) {
            return "SUCCESS";
        } else {
            cerr << reply.error();
            return "FAILURE";
        }
    }
    return "FAILURE";
}

string KeyValueStoreClient::Get(string row, string column) {
    GetRequest request;
    request.set_row(row);
    request.set_column(column);

    GetReply reply;
    ClientContext context;
    Status status = stub_->Get(&context, request, &reply);
    if (status.ok()) {
        ResponseCode rc = reply.response();
        if (rc == ResponseCode::SUCCESS) {
            return reply.value() + "\n";
        } else if (rc == ResponseCode::MISSING_KEY) {
            cerr << reply.error();
            return "MISSING KEY";
        } else {
            cerr << reply.error();
            return "FAILURE";
        }
    }
    return "FAILURE";
}

string KeyValueStoreClient::CPut(string row, string column, string old_value, string new_value, bool is_client, int old_version) {
    CPutRequest request;
    request.set_row(row);
    request.set_column(column);
    request.set_old_value(old_value);
    request.set_new_value(new_value);
    request.set_is_client_message(is_client);
    request.set_src_addr(server_address);
    request.set_old_version(old_version);

    CPutReply reply;
    ClientContext context;
    Status status = stub_->CPut(&context, request, &reply);

    if (status.ok()) {
        ResponseCode rc = reply.response();
        if (rc == ResponseCode::SUCCESS) {
            return "SUCCESS";
        } else if (rc == ResponseCode::CPUT_VAL_MISMATCH) {
            cerr << reply.error();
            return "VAL MISMATCH";
        } else if (rc == ResponseCode::MISSING_KEY) {
            cerr << reply.error();
            return "MISSING KEY";
        } else {
            cerr << reply.error();
            return "FAILURE";
        }
    }
    return "FAILURE";
}

string KeyValueStoreClient::Delete(string row, string column, bool is_client, int old_version) {
    DeleteRequest request;
    request.set_row(row);
    request.set_column(column);
    request.set_is_client_message(is_client);
    request.set_src_addr(server_address);
    request.set_old_version(old_version);

    DeleteReply reply;
    ClientContext context;
    Status status = stub_->Delete(&context, request, &reply);

    if (status.ok()) {
        ResponseCode rc = reply.response();
        if (rc == ResponseCode::SUCCESS) {
            return "SUCCESS";
        } else if (rc == ResponseCode::MISSING_KEY) {
            cerr << reply.error();
            return "MISSING KEY";
        } else {
            cerr << reply.error();
            return "FAILURE";
        }
    }
    return "FAILURE";
}

void KeyValueStoreClient::Recover(string version, int block_id, int num_iter) {
    RecoverRequest request;
    request.set_version(version);
    request.set_num_iter(num_iter);
    // Consider sending current log file so primary can send diff only

    RecoverReply reply;
    do {
        ClientContext context;
        Status status = stub_->Recover(&context, request, &reply);
        if (status.ok()) {
            cerr << "recover response from primary is ok" << endl;
            recovering = 0;
            ofstream log_out;
            if (num_iter == 0) {
                log_out.open(path + "/log", ios::trunc);
                log_out << reply.version() << "\n";
                num_checkpoints = stoi(reply.version());
            } else {
                log_out.open(path + "/log", ios::app);
            }
            log_out << reply.value();
            log_out.close();
            request.set_num_iter(++num_iter);
        }
    } while (!reply.is_end());

    if (reply.version_match()) {
        cerr << "log versions match, no disk copy needed" << endl;
        Metadata();
    } else {
        cerr << "disk copy needed" << endl;
        Disk(block_id);
    }
}

void KeyValueStoreClient::Disk(int block_id) {
    DiskRequest request;
    request.set_block_id(block_id);

    DiskReply reply;
    do {
        ClientContext context;
        Status status = stub_->Disk(&context, request, &reply);
        if (status.ok()) {
            cerr << "loading block" << block_id << endl;
            remove((path + "/block" + to_string(block_id)).c_str());
            int fd = open((path + "/block" + to_string(block_id)).c_str(), O_CREAT | O_RDWR, 0666);
            posix_fallocate(fd, 0, BLOCK_SIZE);
            close(fd);

            ofstream disk_out(path + "/block" + to_string(block_id));
            disk_out.seekp(0);
            disk_out << reply.value();
            cerr << "block" << block_id << " size " << reply.value().size() << endl;
            disk_out.close();
        }
        request.set_block_id(++block_id);
    } while (!reply.is_end());
    cerr << "done with disk copy" << endl;
    Metadata();
}

void KeyValueStoreClient::Metadata() {
    MetadataRequest request;
    MetadataReply reply;
    ClientContext context;
    cerr << "start metadata copy" << endl;
    Status status = stub_->Metadata(&context, request, &reply);
    if (status.ok()) {
        ofstream metadata_out(path + "/metadata", ios::trunc);
        memtable.block_id = stoi(reply.block_id());
        memtable.block_offset = stoi(reply.block_offset());
        metadata_out << memtable.block_id << " " << memtable.block_offset << endl;
        metadata_out << reply.value();
        metadata_out.close();

        ifstream metadata_in(path + "/metadata");
        string line;
        getline(metadata_in, line);
        while (getline(metadata_in, line)) {
            string row = line;
            getline(metadata_in, line);
            string col = line;
            getline(metadata_in, line);
            int blocks_size = stoi(line);
            for (int i = 0; i < blocks_size; ++i) {
                getline(metadata_in, line);
                auto tokens = tokenize(line, " ");
                memtable.disk_map[row][col].push_back(
                    BlockPtr(stoi(tokens[0]), stoi(tokens[1]), stoi(tokens[2])));
            }
        }
        metadata_in.close();
    }
    cerr << "done metadata copy" << endl;
}

void KeyValueStoreClient::Checkpoint() {
    CheckpointRequest request;
    request.set_src_addr(server_address);

    CheckpointReply reply;
    ClientContext context;
    Status status = stub_->Checkpoint(&context, request, &reply);
    if (status.ok()) {
        cerr << "[" << server_address << "] received checkpointing ACK" << endl;
    }
}

void log(vector<string> args) {
    ofstream log_out(path + "/log", ios::app);
    for (auto s : args) {
        log_out << s << endl;
    }
    log_out.close();
}

void checkpoint() {
    // only write to disk files that have been PUT/CPUT
    auto it_mod = modified.begin();
    while (it_mod != modified.end()) {
        string row = it_mod->first;
        for (string column : it_mod->second) {
            string value;
            memtable.get(row, column, &value);
            int size = 0;
            for (auto block : memtable.disk_map[row][column]) {
                fstream checkpoint_fs(path + "/block" + to_string(block.block_id));
                checkpoint_fs.seekp(block.offset);
                char buf[block.size];
                value.copy(buf, block.size, size);
                size += block.size;
                checkpoint_fs.write(buf, block.size);
                checkpoint_fs.close();
            }
        }
        ++it_mod;
    }
    modified.clear();
    // cout << "done with modified" << endl;
    // write disk_map to its file
    ofstream metadata_out(path + "/metadata", ios::trunc);
    // write block id + offset
    metadata_out << memtable.block_id << " " << memtable.block_offset << "\n";
    auto it = memtable.disk_map.begin();
    while (it != memtable.disk_map.end()) {
        // file persists; write row, column, {block_id, offset, size} to metadata
        string row = it->first;
        auto it_row = memtable.disk_map[row].begin();
        while (it_row != memtable.disk_map[row].end()) {
            string column = it_row->first;
            auto blocks = it_row->second;
            metadata_out << row << "\n"
                         << column << "\n"
                         << blocks.size() << "\n";
            for (auto block : blocks) {
                metadata_out << block.block_id << " "
                             << block.offset << " "
                             << block.size << "\n";
            }
            metadata_out << flush;
            it_row++;
        }
        it++;
    }
    metadata_out.close();
    // cout << "done with disk map" << endl;

    num_operations = 0;
    num_checkpoints++;

    ofstream log_out(path + "/log", ios::trunc);
    log_out << num_checkpoints << endl;
    log_out.close();
}

void try_checkpoint(bool force = false) {
    if (force) {
        if (is_primary) {
            cerr << "[PRIMARY] force checkpointing" << endl;
            //  if is primary, send message to replicas
            for (int i = 0; i < (int)replicas.size(); ++i) {
                replicas[i].Checkpoint();
            }
        } else {
            cerr << "[" << server_address << "] force checkpointing" << endl;
            client_to_primary.Checkpoint();
        }
        checkpoint();
    } else if (num_operations == CHECKPOINT_THRESHOLD) {
        cerr << "[" << server_address << "] count checkpointing" << endl;
        checkpoint();
    }
}

// Server class
class KeyValueStoreImpl final : public KeyValueStore::Service {
    Status Put(ServerContext* context, const PutRequest* request, PutReply* reply) {
        // lock_guard<mutex> guard(request_lock);
        all_lock.lock();
        string row = request->row();
        string column = request->column();
        string value = request->value();
        bool is_client_message = request->is_client_message();
        int old_version = request->old_version() == 0 ? num_checkpoints : request->old_version();

        if (is_primary) {
            cerr << "[PRIMARY] put:" << value << " " << value.size() << endl;
            // cout << "primary memory 1: " << memtable.available_memory << endl;
            if (memtable.contains(row)) {
                if (value.size() > memtable.available_memory) {
                    try_checkpoint(true);
                    cout << "[PRIMARY] evict case 1" << endl;
                    memtable.evict_users(row, value.size());
                }
            } else if (memtable.row_sizes[row] + value.size() > memtable.available_memory) {
                try_checkpoint(true);
                cout << "[PRIMARY] evict case 2" << endl;
                memtable.evict_users(row, memtable.row_sizes[row] + value.size());
            }

            // cout << "primary memory 2: " << memtable.available_memory << endl;
            log({"PUT", row, column, to_string(value.size()), value});
            memtable.put(row, column, value);
            modified[row].insert(column);
            ++num_operations;
            try_checkpoint();
            //  send put requests to replicas
            for (int i = 0; i < (int)replicas.size(); ++i) {
                // skip the secondary node that sent this request
                if (replica_addresses[i] == request->src_addr()) continue;
                replicas[i].Put(row, column, value, false, old_version);
            }
            reply->set_response(ResponseCode::SUCCESS);
            all_lock.unlock();
            return Status::OK;
        } else {
            cerr << "[" << server_address << "] put:" << value << " " << value.size() << endl;
            if (is_client_message) {  // send client message to primary
                string ret = client_to_primary.Put(row, column, value, false, old_version);
                if (ret == "FAILURE") {
                    reply->set_response(ResponseCode::FAILURE);
                    all_lock.unlock();
                    return Status::OK;
                }
            }
            // will only go here if request on primary is successful
            if (memtable.contains(row)) {
                if (value.size() > memtable.available_memory) {
                    if (old_version == num_checkpoints)
                        try_checkpoint(true);
                    memtable.evict_users(row, value.size());
                    // cout << "case 1" << endl;
                }
            } else if (memtable.row_sizes[row] + value.size() > memtable.available_memory) {
                if (old_version == num_checkpoints)
                    try_checkpoint(true);
                memtable.evict_users(row, memtable.row_sizes[row] + value.size());
                // cout << "case 2" << endl;
            }

            log({"PUT", row, column, to_string(value.size()), value});
            memtable.put(row, column, value);
            modified[row].insert(column);
            ++num_operations;
            if (old_version == num_checkpoints)
                try_checkpoint();
            reply->set_response(ResponseCode::SUCCESS);
            all_lock.unlock();
            return Status::OK;
        }
    }

    Status Get(ServerContext* context, const GetRequest* request, GetReply* reply) {
        bool locked = false;
        string row = request->row();
        string column = request->column();
        if (memtable.disk_map[row].find(column) == memtable.disk_map[row].end()) {
            reply->set_response(ResponseCode::MISSING_KEY);
            return Status::OK;
        }
        if (!memtable.contains(row)) {
            // lock_guard<mutex> guard(request_lock);
            locked = true;
            all_lock.lock();
            try_checkpoint(true);
            memtable.evict_users(row, memtable.row_sizes[row]);
        }
        string value;
        memtable.get(row, column, &value);
        cerr << "[" << server_address << "] get:" << value << " " << value.size() << endl;
        //  no need to try checkpointing
        reply->set_response(ResponseCode::SUCCESS);
        reply->set_value(value);
        if (locked) {
            all_lock.unlock();
        }
        return Status::OK;
    }

    Status Delete(ServerContext* context, const DeleteRequest* request, DeleteReply* reply) {
        all_lock.lock();
        auto row = request->row();
        auto column = request->column();
        bool is_client_message = request->is_client_message();
        int old_version = request->old_version() == 0 ? num_checkpoints : request->old_version();

        if (is_primary) {
            if (memtable.disk_map[row].find(column) == memtable.disk_map[row].end()) {
                reply->set_response(ResponseCode::MISSING_KEY);
                return Status::OK;
            }

            log({"DELETE", row, column});
            memtable.dele(row, column);
            ++num_operations;
            try_checkpoint();
            //  send delete request to replicas
            for (int i = 0; i < (int)replicas.size(); ++i) {
                // skip the secondary node that sent this request
                if (replica_addresses[i] == request->src_addr()) continue;
                replicas[i].Delete(row, column, false, old_version);
            }
            reply->set_response(ResponseCode::SUCCESS);
            all_lock.unlock();
            return Status::OK;
        } else {
            if (is_client_message) {  //  send client message to primary
                string ret = client_to_primary.Delete(row, column, false, old_version);
                if (ret == "FAILURE") {
                    reply->set_response(ResponseCode::FAILURE);
                    return Status::OK;
                } else if (ret == "MISSING KEY") {
                    reply->set_response(ResponseCode::MISSING_KEY);
                    return Status::OK;
                }
            }
            log({"DELETE", row, column});
            memtable.dele(row, column);
            ++num_operations;
            if (old_version == num_checkpoints)
                try_checkpoint();
            reply->set_response(ResponseCode::SUCCESS);
            all_lock.unlock();
            return Status::OK;
        }
    }

    Status CPut(ServerContext* context, const CPutRequest* request, CPutReply* reply) {
        // lock_guard<mutex> guard(request_lock);
        all_lock.lock();
        string row = request->row();
        string column = request->column();
        string old_value = request->old_value();
        string new_value = request->new_value();
        bool is_client_message = request->is_client_message();
        int old_version = request->old_version() == 0 ? num_checkpoints : request->old_version();

        if (is_primary) {
            cerr << "[PRIMARY] cput (old):" << old_value << " " << old_value.size() << endl;
            cerr << "[PRIMARY] cput (new):" << new_value << " " << new_value.size() << endl;
            //  this node is the primary - execute write and report to replicas
            if (memtable.disk_map[row].find(column) == memtable.disk_map[row].end()) {
                reply->set_response(ResponseCode::MISSING_KEY);
                all_lock.unlock();
                return Status::OK;
            }
            if (memtable.contains(row)) {
                if (new_value.size() > memtable.available_memory) {
                    try_checkpoint(true);
                    cout << "case 1" << endl;
                    memtable.evict_users(row, new_value.size());
                }
            } else if (memtable.row_sizes[row] + new_value.size() > memtable.available_memory) {
                try_checkpoint(true);
                cout << "case 2" << endl;
                memtable.evict_users(row, memtable.row_sizes[row] + new_value.size());
            }

            string value;
            memtable.get(row, column, &value);  // loads old_value into memtable
            log({"CPUT", row, column, to_string(old_value.size()), to_string(new_value.size()), old_value, new_value});
            int ret = memtable.cput(row, column, old_value, new_value);
            if (ret == 2) {
                reply->set_response(ResponseCode::CPUT_VAL_MISMATCH);
                all_lock.unlock();
                return Status::OK;
            }
            modified[row].insert(column);
            ++num_operations;
            try_checkpoint();
            //  send cput request to replicas
            for (int i = 0; i < (int)replicas.size(); ++i) {
                // skip the secondary node that sent this request
                if (replica_addresses[i] == request->src_addr()) continue;
                replicas[i].CPut(row, column, old_value, new_value, false, old_version);
            }
            reply->set_response(ResponseCode::SUCCESS);
            all_lock.unlock();
            return Status::OK;
        } else {
            cerr << "[" << server_address << "] cput (old):" << old_value << " " << old_value.size() << endl;
            cerr << "[" << server_address << "] cput (new):" << new_value << " " << new_value.size() << endl;
            if (is_client_message) {  //  send client message to primary
                string ret = client_to_primary.CPut(row, column, old_value, new_value, false, old_version);
                if (ret == "MISSING KEY") {
                    reply->set_response(ResponseCode::MISSING_KEY);
                    all_lock.unlock();
                    return Status::OK;
                } else if (ret == "VAL MISMATCH") {
                    reply->set_response(ResponseCode::CPUT_VAL_MISMATCH);
                    all_lock.unlock();
                    return Status::OK;
                } else if (ret == "FAILURE") {
                    reply->set_response(ResponseCode::FAILURE);
                    all_lock.unlock();
                    return Status::OK;
                }
            }
            //  execute CPut on this side
            if (memtable.contains(row)) {
                if (new_value.size() > memtable.available_memory) {
                    if (old_version == num_checkpoints)
                        try_checkpoint(true);
                    // cout << "case 1" << endl;
                    memtable.evict_users(row, new_value.size());
                }
            } else if (memtable.row_sizes[row] + new_value.size() > memtable.available_memory) {
                if (old_version == num_checkpoints)
                    try_checkpoint(true);
                // cout << "case 2" << endl;
                memtable.evict_users(row, memtable.row_sizes[row] + new_value.size());
            }

            log({"CPUT", row, column, to_string(old_value.size()), to_string(new_value.size()), old_value, new_value});
            memtable.cput(row, column, old_value, new_value);
            modified[row].insert(column);
            ++num_operations;
            if (old_version == num_checkpoints)
                try_checkpoint();
            reply->set_response(ResponseCode::SUCCESS);
            all_lock.unlock();
            return Status::OK;
        }
    }

    // should only be called in the primary
    Status Recover(ServerContext* context, const RecoverRequest* request, RecoverReply* reply) {
        string replica_version = request->version();
        int num_iter = request->num_iter();
        if (num_iter == 0) {
            all_lock.lock();
            // recreate replica addresses since a secondary node died
            replicas.clear();
            for (string addr : replica_addresses) {
                grpc::ChannelArguments ch_args;
                ch_args.SetMaxReceiveMessageSize(-1);
                replicas.emplace_back(
                    KeyValueStoreClient(grpc::CreateCustomChannel(addr, grpc::InsecureChannelCredentials(), ch_args)));
            }
        }

        ifstream log_in(path + "/log");
        string curr_version;
        if (!getline(log_in, curr_version)) {  // log is empty
            log_in.close();
            all_lock.unlock();
            return Status::CANCELLED;
        }
        reply->set_version(curr_version);

        log_in.seekg(RPC_MSG_SIZE * num_iter, ios::cur);
        char* buf = new char[RPC_MSG_SIZE + 1];
        log_in.read(buf, RPC_MSG_SIZE);
        buf[log_in.gcount()] = '\0';
        if (log_in.peek() == EOF) {
            reply->set_is_end(true);
            if (curr_version == replica_version)
                all_lock.unlock();
        }
        log_in.close();
        string temp(buf);
        reply->set_value(temp);
        delete[] buf;
        if (curr_version == replica_version) {
            reply->set_version_match(true);
        }
        return Status::OK;
    }

    Status Disk(ServerContext* context, const DiskRequest* request, DiskReply* reply) {
        int recover_block_id = request->block_id();
        if (memtable.block_id == recover_block_id) {
            reply->set_is_end(true);
        }

        fstream block_fs(path + "/block" + to_string(recover_block_id));
        // get file size
        block_fs.seekg(0, ios::end);
        int length = block_fs.tellg();
        block_fs.clear();
        block_fs.seekg(0, ios::beg);
        // read block
        char* buf = new char[length + 1];
        block_fs.read(buf, length);
        buf[length] = '\0';
        string temp(buf);
        reply->set_value(temp);
        return Status::OK;
    }

    Status Metadata(ServerContext* context, const MetadataRequest* request, MetadataReply* reply) {
        ifstream metadata_in(path + "/metadata");
        string line;
        getline(metadata_in, line);
        auto tokens = tokenize(line, " ");
        reply->set_block_id(tokens[0]);
        reply->set_block_offset(tokens[1]);

        string value;
        while (getline(metadata_in, line)) {
            value += line + "\n";
        }
        reply->set_value(value);
        all_lock.unlock();
        return Status::OK;
    }

    Status Checkpoint(ServerContext* context, const CheckpointRequest* request, CheckpointReply* reply) {
        if (is_primary) {
            for (int i = 0; i < (int)replicas.size(); ++i) {
                // skip the secondary node that sent this request
                if (replica_addresses[i] == request->src_addr()) continue;
                replicas[i].Checkpoint();
            }
        }
        checkpoint();
        return Status::OK;
    }

    Status ColumnsForRow(ServerContext* context,
                         const ColumnsRequest* request,
                         ColumnsReply* reply)
    {
        string row = request->row();
        if (memtable.disk_map.find(row) == memtable.disk_map.end()) {
            reply->set_response(ResponseCode::MISSING_KEY);
            return Status::OK;
        }

        for (const auto &row_and_col_block : memtable.disk_map[row]) {
            string column_name = row_and_col_block.first;
            reply->add_column_names(column_name);
        }
        reply->set_response(ResponseCode::SUCCESS);
        return Status::OK;
    }
};

void run_server() {
    KeyValueStoreImpl service;
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    grpc::ServerBuilder builder;
    builder.SetMaxMessageSize(50000000);  // 50MB max message size
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    unique_ptr<Server> server(builder.BuildAndStart());
    cout << "Server listening on " << server_address << endl;
    server->Wait();
}

void recover_server() {
    //  get current log version number
    ifstream log_in(path + "/log");
    string version_num;
    if (!getline(log_in, version_num)) {
        cerr << "Log file is empty" << endl;
        return;
    }
    log_in.close();

    // get block filenames
    DIR* dir;
    struct dirent* diread;
    vector<string> files;
    if ((dir = opendir((path + "/").c_str())) != nullptr) {
        while ((diread = readdir(dir)) != nullptr) {
            files.push_back(string(diread->d_name));
        }
        closedir(dir);
    } else {
        cerr << "Directory not found" << endl;
        //  TODO: handle error
        return;
    }

    int max_block = 0;
    for (string file : files) {
        if (file.substr(0, 5) == "block") {
            int id = stoi(file.substr(5));
            if (id >= max_block) {
                max_block = id;
            }
        }
    }
    client_to_primary.Recover(version_num, max_block, 0);

    //  replay logs
    log_in.open(path + "/log");
    string line;
    getline(log_in, line);  // skip first line
    while (getline(log_in, line)) {
        string type = line;
        getline(log_in, line);  // row
        string row = line;
        getline(log_in, line);  // col
        string col = line;
        if (type == "PUT") {
            getline(log_in, line);  // value size
            int size = stoi(line);
            string value;
            value.resize(size);
            log_in.read(&value[0], size);
            getline(log_in, line);  // eat \n
            memtable.put(row, col, value);
        } else if (type == "CPUT") {
            getline(log_in, line);  // old value size
            int old_size = stoi(line);
            getline(log_in, line);  // new value size
            int new_size = stoi(line);
            string old_val;
            old_val.resize(old_size);
            log_in.read(&old_val[0], old_size);
            getline(log_in, line);  // eat newline
            string new_val;
            new_val.resize(new_size);
            log_in.read(&new_val[0], new_size);
            getline(log_in, line);  // eat newline
            memtable.cput(row, col, old_val, new_val);
        } else if (type == "DELE") {
            memtable.dele(row, col);
        }
        modified[row].insert(col);
    }
}

class HealthImpl final : public Health::Service {
    // heartbeat from the master node
    Status Alive(ServerContext* context, const AliveRequest* request, AliveReply* reply) {
        if (request->recover()) {
            all_lock.lock();
            recovering = 1;
        }
        string primary_address = request->primary();
        bool updated = request->updated_group();
        is_primary = (primary_address == server_address);

        if (startup || updated) {
            if (is_primary && request->replicas() != "") {
                replica_addresses = tokenize(request->replicas(), "\n");
                replicas.clear();
                for (string rep : replica_addresses) {
                    grpc::ChannelArguments ch_args;
                    ch_args.SetMaxReceiveMessageSize(-1);
                    replicas.emplace_back(
                        KeyValueStoreClient(grpc::CreateCustomChannel(rep, grpc::InsecureChannelCredentials(), ch_args)));
                }
            } else {
                grpc::ChannelArguments ch_args;
                ch_args.SetMaxReceiveMessageSize(-1);
                client_to_primary = KeyValueStoreClient(
                    grpc::CreateCustomChannel(primary_address, grpc::InsecureChannelCredentials(), ch_args));
            }
            startup = false;
        }
        if (request->recover()) {
            cerr << "start recovery" << endl;
            recover_server();
            recovering = 0;
            cerr << "done recovery" << endl;
            all_lock.unlock();
        }
        return Status::OK;
    }
};

void heartbeat() {
    HealthImpl service;
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    grpc::ServerBuilder builder;
    builder.AddListeningPort(heartbeat_addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    unique_ptr<Server> server(builder.BuildAndStart());
    cout << "Heartbeat monitor listening on " << heartbeat_addr << endl;
    server->Wait();
}

int main(int argc, char** argv) {
    // ./kv_store <server_addr> <heartbeat_addr>
    if (argc != 3) {
        fprintf(stderr, "Missing arguments\n");
        exit(1);
    }
    server_address = argv[1];
    heartbeat_addr = argv[2];
    path = "/home/cis505/cis505_final_project/" + server_address;
    memtable.dir = path;
    // allocate space for the first block if fresh start up
    ifstream log_in(path + "/log");
    string line;
    if (!getline(log_in, line)) {  // empty log
        log_in.close();
        int fd = open((path + "/block0").c_str(), O_CREAT | O_RDWR, 0666);
        posix_fallocate(fd, 0, BLOCK_SIZE);
        close(fd);
        ofstream log_out(path + "/log");
        log_out << num_checkpoints << endl;
        log_out.close();
    }
    thread heartbeat_th(heartbeat);
    cerr << "before while" << endl;
    while (recovering || startup)
        ;
    cerr << "recovering = false" << endl;
    run_server();
    cerr << "done" << endl;
    return 0;
}
