#include <iostream>
#include <string>

#include "app_client.h"
#include "../utils/utils.h"
#include "../types/request_types.h"
#include <grpc/support/time.h>
#include "../constants.h"
#include "absl/log/check.h"

using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;

using paxos::Paxos;
using paxos::Proposal;
using paxos::Logs;
using paxos::Transaction;
using paxos::TransferReq;
using paxos::TransferRes;
using paxos::PrepareReq;
using paxos::PrepareRes;
using paxos::AcceptRes;
using paxos::AcceptRes;
using paxos::CommitReq;

    
AppClient::AppClient() {
    for (auto it = Constants::serverAddresses.begin(); it != Constants::serverAddresses.end(); it++) {
        std::string server = it->first;
        std::string targetAddress = it->second;
        stubs_.push_back(Paxos::NewStub(grpc::CreateChannel(targetAddress, grpc::InsecureChannelCredentials())));
    }

    transactionsIssued = 0;
}


void AppClient::consumeTransferReplies() {
    void* tag;
    bool ok;

    while(cq_.Next(&tag, &ok)) {
        ClientCall* call = static_cast<ClientCall*>(tag);
        CHECK(ok);
        
        if (call->status.ok() && call->callType_ == types::TRANSFER) {
            std::unique_lock<std::shared_mutex> lock(performanceMutex);
            std::unique_lock<std::shared_mutex> lock2(transferringServersMutex);
            transactionsProcessed++;
            
            int tid = call->transferReply.id();
            std::string serverId = call->transferReply.server_id();
            transferringServers.erase(serverId);
            
            std::chrono::system_clock::time_point endTime = std::chrono::system_clock::now();
            double diff = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTimes[tid]).count();
            startTimes.erase(tid);
            
            totalTimeTaken += diff;
            performance = (1000 * transactionsProcessed) / totalTimeTaken;
        } else {
            std::cout << "Transfer RPC failed " << call->status.error_message() << std::endl;
        }

        delete call;
    }
}


void AppClient::processTransactions(std::vector<types::Transaction> transactions) {
    int requestsIssued = 0;
    std::vector<bool> issued(transactions.size(), false);

    while (requestsIssued < transactions.size()) {
        for (int i = 0; i < transactions.size(); i++) {
            auto& t = transactions[i];
            if (issued[i] || transferringServers.find(t.sender) != transferringServers.end()) {
                continue;
            } else {
                std::shared_lock<std::shared_mutex> lock(transferringServersMutex);
                // std::cout << "issuing " << t.sender << ", " << t.receiver << ", " << t.amount << std::endl;
                transactionsIssued++;
                transferringServers.insert(t.sender);
                startTimes[transactionsIssued] = std::chrono::system_clock::now();
                sendTransferAsync(transactionsIssued, t.sender, t.receiver, t.amount);
                issued[i] = true;
                requestsIssued++;
            }
        }
    }
}

void AppClient::GetBalance(std::string serverName, int& res) {
    sendGetBalanceAsync(serverName);

    void* tag;
    bool ok;
    while (true) {  // wait for balance request to complete
        grpc::CompletionQueue::NextStatus responseStatus = cq_.AsyncNext(&tag, &ok, gpr_time_0(GPR_CLOCK_REALTIME));
        if (responseStatus == grpc::CompletionQueue::NextStatus::GOT_EVENT) {
            ClientCall* call = static_cast<ClientCall*>(tag);
            if (ok && call->callType_ == types::GET_BALANCE) {
                if (!call->status.ok()) {
                    std::cout << call->status.error_message() << std::endl;
                } else {
                    res = call->getBalanceReply.amount();
                    delete call;
                    break;
                }

                delete call;
            } 
        } 
    }
}

void AppClient::GetLogs(std::string serverName, std::vector<types::Transaction>& logs) {
    sendGetLogsAsync(serverName);

    void* tag;
    bool ok;
    while (true) {  // wait for get logs request to complete
        grpc::CompletionQueue::NextStatus responseStatus = cq_.AsyncNext(&tag, &ok, gpr_time_0(GPR_CLOCK_REALTIME));
        if (responseStatus == grpc::CompletionQueue::NextStatus::GOT_EVENT) {
            ClientCall* call = static_cast<ClientCall*>(tag);
            if (ok && call->callType_ == types::GET_LOGS) {
                if (!call->status.ok()) {
                    std::cout << call->status.error_message() << std::endl;
                } else {
                    for (int i = 0; i < call->getLogsReply.logs_size(); i++) {
                        const Transaction& t = call->getLogsReply.logs(i);
                        logs.push_back({ t.id(), t.sender(), t.receiver(), t.amount() });
                    }

                    delete call;
                    break;
                }

                delete call;
            }
        } 
    }
}

