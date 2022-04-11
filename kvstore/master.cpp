#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>

#ifdef BAZEL_BUILD
#include "kvstore/protos/kvstore.grpc.pb.h"
#else
#include "kvstore.grpc.pb.h"
#endif

#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "kv_cache.hh"
#include "utils.hh"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using kvstore::AliveReply;
using kvstore::AliveRequest;
using kvstore::ChangeStatusReply;
using kvstore::ChangeStatusRequest;
using kvstore::Health;
using kvstore::MasterBackend;
using kvstore::NodeStatusReply;
using kvstore::NodeStatusRequest;
using kvstore::ResponseCode;
using kvstore::WhichReply;
using kvstore::WhichRequest;
using kvstore::RowsReply;
using kvstore::RowsRequest;

using namespace std;

#define GROUP_SIZE 3

mutex map_lock;
unordered_map<int, thread> thread_map;          // server_id -> thread
unordered_map<int, string> servers;             // server_id -> server_addr
unordered_set<int> server_ids_admin_down;       // server_ids that admin killed
unordered_map<int, string> heartbeat_map;       // <server_id, heartbeat_addr>
unordered_map<int, unordered_set<int>> groups;  // group_id -> {server_id}
unordered_map<int, int> primaries;              // group_id -> primary server_id
unordered_map<string, int> user_to_group;       // user -> group_id
unordered_map<int, int> rr_counter;             // round robin server assigner, <group_id, count>
unordered_set<int> dead_servers;                // {server_id}
unordered_set<int> init_servers;                // {server_id}
unordered_map<int, bool> changed_group;         // <group_id, {replicas}>

string config_file;
string local_path = "/home/cis505/cis505_final_project/";
string server_address = "localhost:5000";
volatile sig_atomic_t exit_flag = 0;
int group_cnt = 0;
unique_ptr<Server> server;

class HealthClient {
   private:
    std::unique_ptr<Health::Stub> stub_;

   public:
    HealthClient(shared_ptr<Channel> channel)
        : stub_(Health::NewStub(channel)) {}

    string Alive(int group_id, int server_id) {
        int primary_id = primaries[group_id];
        AliveRequest request;
        if (dead_servers.count(server_id)) {
            request.set_recover(true);
        }
        request.set_updated_group(changed_group[server_id]);
        changed_group[server_id] = false;
        request.set_primary(servers[primary_id]);
        string replicas;
        for (auto client_id : groups[group_id]) {
            if (client_id != primary_id && !dead_servers.count(client_id)) {
                replicas += servers[client_id] + "\n";
            }
        }
        request.set_replicas(replicas);

        AliveReply reply;
        chrono::time_point<chrono::system_clock> deadline = chrono::system_clock::now() + chrono::milliseconds(500);
        ClientContext context;
        context.set_deadline(deadline);
        Status status = stub_->Alive(&context, request, &reply);
        if (status.ok()) {
            return "ALIVE";
        }
        changed_group[server_id] = true;
        return "DEAD";
    }
};

// monitor storage nodes
void heartbeat(int server_id) {
    int group_id;
    for (auto const& x : groups) {
        if (x.second.count(server_id)) {  // found group id
            group_id = x.first;
        }
    }
    HealthClient monitor(grpc::CreateChannel(heartbeat_map[server_id], grpc::InsecureChannelCredentials()));
    while (!exit_flag) {
        this_thread::sleep_for(chrono::milliseconds(500));
        if (server_ids_admin_down.find(server_id) != server_ids_admin_down.end()) {
            continue;
        }
        string ret = monitor.Alive(group_id, server_id);
        if (init_servers.count(server_id)) {  // check if the server's started up
            if (ret == "ALIVE") {
                // lock_guard<mutex> guard(map_lock);
                cerr << "server " << server_id << " init" << endl;
                init_servers.erase(server_id);
            }
            continue;
        }
        // only do status checking if the server has started up
        lock_guard<mutex> guard(map_lock);
        if (!dead_servers.count(server_id) && ret == "DEAD") {  // storage node is down
            cerr << "server " << server_id << " is dead" << endl;
            dead_servers.insert(server_id);
            if (server_id == primaries[group_id]) {  // primary down, need to assign new primary
                bool found = false;
                for (auto const& x : groups[group_id]) {
                    if (!dead_servers.count(x)) {  // found another server
                        found = true;
                        primaries[group_id] = x;
                        for (auto const& y : groups[group_id]) {
                            changed_group[y] = true;
                        }
                        cerr << "server " << x << " is the primary for group " << group_id << " now" << endl;
                        break;
                    }
                }
                if (!found) {  // no alive servers in group
                    cerr << "server " << server_id << ": no servers in group " << group_id << " are alive" << endl;
                }
            }
        } else if (dead_servers.count(server_id) && ret == "ALIVE") {  // storage node is rebooted
            dead_servers.erase(server_id);
            for (auto const& x : groups[group_id]) {
                changed_group[x] = true;
            }
            // it's updated so the node can service requests again
            cerr << "server " << server_id << " is back up" << endl;
        }
    }
}

// clean up threads
void clean_up() {
    while (!exit_flag) {
        sleep(1);
    }
    server->Shutdown();
    for (auto& th : thread_map) {
        th.second.join();
    }
}

