#include "async_server.h"
#include "../types/request_types.h"
#include <shared_mutex>
#include "call_data.h"
#include "absl/log/check.h"
#include <chrono>
#include "paxos_client_call.h"
#include <grpc/support/time.h>
#include "../constants.h"
#include <set>
#include <random>
#include <iostream>
#include <fstream>
#include <SQLiteCpp/SQLiteCpp.h>
#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Statement.h>
#include <SQLiteCpp/Exception.h>
#include <algorithm>

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::Status;
using google::protobuf::Empty;
using grpc::CompletionQueue;

// Server Implementation
PaxosServer::PaxosServer(std::string serverName) {
  srand(serverName[1] - '0');
  this->serverName = serverName;
  
  balance = 100;
  currentTransactionNum = 0;
  rpcTimeoutSeconds = 1;
  
  myProposal.proposalNum = 0;
  myProposal.serverName = serverName;
  acceptNum.proposalNum = 0;
  acceptNum.serverName = "";
  highestAcceptNum.proposalNum = 0;
  highestAcceptNum.serverName = "";
  highestAcceptVal = std::vector<types::Transaction>();

  localLogs = std::vector<types::Transaction>();
  remoteLogs = std::vector<types::Transaction>();
  acceptVal = std::vector<types::Transaction>();
  
  lastCommittedBlock = 0;
  lastCommittedProposal.proposalNum = 0;
  lastCommittedProposal.serverName = "";
  committedBlocks = std::vector<types::TransactionBlock>();

  prepareSuccesses = 0;
  prepareFailures = 0;
  acceptSuccesses = 0;
  acceptFailures = 0;
  commitSuccesses = 0;
  commitFailures = 0;
  awaitPrepareDecision = false;
  awaitAcceptDecision = false;
  awaitCommitDecision = false;

  currentState_ = IDLE;
  
  dbFilename = serverName + ".db";
  
  setupDB();
  getSavedStateDB();
  getLocalLogsDB();
  getCommittedLogsDB();
}

void PaxosServer::run(std::string targetAddress) {
  ServerBuilder builder;
  builder.AddListeningPort(targetAddress, grpc::InsecureServerCredentials());
  builder.RegisterService(&service_);
  requestCQ = builder.AddCompletionQueue();
  responseCQ = std::make_unique<CompletionQueue>();
  server_ = builder.BuildAndStart();

  for (auto it = Constants::serverAddresses.begin(); it != Constants::serverAddresses.end(); it++) {
    std::string server = it->first;
    std::string targetAddress = it->second;
    stubs_.push_back(Paxos::NewStub(grpc::CreateChannel(targetAddress, grpc::InsecureChannelCredentials())));    
  }

  std::cout << "Server running on " << targetAddress << std::endl;
  HandleRPCs();
}

PaxosServer::~PaxosServer() {
  server_->Shutdown();
  requestCQ->Shutdown();
  responseCQ->Shutdown();
}

void PaxosServer::HandleRPCs() {
  new CallData(&service_, this, requestCQ.get(), types::TRANSFER);
  new CallData(&service_, this, requestCQ.get(), types::GET_BALANCE);
  new CallData(&service_, this, requestCQ.get(), types::GET_LOGS);
  new CallData(&service_, this, requestCQ.get(), types::GET_DB_LOGS);
  new CallData(&service_, this, requestCQ.get(), types::PREPARE);
  new CallData(&service_, this, requestCQ.get(), types::ACCEPT);
  new CallData(&service_, this, requestCQ.get(), types::COMMIT);
  new CallData(&service_, this, requestCQ.get(), types::SYNC);

  void* requestTag;
  bool requestOk;
  void* responseTag;
  bool responseOk;

  while (true) {
      // Poll the request queue
      grpc::CompletionQueue::NextStatus requestStatus = 
          requestCQ->AsyncNext(&requestTag, &requestOk, gpr_time_0(GPR_CLOCK_REALTIME));

      // Poll the response queue
      grpc::CompletionQueue::NextStatus responseStatus = 
          responseCQ->AsyncNext(&responseTag, &responseOk, gpr_time_0(GPR_CLOCK_REALTIME));

      // Handle request events
      if (requestStatus == grpc::CompletionQueue::NextStatus::GOT_EVENT && requestOk) {
          static_cast<CallData*>(requestTag)->Proceed();  // Process request
      }

      // Handle response events
      if (responseStatus == grpc::CompletionQueue::NextStatus::GOT_EVENT && responseOk) {
          static_cast<PaxosClientCall*>(responseTag)->HandleRPCResponse();  // Process response
      }
  }
}

