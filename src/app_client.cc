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

// TODO implement get performance

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
}

void AppClient::ProcessTransactions(std::vector<types::Transaction>& transactions) {
    ClientContext context;
    Transactions txns;
    Empty reply;

    for (auto& t: transactions) {
        Transaction* tx = txns.add_transactions();
        tx->set_sender(t.sender);
        tx->set_receiver(t.receiver);
        tx->set_amount(t.amount);
    }

    for (auto& stub_: clientStubs_) {
        Status status = stub_->Process(&context, txns, &reply);
        if (!status.ok()) {
            std::cout << "Process RPC failed" << std::endl;
        }
    }
}

void AppClient::GetLogs(std::string serverName, std::vector<types::PbftLogEntry>& logs) {
    int serverId = serverName[1] - '1';
    ClientContext context;
    Empty request;
    GetLogRes reply;

    Status status = serverStubs_[serverId]->GetLog(&context, request, &reply);
    if (status.ok()) {
        for (int i = 0; i < reply.entries_size(); i++) {
            logs.push_back({
                { i, reply.entries(i).t().sender(), reply.entries(i).t().receiver(), reply.entries(i).t().amount() },
                reply.entries(i).matching_prepares(),
                reply.entries(i).matching_commits()
            });
        }
    } else {
        std::cout << "GetLog RPC failed" << std::endl;
    }
}

void AppClient::GetDb(std::vector<std::vector<std::string>>& db, std::vector<std::string>& aliveServers) {
    ClientContext context;
    for (int i = 0; i < Constants::serverNames.size(); i++) {
        Empty request;
        GetDBRes reply;
        std::vector<std::string> balances = std::vector<std::string>();
        
        if (std::find(aliveServers.begin(), aliveServers.end(), Constants::serverNames[i]) == aliveServers.end()) {
            for (int j = 0; j < Constants::clientAddresses.size(); j++) {
                balances.push_back("-");
            }
        } else {
            Status status = serverStubs_[i]->GetDb(&context, request, &reply);
            for (int j = 0; j < Constants::clientAddresses.size(); j++) {
                balances.push_back(std::to_string(reply.balances(j)));
            }
        }

        db.push_back(balances);
    }
}

void AppClient::GetStatus(int seqNum, std::vector<std::string>& status) {
    ClientContext context;
    GetStatusReq request;
    request.set_seq_num(seqNum);
    GetStatusRes reply;

    for (int i = 0; i < serverStubs_.size(); i++) {
        Status s = serverStubs_[i]->GetStatus(&context, request, &reply);
        if (s.ok()) {
            status.push_back(reply.status());
        } else {
            std::cout << "GetStatus RPC failed" << std::endl;
        }
    }
}

void AppClient::GetViewChanges(std::string serverName, std::vector<types::ViewChangeInfo>& viewChanges) {
    // Pick any of the alive servers which is non-byzantine
    ClientContext context;
    Empty request;
    GetViewChangesRes reply;

    int serverId = serverName[1] - '1';
    Status status = serverStubs_[serverId]->GetViewChanges(&context, request, &reply);
    if (status.ok()) {
            for (int i = 0; i < reply.view_changes_size(); i++) {
            const ViewChangesResEntry& e = reply.view_changes(i);
            // std::vector<int> preparedEntries;
            // for (int j = 0; j < e.prepared_entries_size(); j++) {
                // preparedEntries.push_back(e.prepared_entries(j));
            // }

            viewChanges.push_back({
                e.view_num(),
                e.stable_checkpoint(),
                e.initiator()
                // preparedEntries
            });
        }
    } else {
        std::cout << "GetViewChanges RPC failed" << std::endl;
    }
}

double AppClient::GetPerformance() {
    return 0.0;
}