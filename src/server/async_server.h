#pragma once

#include <string.h>
#include <grpcpp/grpcpp.h>
#include "paxos.grpc.pb.h"
#include <shared_mutex>
#include "../types/transaction.h"
#include <chrono>

using grpc::Server;
using grpc::CompletionQueue;
using grpc::ServerCompletionQueue;

using paxos::Paxos;
using paxos::Proposal;
using paxos::Transaction;
using paxos::TransferReq;
using paxos::TransferRes;
using paxos::Balance;
using paxos::Logs;
using paxos::DBLogs;
using paxos::TransactionBlock;
using paxos::PrepareReq;
using paxos::PrepareRes;
using paxos::AcceptReq;
using paxos::AcceptRes;
using paxos::CommitReq;
using paxos::SyncReq;
using paxos::SyncRes;

// Server Implementation
class PaxosServer final {
public:
    PaxosServer(std::string serverName);
    void run(std::string targetAddress);
    ~PaxosServer();
    void HandleRPCs();

    void handlePrepareReply(PrepareRes& reply);
    void handleAcceptReply(AcceptRes& reply);
    void handleSyncReply(SyncRes& reply);
    
    bool processTransferCall(TransferReq* req, TransferRes* res);
    bool processGetBalanceCall(Balance* res);
    bool processGetLogsCall(Logs* logs);
    bool processGetDBLogsCall(DBLogs* blocks);
    bool processPrepareCall(PrepareReq* req, PrepareRes* res);
    bool processAcceptCall(AcceptReq* req, AcceptRes* res);
    bool processCommitCall(CommitReq* req);
    bool processSyncCall(SyncReq* req, SyncRes* res);

    // bool isProposer();
    


private:
    std::string serverName;

    // Completion Queue for incoming requests to the server
    std::unique_ptr<ServerCompletionQueue> requestCQ;

    // Completion Queue for responses to server's prepare, accept, and commit requests
    std::unique_ptr<CompletionQueue> responseCQ;

    Paxos::AsyncService service_;
    std::unique_ptr<Server> server_;

    // Stubs for sending prepare, accept, and commit RPCs to other servers  
    std::vector<std::unique_ptr<Paxos::Stub>> stubs_;

    int rpcTimeoutSeconds;

    enum ServerState { IDLE, PREPARE, PROPOSE, COMMIT, PROMISED, ACCEPTED, SYNC };
    ServerState currentState_;
    
    int balance;
    int currentTransactionNum;
    std::chrono::time_point<std::chrono::system_clock> consensusDeadline;
    const static int minBackoffMs = 100;
    const static int maxBackoffMs = 200;
    std::vector<types::Transaction> localLogs;

    types::Proposal myProposal;

    /* For Prepare phase */
    // highest accept num and accept val received in prepare replies (should be reset before prepare phase)
    types::Proposal highestAcceptNum;
    std::vector<types::Transaction> highestAcceptVal;
    
    // logs from other servers received in prepare replies
    std::vector<types::Transaction> remoteLogs;

    /* For Accept phase */
    // accept num and accept val at current node
    types::Proposal acceptNum;
    std::vector<types::Transaction> acceptVal;

    /* For commit phase */
    int lastCommittedBlock;
    types::Proposal lastCommittedProposal;
    std::vector<types::TransactionBlock> committedBlocks;
    
    // variables for tracking the state of consensus
    const static int MAJORITY = 3;
    int prepareSuccesses;
    int prepareFailures;
    int acceptSuccesses;
    int acceptFailures;
    int commitSuccesses;
    int commitFailures;
    bool awaitPrepareDecision;
    bool awaitAcceptDecision;
    bool awaitCommitDecision;
    bool transferSync;

    std::string dbFilename;

    int getServerIdFromName(std::string serverName);
    void sendPrepareToCluster(PrepareReq& request);
    void sendAcceptToCluster(AcceptReq& request);
    void sendCommitToCluster(CommitReq& request);
    void requestSync(std::string serverName);

    void sendPrepareAsync(PrepareReq& request, std::unique_ptr<Paxos::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline);
    void sendAcceptAsync(AcceptReq& request, std::unique_ptr<Paxos::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline);
    void sendCommitAsync(CommitReq& request, std::unique_ptr<Paxos::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline);
    void sendSyncAsync(SyncReq& request, std::unique_ptr<Paxos::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline);

    void copyProposal(Proposal* to, types::Proposal from);
    void copyLogs(Logs* logs, std::vector<types::Transaction>& val);
    std::vector<types::Transaction> getLogsForProposal();
    void commitAcceptVal();
    void replicateBlock(std::string serverName, int blockId);

    void doCommit(int commitProposalNum, std::string commitProposalServer, std::vector<types::Transaction> logs);
    void setupDB();
    void getLocalLogsDB();
    void storeLocalLogDB(types::Transaction t);
    void refreshLocalLogsDB();
    void getSavedStateDB();
    void saveStateDB();
    void getCommittedLogsDB();
    void commitLogsToDB(std::vector<types::Transaction> logs);

    void resetConsensusDeadline();
};