int PaxosServer::getServerIdFromName(std::string serverName) {
  return serverName[1] - '0';
}


/*
 * Helper methods for consensus
 */

void PaxosServer::sendPrepareToCluster(PrepareReq& request) {
  int myServerId = getServerIdFromName(serverName);
  std::chrono::time_point deadline = std::chrono::system_clock::now() + std::chrono::seconds(rpcTimeoutSeconds);
  for (int i = 0; i < stubs_.size(); i++) {
      if (i != myServerId - 1) {
        sendPrepareAsync(request, stubs_[i], deadline);
      }
  }
}

void PaxosServer::sendAcceptToCluster(AcceptReq& request) {
  int myServerId = getServerIdFromName(serverName);
  std::chrono::time_point deadline = std::chrono::system_clock::now() + std::chrono::seconds(rpcTimeoutSeconds);
  for (int i = 0; i < stubs_.size(); i++) {
    if (i != myServerId - 1) {
      sendAcceptAsync(request, stubs_[i], deadline);
    }
  }
}

void PaxosServer::sendCommitToCluster(CommitReq& request) {
  int myServerId = getServerIdFromName(serverName);
  std::chrono::time_point deadline = std::chrono::system_clock::now() + std::chrono::seconds(rpcTimeoutSeconds);
  for (int i = 0; i < stubs_.size(); i++) {
    if (i != myServerId - 1) {
      sendCommitAsync(request, stubs_[i], deadline);
    }
  }
}

void PaxosServer::requestSync(std::string serverName) {
    SyncReq req;
    req.set_last_committed_block(lastCommittedBlock);
    
    int targetServerId = getServerIdFromName(serverName);
    sendSyncAsync(req, stubs_[targetServerId - 1], std::chrono::system_clock::now() + std::chrono::seconds(rpcTimeoutSeconds));
}

void PaxosServer::sendPrepareAsync(PrepareReq& request, std::unique_ptr<Paxos::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline) {
  PaxosClientCall* call = new PaxosClientCall(this, responseCQ.get(), types::PREPARE);
  call->sendPrepare(request, stub_, deadline);
}

void PaxosServer::sendAcceptAsync(AcceptReq& request, std::unique_ptr<Paxos::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline) {
  PaxosClientCall* call = new PaxosClientCall(this, responseCQ.get(), types::ACCEPT);
  call->sendAccept(request, stub_, deadline);
}

void PaxosServer::sendCommitAsync(CommitReq& request, std::unique_ptr<Paxos::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline) {
  PaxosClientCall* call = new PaxosClientCall(this, responseCQ.get(), types::COMMIT);
  call->sendCommit(request, stub_, deadline);
}

void PaxosServer::sendSyncAsync(SyncReq& request, std::unique_ptr<Paxos::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline) {
  PaxosClientCall* call = new PaxosClientCall(this, responseCQ.get(), types::SYNC);
  call->sendSync(request, stub_, deadline);
}

