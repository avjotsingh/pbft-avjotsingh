#include <string>
#include <vector>
#include <map>
#include "types/transaction.h"

#include <grpcpp/grpcpp.h>
#include "pbft.grpc.pb.h"

using pbft::PbftServer;
using pbft::PbftClient;

class AppClient {
public:
    AppClient();
    void ProcessTransactions(std::vector<types::Transaction>& transactions);
    void GetLogs(std::string serverName, std::vector<types::PbftLogEntry>& logs);
    void GetDb(std::vector<std::vector<std::string>>& db, std::vector<std::string>& aliveServers);
    void GetStatus(int sequenceNumber, std::vector<std::string>& status);
    void GetViewChanges(std::string serverName, std::vector<types::ViewChangeInfo>& viewChanges);
    double GetPerformance();

private:
    std::vector<std::unique_ptr<PbftServer::Stub>> serverStubs_;
    std::vector<std::unique_ptr<PbftClient::Stub>> clientStubs_;
};