class MasterBackendImpl final : public MasterBackend::Service {
    Status Which(ServerContext* context, const WhichRequest* request, WhichReply* reply) {
        // lock_guard<mutex> guard(map_lock);
        string user = request->user();
        if (!user_to_group.count(user)) {  // new user
            user_to_group[user] = group_cnt++ % GROUP_SIZE;
        }
        int group_id = user_to_group[user];
        int server_id = (rr_counter[group_id] % GROUP_SIZE) + group_id * GROUP_SIZE;
        bool found_server = false;
        for (int i = 0; i < GROUP_SIZE; ++i) {  // find an alive server
            if (!dead_servers.count(server_id)) {
                found_server = true;
                break;
            }
            // cycles through the interval of server ids corresponding to the group
            // e.g. for GROUP_SIZE=3, group 0=[0,2], group1=[3,5], group2=[6,8]
            server_id = (++rr_counter[group_id] % GROUP_SIZE) + group_id * GROUP_SIZE;
        }
        cerr << "(" << group_id << ")" << user << " assigned to server " << server_id << endl;
        string server_addr = servers[server_id];
        if (!found_server) {  // no alive servers for this group
            reply->set_response(ResponseCode::FAILURE);
            return Status::OK;
        }
        reply->set_address(server_addr);
        // increment cnt for group
        rr_counter[group_id] = (rr_counter[group_id] + 1) % GROUP_SIZE;
        reply->set_response(ResponseCode::SUCCESS);
        return Status::OK;
    }

    Status NodeStatus(ServerContext* context,
                      const NodeStatusRequest* request,
                      NodeStatusReply* reply) {
        vector<string> ret_servers;
        vector<bool> servers_alive;

        for (const auto& pair : servers) {
            reply->add_node_names(pair.second);
            // Assume server is alive if not in dead servers
            reply->add_nodes_alive(
                dead_servers.find(pair.first) == dead_servers.end());
        }
        reply->set_response(ResponseCode::SUCCESS);

        return Status::OK;
    }

    Status NodeTurnOff(ServerContext* context,
                       const ChangeStatusRequest* request,
                       ChangeStatusReply* reply) {
        string server_addr = request->node_name();
        for (const auto& server : servers) {
            if (server.second == server_addr) {
                lock_guard<mutex> guard(map_lock);
                server_ids_admin_down.insert(server.first);
                dead_servers.insert(server.first);
                int group_id;
                for (auto const& x : groups) {
                    if (x.second.count(server.first)) {  // found group id
                        group_id = x.first;
                    }
                }
                for (auto const& x : groups[group_id]) {
                    changed_group[x] = true;
                }

                cerr << "server " << server.first << " is turned off" << endl;
                if (server.first == primaries[group_id]) {  // primary down, need to assign new primary
                    bool found = false;
                    for (auto const& x : groups[group_id]) {
                        if (!dead_servers.count(x)) {  // found another server
                            found = true;
                            primaries[group_id] = x;
                            cerr << "server " << x << " is the primary for group " << group_id << " now" << endl;
                            break;
                        }
                    }
                    if (!found) {  // no alive servers in group
                        cerr << "server " << server.first << ": no servers in group " << group_id << " are alive" << endl;
                    }
                }
                break;
            }
        }

        reply->set_response(ResponseCode::SUCCESS);
        return Status::OK;
    }

    Status NodeTurnOn(ServerContext* context,
                      const ChangeStatusRequest* request,
                      ChangeStatusReply* reply) {
        string server_addr = request->node_name();
        for (const auto& server : servers) {
            if (server.second == server_addr) {
                lock_guard<mutex> guard(map_lock);
                server_ids_admin_down.erase(server.first);
                int group_id;
                for (auto const& x : groups) {
                    if (x.second.count(server.first)) {  // found group id
                        group_id = x.first;
                    }
                }
                for (auto const& x : groups[group_id]) {
                    changed_group[x] = true;
                }
                break;
            }
        }

        reply->set_response(ResponseCode::SUCCESS);
        return Status::OK;
    }

    Status GetRows(ServerContext* context,
                   const RowsRequest* request,
                   RowsReply* reply)
    {
        for (const auto user_group : user_to_group) {
            string row = user_group.first;
            reply->add_row_names(row);
        }
        reply->set_response(ResponseCode::SUCCESS);
        return Status::OK;
    }
};

void run_master() {
    MasterBackendImpl service;
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    server = move(unique_ptr<Server>(builder.BuildAndStart()));
    cout << "Server listening on " << server_address << endl;
    server->Wait();
}

void configure() {
    int server_id = 0;
    int group_id = 0;
    fstream config_fs(config_file);
    string line;
    int count = 0;
    while (getline(config_fs, line)) {
        // cout << line << endl;
        // line = "main_addr heartbeat_addr"
        auto tokens = tokenize(line, " ");
        string server_addr = tokens[0];
        string heartbeat_addr = tokens[1];
        servers[server_id] = server_addr;
        heartbeat_map[server_id] = heartbeat_addr;
        groups[group_id].insert(server_id);
        if (count == 0) {
            primaries[group_id] = server_id;
        }
        if (++count == GROUP_SIZE) {
            group_id++;
            count = 0;
        }
        init_servers.insert(server_id++);
        // deletes existing folders if they exist and then creates them
        nftw((local_path + server_addr).c_str(), rm_files, 10, FTW_DEPTH | FTW_MOUNT | FTW_PHYS);
        int status = mkdir((local_path + server_addr).c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
        if (status < 0) cerr << "mkdir error" << endl;
    }
}

void sig_handler(int sig) {
    if (sig == SIGINT) {
        exit_flag = 1;
    }
}

int main(int argc, char** argv) {
    struct sigaction a;
    a.sa_handler = sig_handler;
    a.sa_flags = 0;
    sigemptyset(&a.sa_mask);
    sigaction(SIGINT, &a, NULL);

    if (argc != 2) {
        fprintf(stderr, "Missing server config\n");
        exit(1);
    }
    config_file = local_path + argv[1];
    configure();
    for (auto const& x : servers) {  // heartbeat thread for each storage node
        thread_map[x.first] = move(thread(heartbeat, x.first));
    }
    thread shutdown_th(clean_up);
    run_master();
    shutdown_th.join();
    return 0;
}