void PaxosServer::handlePrepareReply(PrepareRes& reply) {
  if (currentState_ == PREPARE || (currentState_ == PROPOSE && !awaitAcceptDecision)) {
    if (reply.proposal().number() == myProposal.proposalNum && reply.ack()) {
      prepareSuccesses++;
      if (reply.has_logs() && highestAcceptVal.empty()) {
        for (int i = 0; i < reply.logs().logs_size(); i++) {
          const Transaction& t = reply.logs().logs(i);
          remoteLogs.push_back({ t.id(), t.sender(), t.receiver(), t.amount() });
        }
      } else if (reply.has_accept_num() && reply.accept_num().number() > highestAcceptNum.proposalNum) {
        highestAcceptNum.proposalNum = reply.accept_num().number();
        highestAcceptNum.serverName = reply.accept_num().server_id();

        remoteLogs.clear();
        highestAcceptVal.clear();

        for (int i = 0; i < reply.accept_val().logs_size(); i++) {
          const Transaction& t = reply.accept_val().logs(i);
          highestAcceptVal.push_back({ t.id(), t.sender(), t.receiver(), t.amount() });
        }
      }
    } else if (reply.proposal().number() == myProposal.proposalNum && !reply.ack()) {
      prepareFailures++;
      // check if catch up is needed
      if (reply.last_committed_block() > lastCommittedBlock) {
        currentState_ = SYNC;
        requestSync(reply.server_id());
      }
    }

    // state change logic
    if (prepareSuccesses >= MAJORITY) {
      currentState_ = PROPOSE;
      awaitPrepareDecision = false;
      // resetConsensusDeadline();
    } else if (prepareFailures >= MAJORITY) {
      currentState_ = IDLE;
      awaitPrepareDecision = false;
      resetConsensusDeadline();
    }
  }
}

void PaxosServer::handleAcceptReply(AcceptRes& reply) {
  if (currentState_ != PROPOSE) {
    // ignore the accept replies
    return;
  }

  if (reply.proposal().number() == acceptNum.proposalNum && reply.ack()) {
    acceptSuccesses++;
  } else {
    acceptFailures++;
  }

  if (acceptSuccesses >= MAJORITY) {
    currentState_ = COMMIT;
    awaitAcceptDecision = false;
    // resetConsensusDeadline();
  } else if (acceptFailures >= MAJORITY) {
    currentState_ = IDLE;
    awaitAcceptDecision = false;
    resetConsensusDeadline();
  }
}

void PaxosServer::handleSyncReply(SyncRes& reply) {
  if (reply.block_id() == lastCommittedBlock + 1) {
    types::TransactionBlock block;
    block.id = reply.block_id();
    block.commitProposal.proposalNum = reply.proposal().number();
    block.commitProposal.serverName = reply.proposal().server_id();

    for (int i = 0; i < reply.block().logs_size(); i++) {
      auto& t = reply.block().logs(i);
      block.block.push_back({ t.id(), t.sender(), t.receiver(), t.amount() });
    }

    doCommit(reply.proposal().number(), reply.proposal().server_id(), block.block);
    if (reply.ack()) {
      currentState_ = SYNC;
      requestSync(reply.server_id());
    } else {
      // sync complete
      currentState_ = IDLE;
    }
  }

  // set myProposalNum to the proposal num of last committed transaction
  if (reply.proposal().number() > myProposal.proposalNum) {
    myProposal.proposalNum = reply.proposal().number();
  }
}

void PaxosServer::copyProposal(Proposal* to, types::Proposal from) {
  to->set_number(from.proposalNum);
  to->set_server_id(from.serverName);
}

void PaxosServer::copyLogs(Logs* logs, std::vector<types::Transaction>& val) {
  for (auto l : val) {
        Transaction* t = logs->add_logs();
        t->set_id(l.id);
        t->set_sender(l.sender);
        t->set_receiver(l.receiver);
        t->set_amount(l.amount);
      }
}

std::vector<types::Transaction> PaxosServer::getLogsForProposal() {
  std::vector<types::Transaction> result;  

  // set of committed logs
  std::set<std::string> committedSet;
  for (auto& block : committedBlocks) {
    for (auto& t : block.block) {
      committedSet.insert(t.toString());
    }
  }

  if (!highestAcceptVal.empty()) {
    // iterate through highestAcceptVal and check if any entries are already committed
    for (auto& t : highestAcceptVal) {
      if (committedSet.find(t.toString()) == committedSet.end()) {
        result.push_back(t);
      }
    }
  }

  if (result.size() == 0) {
    // highestAcceptVal is already committed. ignore it.
    // it is not possible for localLogs to be already committed
    result = localLogs;

    // however some of the remote logs could have been committed already
    for (auto& t: remoteLogs) {
      if (committedSet.find(t.toString()) == committedSet.end()) {
        result.push_back(t);
      }
    }
  }

  // remove duplicates from proposed value
  std::set<std::string> resultSet;
  std::vector<types::Transaction> finalResult;
  for (auto &t : result) {
    if (resultSet.find(t.toString()) == resultSet.end()) {
      finalResult.push_back(t);
    }
    resultSet.insert(t.toString());
  }

  return finalResult;
}

