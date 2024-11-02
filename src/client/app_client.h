#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <chrono>
#include <shared_mutex>

#include <grpcpp/grpcpp.h>
#include "paxos.grpc.pb.h"

#include "../types/request_types.h"
#include "../types/transaction.h"

using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;
using paxos::Paxos;
using paxos::PrepareRes;
using paxos::AcceptRes;
using paxos::TransferRes;
using paxos::Balance;
using paxos::Logs;
using paxos::DBLogs;
using paxos::TransactionBlock;


class AppClient {
public:
    AppClient();
    void processTransactions(std::vector<types::Transaction> transactions);
    void GetBalance(std::string serverName, int& res);
    void GetLogs(std::string serverName, std::vector<types::Transaction>& logs);
    void GetDBLogs(std::string serverName, std::vector<types::TransactionBlock>& blocks);
    double GetPerformance();
    void consumeTransferReplies();
    
    
private:
    int getServerIdFromName(std::string serverName);
    void checkAndConsumeTransferReply();
    bool sendTransferAsync(int transaction_id, std::string sender, std::string receiver, int amount);
    bool sendGetLogsAsync(std::string serverName);
    bool sendGetDBLogsAsync(std::string serverName);
    bool sendGetBalanceAsync(std::string serverName);
    

    struct ClientCall {
        types::RequestTypes callType_;
        std::string targetServer;

        // Container for the data we expect from the server.
        TransferRes transferReply;
        Balance getBalanceReply;
        Logs getLogsReply;
        DBLogs getDBLogsReply;

        // Context for the client. It could be used to convey extra information to
        // the server and/or tweak certain RPC behaviors.
        ClientContext context;

        // Storage for the status of the RPC upon completion.
        Status status;

        std::unique_ptr<ClientAsyncResponseReader<TransferRes>> transferResponseReader;
        std::unique_ptr<ClientAsyncResponseReader<Balance>> balanceResponseReader;
        std::unique_ptr<ClientAsyncResponseReader<Logs>> logsResponseReader;
        std::unique_ptr<ClientAsyncResponseReader<DBLogs>> dbLogsResponseReader;
    };

    int currentView;
    const static int numServers = 7;

    // servers' exposed services.
    std::vector<std::unique_ptr<Paxos::Stub>> stubs_;

    // The producer-consumer queue we use to communicate asynchronously with the
    // gRPC runtime.
    CompletionQueue cq_;

    double performance;
    std::shared_mutex performanceMutex;
    
    int transactionsIssued;
    int transactionsProcessed;

    std::map<int, std::chrono::system_clock::time_point> startTimes;
    double totalTimeTaken;
    std::set<std::string> transferringServers;
    std::shared_mutex transferringServersMutex;
};