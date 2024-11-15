#include "app_client.h"
#include "constants.h"

using grpc::ClientContext;
using grpc::Status;
using google::protobuf::Empty;

using pbft::Transactions;
using pbft::Transaction;
using pbft::TransactionData;
using pbft::GetLogRes;
using pbft::GetDBRes;
using pbft::GetStatusReq;
using pbft::GetStatusRes;
using pbft::ViewChangesResEntry;
using pbft::GetViewChangesRes;;
using pbft::GetPerformanceRes;


AppClient::AppClient() {
    for (auto it = Constants::serverAddresses.begin(); it != Constants::serverAddresses.end(); it++) {
        std::string server = it->first;
        std::string targetAddress = it->second;
        serverStubs_.push_back(PbftServer::NewStub(grpc::CreateChannel(targetAddress, grpc::InsecureChannelCredentials())));    
    }

    for (auto it = Constants::clientAddresses.begin(); it != Constants::clientAddresses.end(); it++) {
        std::string client = it->first;
        std::string targetAddress = it->second;
        clientStubs_.push_back(PbftClient::NewStub(grpc::CreateChannel(targetAddress, grpc::InsecureChannelCredentials())));    
    }

    rpcTimeoutSeconds = 2;
}

void AppClient::ProcessTransactions(std::vector<types::Transaction>& transactions) {
    Transactions txns;
    
    for (auto& t: transactions) {
        Transaction* tx = txns.add_transactions();
        tx->set_sender(t.sender);
        tx->set_receiver(t.receiver);
        tx->set_amount(t.amount);
    }

    for (auto& stub_: clientStubs_) {
        ClientContext context;
        Empty reply;
        Status status = stub_->Process(&context, txns, &reply);
    }
}

void AppClient::GetLogs(std::string serverName, std::vector<types::PbftLogEntry>& logs, types::ServerInfo& info) {
    int serverId = serverName[1] - '1';
    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(rpcTimeoutSeconds));
    Empty request;
    GetLogRes reply;

    Status status = serverStubs_[serverId]->GetLog(&context, request, &reply);
    if (status.ok()) {
        for (int i = 0; i < reply.entries_size(); i++) {
            logs.push_back({
                { i, reply.entries(i).t().sender(), reply.entries(i).t().receiver(), reply.entries(i).t().amount() },
                reply.entries(i).matching_prepares(),
                reply.entries(i).matching_commits(),
                reply.entries(i).valid()
            });
        }

        info.lastCommitted = reply.last_committed();
        info.lastExecuted = reply.last_executed();
        info.lastCheckpoint = reply.last_checkpoint();
        info.lastStableCheckpoint = reply.last_stable_checkpoint();

    }
}

void AppClient::GetDb(std::vector<std::vector<std::string>>& db, std::vector<std::string>& aliveServers) {
    for (int i = 0; i < Constants::serverNames.size(); i++) {
        Empty request;
        GetDBRes reply;
        std::vector<std::string> balances = std::vector<std::string>();
    
        ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(rpcTimeoutSeconds));
        Status status = serverStubs_[i]->GetDb(&context, request, &reply);
        if (status.ok()) {
            for (int j = 0; j < Constants::clientAddresses.size(); j++) {
                balances.push_back(std::to_string(reply.balances(j)));
            }
        } else {
            for (int j = 0; j < Constants::clientAddresses.size(); j++) {
                balances.push_back("-");
            }
        }

        db.push_back(balances);
    }
}

void AppClient::GetStatus(int seqNum, std::vector<std::string>& status) {
    GetStatusReq request;
    request.set_seq_num(seqNum);

    for (int i = 0; i < serverStubs_.size(); i++) {
        ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(rpcTimeoutSeconds));
        GetStatusRes reply;

        Status s = serverStubs_[i]->GetStatus(&context, request, &reply);
        if (s.ok()) {
            status.push_back(reply.status());
        } else {
            status.push_back("-");
        }
    }
}

// void AppClient::GetViewChanges(std::string serverName, std::vector<types::ViewChangeInfo>& viewChanges) {
//     // Pick any of the alive servers which is non-byzantine
//     ClientContext context;
//     Empty request;
//     GetViewChangesRes reply;

//     int serverId = serverName[1] - '1';
//     Status status = serverStubs_[serverId]->GetViewChanges(&context, request, &reply);
//     if (status.ok()) {
//             for (int i = 0; i < reply.view_changes_size(); i++) {
//             const ViewChangesResEntry& e = reply.view_changes(i);
//             // std::vector<int> preparedEntries;
//             // for (int j = 0; j < e.prepared_entries_size(); j++) {
//                 // preparedEntries.push_back(e.prepared_entries(j));
//             // }

//             viewChanges.push_back({
//                 e.view_num(),
//                 e.stable_checkpoint(),
//                 e.initiator()
//                 // preparedEntries
//             });
//         }
//     } else {
//         std::cout << "GetViewChanges RPC failed" << std::endl;
//     }
// }

double AppClient::GetPerformance() {
    Empty request;
    double res = 0.0;
    int count = 0;

    for (int i = 0; i < clientStubs_.size(); i++) {
        ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(rpcTimeoutSeconds));
        GetPerformanceRes reply;

        Status s = clientStubs_[i]->GetPerformance(&context, request, &reply);
        if (s.ok()) {
            if (reply.performance() > 0.0) {
                res += reply.performance();
                count += 1;
            }
        }
    }

    return res / count;
}