bool PaxosServer::processTransferCall(TransferReq* req, TransferRes* res) {

  bool success = true;
  res->set_id(req->id());
  res->set_server_id(serverName);

  switch (currentState_) {
    case SYNC:
      success = false;
      break;

    case IDLE:
    case PROMISED:
    case ACCEPTED:
      // server can process the client transaction
      if (req->amount() <= balance) {
        currentTransactionNum++;
        types::Transaction t = { currentTransactionNum, serverName, req->receiver(), req->amount() };
        localLogs.push_back(t);
        balance -= req->amount();
        storeLocalLogDB(t);
        res->set_ack(true);
        success = true;
      } else {
        // initiate consensus
        if (std::chrono::system_clock::now() >= consensusDeadline) {
          awaitPrepareDecision = false;
          currentState_ = PREPARE;
        }
        // wait longer. return false to indicate that the request is not fully processed yet
        success = false;
      }
      break;

    case PREPARE:
      if (!awaitPrepareDecision || (awaitPrepareDecision && std::chrono::system_clock::now() >= consensusDeadline)) {
        
        // Compose a Prepare request
        myProposal.proposalNum = myProposal.proposalNum + 1;
        myProposal.serverName = serverName;
        
        highestAcceptNum.proposalNum = 0;
        highestAcceptNum.serverName = "";
        highestAcceptVal.clear();

        prepareSuccesses = 1;
        prepareFailures = 0;
        remoteLogs.clear();
        PrepareReq prepareReq;
        copyProposal(prepareReq.mutable_proposal(), myProposal);
        prepareReq.set_last_committed_block(lastCommittedBlock);

        // send a prepare request to cluster
        awaitPrepareDecision = true;
        sendPrepareToCluster(prepareReq);
        resetConsensusDeadline();
      }

      success = false;
      break;

    case PROPOSE:
      if (awaitAcceptDecision && std::chrono::system_clock::now() >= consensusDeadline) {
        currentState_ = PREPARE;
        awaitAcceptDecision = false;
      } else if (!awaitAcceptDecision) {
        AcceptReq acceptReq;
        copyProposal(acceptReq.mutable_proposal(), myProposal);
        acceptReq.set_last_committed_block(lastCommittedBlock);
        
        std::vector<types::Transaction> proposedVal = getLogsForProposal();
        if (proposedVal.empty()) {
          // nothing to propose
          currentState_ = IDLE;
          success = false;
        } else {
          acceptNum.proposalNum = myProposal.proposalNum;
          acceptNum.serverName = myProposal.serverName;
          acceptVal = proposedVal;
          copyLogs(acceptReq.mutable_logs(), proposedVal);
          acceptSuccesses = 1;
          acceptFailures = 0;
          awaitAcceptDecision = true;
          sendAcceptToCluster(acceptReq);
          resetConsensusDeadline();
        }
      }
        
      success = false;
      break;

    case COMMIT:
      CommitReq commitReq;
      copyProposal(commitReq.mutable_proposal(), myProposal);
      sendCommitToCluster(commitReq);
      doCommit(acceptNum.proposalNum, acceptNum.serverName, acceptVal);

      // insert the consensus causing transaction into localLogs
      if (req->amount() <= balance) {
        currentTransactionNum++;
        types::Transaction t = { currentTransactionNum, serverName, req->receiver(), req->amount() };
        
        localLogs.push_back(t);
        balance -= req->amount();
        storeLocalLogDB(t);
        res->set_ack(true);
        success = true;
      } else {
        success = false;
      }

      currentState_ = IDLE;
      acceptNum.proposalNum = 0;
      acceptNum.serverName = "";
      acceptVal.clear();

      // TODO: have another thread save the state periodically
      saveStateDB();
      break;
  }

  return success;
}