void AppClient::GetDBLogs(std::string serverName, std::vector<types::TransactionBlock>& blocks) {
    sendGetDBLogsAsync(serverName);

    void* tag;
    bool ok;

    while (true) {
        grpc::CompletionQueue::NextStatus responseStatus = cq_.AsyncNext(&tag, &ok, gpr_time_0(GPR_CLOCK_REALTIME));
        if (responseStatus == grpc::CompletionQueue::NextStatus::GOT_EVENT) {
            ClientCall* call = static_cast<ClientCall*>(tag);
            if (ok && call->callType_ == types::GET_DB_LOGS) {
                if (!call->status.ok()) {
                    std::cout << call->status.error_message() << std::endl;
                } else {
                    for (int i = 0; i < call->getDBLogsReply.blocks_size(); i++) {
                        const TransactionBlock& b = call->getDBLogsReply.blocks(i);
                        types::TransactionBlock block;
                        block.id = b.block_id();
                        block.commitProposal.proposalNum = b.proposal().number();
                        block.commitProposal.serverName = b.proposal().server_id();

                        for (int j = 0; j < b.logs().logs_size(); j++) {
                            const Transaction& t = b.logs().logs(j);
                            block.block.push_back({ t.id(), t.sender(), t.receiver(), t.amount() });
                        }

                        blocks.push_back(block);
                    }

                    delete call;
                    break;
                }

                delete call;
            }
        } 
    }
}

double AppClient::GetPerformance() {
    std::shared_lock<std::shared_mutex> lock(performanceMutex);
    std::cout << "Transactions Processed: " << transactionsIssued << std::endl;
    std::cout << "Time taken (ms): " << totalTimeTaken << std::endl;
    return performance;
}

int AppClient::getServerIdFromName(std::string serverName) {
    return serverName[1] - '0';
}


bool AppClient::sendTransferAsync(int transaction_id, std::string sender, std::string receiver, int amount) {
    int serverId = getServerIdFromName(sender);
    
    TransferReq request;
    request.set_id(transaction_id);
    request.set_receiver(receiver);
    request.set_amount(amount);
    
    ClientCall* call = new ClientCall;
    call->callType_ = types::TRANSFER;
    call->targetServer = sender;
    call->transferResponseReader = stubs_[serverId - 1]->PrepareAsyncTransfer(&call->context, request, &cq_);
    call->transferResponseReader->StartCall();
    call->transferResponseReader->Finish(&call->transferReply, &call->status, (void*)call);

    return true;
}

bool AppClient::sendGetBalanceAsync(std::string serverName) {
    int serverId = getServerIdFromName(serverName);
    
    google::protobuf::Empty request;
    
    ClientCall* call = new ClientCall;
    call->callType_ = types::GET_BALANCE;
    call->targetServer = serverName;
    call->balanceResponseReader = stubs_[serverId - 1]->PrepareAsyncGetBalance(&call->context, request, &cq_);
    call->balanceResponseReader->StartCall();
    call->balanceResponseReader->Finish(&call->getBalanceReply, &call->status, (void*)call);

    return true;
}

bool AppClient::sendGetLogsAsync(std::string serverName) {
    int serverId = getServerIdFromName(serverName);
    
    google::protobuf::Empty request;
    
    ClientCall* call = new ClientCall;
    call->callType_ = types::GET_LOGS;
    call->targetServer = serverName;
    call->logsResponseReader = stubs_[serverId - 1]->PrepareAsyncGetLogs(&call->context, request, &cq_);
    call->logsResponseReader->StartCall();
    call->logsResponseReader->Finish(&call->getLogsReply, &call->status, (void*)call);

    return true;
}

bool AppClient::sendGetDBLogsAsync(std::string serverName) {
    int serverId = getServerIdFromName(serverName);
    
    google::protobuf::Empty request;
    
    ClientCall* call = new ClientCall;
    call->callType_ = types::GET_DB_LOGS;
    call->targetServer = serverName;
    call->dbLogsResponseReader = stubs_[serverId - 1]->PrepareAsyncGetDBLogs(&call->context, request, &cq_);
    call->dbLogsResponseReader->StartCall();
    call->dbLogsResponseReader->Finish(&call->getDBLogsReply, &call->status, (void*)call);

    return true;
}
