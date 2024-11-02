#pragma once

#include <grpcpp/grpcpp.h>
#include <grpcpp/alarm.h>
#include "paxos.grpc.pb.h"
#include "../types/request_types.h"
#include "../utils/utils.h"
#include "async_server.h"

using grpc::ClientAsyncResponseReader;
using grpc::CompletionQueue;
using grpc::ClientContext;
using google::protobuf::Empty;
using grpc::Alarm;
using grpc::Status;

using paxos::Paxos;
using paxos::Transaction;
using paxos::TransferReq;
using paxos::TransferRes;
using paxos::Balance;
using paxos::Logs;
using paxos::PrepareReq;
using paxos::PrepareRes;
using paxos::AcceptReq;
using paxos::AcceptRes;
using paxos::CommitReq;
using paxos::SyncReq;
using paxos::SyncRes;

// struct for keeping state and data information
class PaxosClientCall {

public:
    PaxosClientCall(PaxosServer* server, CompletionQueue* cq, types::RequestTypes callType);
    void sendPrepare(PrepareReq& req, std::unique_ptr<Paxos::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline);
    void sendAccept(AcceptReq& req, std::unique_ptr<Paxos::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline);
    void sendCommit(CommitReq& req, std::unique_ptr<Paxos::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline);
    void sendSync(SyncReq& req, std::unique_ptr<Paxos::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline);
    void HandleRPCResponse();

private:
    PaxosServer* server_;
    CompletionQueue* cq_;
    types::RequestTypes callType_;

    // Container for the data we expect from the server.
    PrepareRes prepareReply;
    AcceptRes acceptReply;
    google::protobuf::Empty commitReply;
    SyncRes syncReply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // Storage for the status of the RPC upon completion.
    Status status;

    std::unique_ptr<ClientAsyncResponseReader<PrepareRes>> prepareResponseReader;
    std::unique_ptr<ClientAsyncResponseReader<AcceptRes>> acceptResponseReader;
    std::unique_ptr<ClientAsyncResponseReader<google::protobuf::Empty>> commitResponseReader;
    std::unique_ptr<ClientAsyncResponseReader<SyncRes>> syncResponseReader;
};