bool PaxosServer::processGetBalanceCall(Balance* res) {
  res->set_amount(balance);
  return true;
}

bool PaxosServer::processGetLogsCall(Logs* logs) {
  copyLogs(logs, localLogs);
  return true;
}

bool PaxosServer::processGetDBLogsCall(DBLogs* blocks) {
  for (int i = 0; i < committedBlocks.size(); i++) {
    types::TransactionBlock& block = committedBlocks.at(i);
    
    TransactionBlock* b = blocks->add_blocks();
    b->set_block_id(i + 1);
    Proposal* p = b->mutable_proposal();
    p->set_number(block.commitProposal.proposalNum);
    p->set_server_id(block.commitProposal.serverName);
    
    Logs* logs = b->mutable_logs();
    copyLogs(logs, block.block);
  }
  return true;
}

bool PaxosServer::processPrepareCall(PrepareReq* req, PrepareRes* res) {
  res->mutable_proposal()->CopyFrom(req->proposal());
  res->set_server_id(serverName);
  res->set_last_committed_block(lastCommittedBlock);

  if (currentState_ == IDLE || currentState_ == PREPARE || currentState_ == PROMISED || (currentState_ == PROPOSE && !awaitAcceptDecision)) {
      // just check the proposer's proposal number and last commited block
      if (req->proposal().number() > myProposal.proposalNum && req->last_committed_block() >= lastCommittedBlock) {
            myProposal.proposalNum = req->proposal().number();
            myProposal.serverName = req->proposal().server_id();
            copyLogs(res->mutable_logs(), localLogs);
            res->set_ack(true);
            currentState_ = PROMISED;
            resetConsensusDeadline();
            if (req->last_committed_block() > lastCommittedBlock) {
              requestSync(req->proposal().server_id());
            }

      } else {
        res->set_ack(false);
      }

      return true;
  } else if ((currentState_ == PROPOSE && awaitAcceptDecision) || currentState_ == ACCEPTED) {
      // check if proposal is valid
      if (req->proposal().number() > acceptNum.proposalNum && req->last_committed_block() >= lastCommittedBlock) {
        myProposal.proposalNum = req->proposal().number();
        myProposal.serverName = req->proposal().server_id();
        copyProposal(res->mutable_accept_num(), acceptNum);
        copyLogs(res->mutable_accept_val(), acceptVal);
        res->set_ack(true);
        currentState_ = ACCEPTED;
        resetConsensusDeadline();
        if (req->last_committed_block() > lastCommittedBlock) {
          requestSync(req->proposal().server_id());
        }
      } else {
        res->set_ack(false);
      }

      return true;
  } else {
    // handle the prepare later. do the commit first
    return false;
  }
}

bool PaxosServer::processAcceptCall(AcceptReq* req, AcceptRes* res) {  
  res->mutable_proposal()->CopyFrom(req->proposal());

  // accept the value conditionally 
  // [IDLE] : server was not issued any transaction, and was not involved in consensus
  // [PREPARE] : server started consensus, but in the meantime another leader got elected who is now sending accept requests
  // [PROPOSE] : server was slow, before it could send out accept requests another leader got elected who is now sending accept requests
  // [PROMISED] : server promised to receive value in response to prepare requests
  // [ACCEPTED] : minority server accepted a value in previous round. A new value is proposed in the second round since this server did not get a prepare request
  // [COMMIT] : A partition prevented the server from committing. In the meantime, a new leader came in and committed the accepted block
  bool accept = false;
  switch (currentState_) {
    case IDLE:
    case PREPARE:
      accept = req->proposal().number() > myProposal.proposalNum;
      break;
    case PROPOSE:
      if (!awaitAcceptDecision) {
        accept = req->proposal().number() > myProposal.proposalNum;
      } else {
        accept = req->proposal().number() > acceptNum.proposalNum;
      }
      
    case PROMISED:
      accept = req->proposal().number() >= myProposal.proposalNum;
      break;

    case ACCEPTED:
      accept = req->proposal().number() > acceptNum.proposalNum;
      break;

    default:
      accept = false;
      break;
    
  }
  
  if (accept && req->last_committed_block() == lastCommittedBlock) {
    res->set_ack(true);
    acceptNum.proposalNum = req->proposal().number();
    acceptNum.serverName = req->proposal().server_id();

    // server is in sync. no catch up needed
    acceptVal.clear();
    for (int i = 0; i < req->logs().logs_size(); i++) {
      const Transaction& t = req->logs().logs(i);
      acceptVal.push_back({ t.id(), t.sender(), t.receiver(), t.amount() });
    }

    currentState_ = ACCEPTED;
  } else if (req->last_committed_block() > lastCommittedBlock) {
    requestSync(req->proposal().server_id());
    res->set_ack(false);
  } else {
    res->set_ack(false);
  }

  return true;
}

bool PaxosServer::processCommitCall(CommitReq* req) {
  if (currentState_ == ACCEPTED && req->proposal().number() == acceptNum.proposalNum) {
    doCommit(req->proposal().number(), req->proposal().server_id(), acceptVal);
    acceptNum.proposalNum = 0;
    acceptNum.serverName = "";
    acceptVal.clear();
    currentState_ = IDLE;
  } else if (req->proposal().number() > myProposal.proposalNum) {
    requestSync(req->proposal().server_id());
  } 
  return true;
}

bool PaxosServer::processSyncCall(SyncReq* req, SyncRes* res) {
  int requestedBlocK = req->last_committed_block() + 1;
  if (committedBlocks.size() >= requestedBlocK) {
    auto& block = committedBlocks.at(req->last_committed_block());  
    res->set_block_id(block.id);
    Proposal* p = res->mutable_proposal();
    p->set_number(block.commitProposal.proposalNum);
    p->set_server_id(block.commitProposal.serverName);
    Logs* l = res->mutable_block();
    for (auto& t: block.block) {
      Transaction* transaction = l->add_logs();
      transaction->set_id(t.id);
      transaction->set_sender(t.sender);
      transaction->set_receiver(t.receiver);
      transaction->set_amount(t.amount);
    }
    res->set_server_id(serverName);
  } else {
    res->set_ack(false);
  }
  

  return true;
}

void PaxosServer::doCommit(int commitProposalNum, std::string commitProposalServer, std::vector<types::Transaction> logs) {
  lastCommittedBlock++;
  lastCommittedProposal.proposalNum = commitProposalNum;
  lastCommittedProposal.serverName = commitProposalServer;

  types::TransactionBlock block;
  block.id = lastCommittedBlock;
  block.commitProposal.proposalNum = lastCommittedProposal.proposalNum;
  block.commitProposal.serverName = lastCommittedProposal.serverName;
  for (auto& t: logs) {
    block.block.push_back({ t.id, t.sender, t.receiver, t.amount });
  }
  
  committedBlocks.push_back(block);
  commitLogsToDB(logs);

  // update local logs and balance
  // set of committed logs
  std::set<std::string> committedSet;
  for (auto& t : logs) {
    if (t.receiver == serverName) balance += t.amount;
    committedSet.insert(t.toString());
  }


  std::vector<types::Transaction> updatedLocalLogs;
  for (auto& t : localLogs) {
    if (committedSet.find(t.toString()) == committedSet.end()) {
      updatedLocalLogs.push_back(t);
    }
  }

  localLogs = updatedLocalLogs;
  refreshLocalLogsDB();
}

void PaxosServer::setupDB() {
  try {
    SQLite::Database db(dbFilename.c_str(), SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE);

      // Compile a SQL query, containing one parameter (index 1)
      db.exec("CREATE TABLE IF NOT EXISTS logs ("
          "block_id INTEGER NOT NULL,"
          "proposal_num INTEGER NOT NULL,"
          "proposer CHAR(2) NOT NULL,"
          "transaction_id INTEGER NOT NULL,"
          "sender CHAR(2) NOT NULL,"
          "receiver CHAR(2) NOT NULL,"
          "amount INTEGER NOT NULL"
          ");");

      db.exec("CREATE TABLE IF NOT EXISTS local ("
          "transaction_id INTEGER NOT NULL,"
          "sender CHAR(2) NOT NULL,"
          "receiver CHAR(2) NOT NULL,"
          "amount INTEGER NOT NULL"
          ");");

      db.exec("CREATE TABLE IF NOT EXISTS accept_val ("
          "transaction_id INTEGER NOT NULL,"
          "sender CHAR(2) NOT NULL,"
          "receiver CHAR(2) NOT NULL,"
          "amount INTEGER NOT NULL"
          ");");

      db.exec("CREATE TABLE IF NOT EXISTS state ("
          "proposal_num INTEGER NOT NULL,"
          "accept_num CHAR(2) NOT NULL,"
          "accept_num_server CHAR(2) NOT NULL"
          ");");

  } catch (SQLite::Exception& e) {
    std::cerr << "exception " << e.what() << std::endl;
  }
}

void PaxosServer::getLocalLogsDB() {
  try {
    // Open a database file
    SQLite::Database db(dbFilename.c_str(), SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE);
    SQLite::Statement query(db, "SELECT * FROM local ORDER BY transaction_id;");
    localLogs.clear();

    while (query.executeStep()) {
      int transaction_id = query.getColumn(0);
      std::string sender = query.getColumn(1).getString();
      std::string receiver = query.getColumn(2).getString();
      int amount = query.getColumn(3);  
      balance -= amount;

      currentTransactionNum = std::max(currentTransactionNum, transaction_id);
      localLogs.push_back({ transaction_id, sender, receiver, amount });
    }
  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << std::endl;
  }
}

void PaxosServer::storeLocalLogDB(types::Transaction t) {
  try {
    // Open a database file
    SQLite::Database db(dbFilename.c_str(), SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE);
    SQLite::Statement insert(db, "INSERT INTO local (transaction_id, sender, receiver, amount) VALUES (?, ?, ?, ?)");

    insert.bind(1, t.id);
    insert.bind(2, t.sender);
    insert.bind(3, t.receiver);
    insert.bind(4, t.amount);

    insert.exec();
  } catch (SQLite::Exception& e) {
    std::cerr << "exception " << e.what() << std::endl;
  }
}

void PaxosServer::refreshLocalLogsDB() {
  try {
    // Open a database file
    SQLite::Database db(dbFilename.c_str(), SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE);
    db.exec("BEGIN TRANSACTION;");
    
    SQLite::Statement del(db, "DELETE FROM local;");
    del.exec();

    SQLite::Statement insert(db, "INSERT INTO local (transaction_id, sender, receiver, amount) VALUES (?, ?, ?, ?);");
    for (auto &t : localLogs) {
      insert.bind(1, t.id);
      insert.bind(2, t.sender);
      insert.bind(3, t.receiver);
      insert.bind(4, t.amount);
      insert.exec();
      insert.reset();
    }
    db.exec("COMMIT;");
    
  } catch (SQLite::Exception& e) {
    std::cerr << "exception " << e.what() << std::endl;
  }
}

void PaxosServer::getSavedStateDB() {
  try {
    SQLite::Database db(dbFilename, SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE);
    SQLite::Statement query(db, "SELECT proposal_num, accept_num, accept_num_server FROM state;");
    
    if (query.executeStep()) {
      myProposal.proposalNum = query.getColumn(0);
      acceptNum.proposalNum = query.getColumn(1);
      acceptNum.serverName = query.getColumn(2).getString();
    }
    
    query.reset();
    acceptVal.clear();
    SQLite::Statement query2(db, "SELECT transaction_id, sender, receiver, amount FROM accept_val;");
    while (query2.executeStep()) {
      int transaction_id = query2.getColumn(0);
      std::string sender = query2.getColumn(1).getString();
      std::string receiver = query2.getColumn(1).getString();
      int amount = query2.getColumn(3);

      acceptVal.push_back({ transaction_id, sender, receiver, amount });
    }

  } catch (SQLite::Exception& e) {
    std::cerr << "Exception: " << e.what() << std::endl;
  }
}

void PaxosServer::saveStateDB() {
  try {
    SQLite::Database db(dbFilename, SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE);

    db.exec("BEGIN TRANSACTION;");
    SQLite::Statement insert(db, "INSERT INTO state (proposal_num, accept_num, accept_num_server) VALUES (?, ?, ?);");
    insert.bind(1, myProposal.proposalNum);
    insert.bind(2, acceptNum.proposalNum);
    insert.bind(3, acceptNum.serverName);
    insert.exec();
    insert.reset();

    SQLite::Statement insert2(db, "INSERT INTO accept_val (transaction_id, sender, receiver, amount) VALUES (?, ?, ?, ?);");
    for (auto &t : localLogs) {
      insert2.bind(1, t.id);
      insert2.bind(2, t.sender);
      insert2.bind(3, t.receiver);
      insert2.bind(4, t.amount);
      insert2.exec();
      insert2.reset();
    }
    db.exec("COMMIT;");
  } catch (SQLite::Exception& e) {
    std::cerr << "exception " << e.what() << std::endl;
  }
}

void PaxosServer::getCommittedLogsDB() {
  try {
    // Open a database file
    SQLite::Database db(dbFilename.c_str(), SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE);
    SQLite::Statement query(db, "SELECT * FROM logs ORDER BY block_id, transaction_id;");
    committedBlocks.clear();

    while (query.executeStep()) {
      int block_id = query.getColumn(0);
      int proposal_num = query.getColumn(1);
      std::string proposer = query.getColumn(2).getString();
      int transaction_id = query.getColumn(3);
      std::string sender = query.getColumn(4).getString();
      std::string receiver = query.getColumn(5).getString();
      int amount = query.getColumn(6);

      if (block_id > committedBlocks.size()) {
        types::Proposal p = { proposal_num, proposer };
        committedBlocks.push_back( { block_id, p, std::vector<types::Transaction>() } );
      }

      if (this->serverName == sender) {
        balance -= amount;
      } else if (this->serverName == receiver) {
        balance += amount;
      }

      committedBlocks[block_id - 1].block.push_back({ transaction_id, sender, receiver, amount });
    }

    lastCommittedBlock = committedBlocks.size();
    if (lastCommittedBlock > 0) {
      lastCommittedProposal.proposalNum = committedBlocks.at(committedBlocks.size() - 1).commitProposal.proposalNum;
      lastCommittedProposal.serverName = committedBlocks.at(committedBlocks.size() - 1).commitProposal.serverName;
    } else {
      lastCommittedProposal.proposalNum = 0;
      lastCommittedProposal.serverName = "";
    }

  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << std::endl;
  }
}

void PaxosServer::commitLogsToDB(std::vector<types::Transaction> logs) {
  try    {
    SQLite::Database db(dbFilename, SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE);
    
    db.exec("BEGIN TRANSACTION;");
    SQLite::Statement insert(db, "INSERT INTO logs (block_id, proposal_num, proposer, transaction_id, sender, receiver, amount) VALUES (?, ?, ?, ?, ?, ?, ?);");
    
    for (const auto& t : logs) {
      insert.bind(1, lastCommittedBlock);
      insert.bind(2, lastCommittedProposal.proposalNum);
      insert.bind(3, lastCommittedProposal.serverName);
      insert.bind(4, t.id);
      insert.bind(5, t.sender);
      insert.bind(6, t.receiver);
      insert.bind(7, t.amount);
      insert.exec();
      insert.reset(); 
    }  
    db.exec("COMMIT");

  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << std::endl;
  }
}

void PaxosServer::resetConsensusDeadline() {
  static std::random_device rd;   // Random device for seed.
  static std::mt19937 gen(rd());  // Mersenne Twister generator.
  std::uniform_int_distribution<> dist(PaxosServer::minBackoffMs, PaxosServer::maxBackoffMs);
  int backoff = dist(gen);
  consensusDeadline = std::chrono::system_clock::now() + std::chrono::milliseconds(backoff);
}

void RunServer(std::string serverName, std::string serverAddress) {
  PaxosServer server(serverName);
  server.run(serverAddress);
}

int main(int argc, char** argv) {
  
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <server_name> <target_address>" << std::endl;
    return 1;
  }

  try {
    RunServer(argv[1], argv[2]);
  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << std::endl;
  }


  return 0;
}