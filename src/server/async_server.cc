#include <set>
#include <random>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <chrono>

#include "async_server.h"
#include "../constants.h"
#include "../crypto/crypto.h"
#include "../utils/utils.h"

using pbft::TransactionData;
using pbft::Context;
using pbft::NewViewData;
using pbft::SyncResData;
using pbft::PrepareProof;
using pbft::ViewChangeData;
using pbft::LogEntry;
using pbft::ViewChangesResEntry;


PbftServerImpl::PbftServerImpl(int id, std::string name, bool byzantine) {
  serverId = id;
  serverName = name;
  isByzantine = byzantine;

  f = 2;
  k = 20;

  currentView = 1;
  currentSequence = -1;
  clusterSize = 7;

  lowWatermark = 0;
  highWatermark =  lowWatermark + k - 1;

  log = std::vector<MessageInfo*>(k, nullptr);
  digestToEntry = std::map<std::string, MessageInfo*>();

  checkpointStepSize = 10;
  lastStableCheckpointSeqNum = -1;
  lastExecuted = -1;
  state_ = NORMAL;


  rpcTimeoutSeconds = 2;
  viewChangeTimeoutSeconds = 0;
  viewChangeTimeoutDelta = 2;
  optimisticTimeoutSeconds = 2;
  retryTimeoutSeconds = 2;

  lastExecuted = -1;
  for (unsigned char c = 'A'; c <= 'J'; c++) {
    balances[std::to_string(c)] = 10;
  }
}

PbftServerImpl::~PbftServerImpl() {
  server_->Shutdown();
  requestCQ->Shutdown();
  responseCQ->Shutdown();
}

void PbftServerImpl::run(std::string targetAddress) {
  ServerBuilder builder;
  builder.AddListeningPort(targetAddress, grpc::InsecureServerCredentials());
  builder.RegisterService(&service_);
  requestCQ = builder.AddCompletionQueue();
  responseCQ = std::make_unique<CompletionQueue>();
  server_ = builder.BuildAndStart();

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

  std::cout << "Server running on " << targetAddress << std::endl;
  HandleRPCs();
}

void PbftServerImpl::HandleRPCs() {
  new RequestData(&service_, this, requestCQ.get(), types::TRANSFER);
  new RequestData(&service_, this, requestCQ.get(), types::PRE_PREPARE);
  new RequestData(&service_, this, requestCQ.get(), types::PRE_PREPARE_OK);
  new RequestData(&service_, this, requestCQ.get(), types::PREPARE);
  new RequestData(&service_, this, requestCQ.get(), types::PREPARE_OK);
  new RequestData(&service_, this, requestCQ.get(), types::COMMIT);
  new RequestData(&service_, this, requestCQ.get(), types::VIEW_CHANGE);
  new RequestData(&service_, this, requestCQ.get(), types::NEW_VIEW);

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
          static_cast<RequestData*>(requestTag)->Proceed();  // Process request
      }

      // Handle response events
      if (responseStatus == grpc::CompletionQueue::NextStatus::GOT_EVENT && responseOk) {
          static_cast<ResponseData*>(responseTag)->HandleRPCResponse();  // Process response
      }

      // Clean up the queue of futures
      if (!futures.empty()) {
        std::future<void>& f = futures.front();
        std::future_status status = f.wait_for(std::chrono::milliseconds(0));
        if (status == std::future_status::ready) futures.pop();
      }
  }
}

int PbftServerImpl::getLeaderId() {
  return currentView % clusterSize;
}

void PbftServerImpl::sendRequest(Request& request, int receiverId, types::RequestTypes type) {
  // Sign the request
  std::string dataString;
  request.data().SerializeToString(&dataString);
  addMACSignature(dataString, request.mutable_sig_vec());

  // Send grpc request
  ResponseData* call = new ResponseData(this, responseCQ.get());
  std::unique_ptr<PbftServer::Stub>& stub_ = serverStubs_.at(receiverId);
  std::chrono::time_point deadline = std::chrono::system_clock::now() + std::chrono::seconds(rpcTimeoutSeconds);
  switch (type) {
    case types::PRE_PREPARE_OK:
      call->sendPrePrepareOk(request, stub_, deadline);
      break;
    case types::PREPARE_OK:
      call->sendPrepareOk(request, stub_, deadline);
      break;
    default:
      break;
  }
}

void PbftServerImpl::sendRequest(ProofRequest& request, int receiverId, types::RequestTypes type) {
  // Send grpc request
  ResponseData* call = new ResponseData(this, responseCQ.get());
  std::unique_ptr<PbftServer::Stub>& stub_ = serverStubs_.at(receiverId);
  std::chrono::time_point deadline = std::chrono::system_clock::now() + std::chrono::seconds(rpcTimeoutSeconds);
  switch (type) {
    case types::PREPARE:
      call->sendPrepare(request, stub_, deadline);
      break;
    case types::COMMIT:
      call->sendCommit(request, stub_, deadline);
      break;
    default:
      break;
  }
}

void PbftServerImpl::sendRequestToAll(ProofRequest& request, types::RequestTypes type) {
  for (int i = 0; i < clusterSize; i++) {
      if (i != serverId) {
        sendRequest(request, i, type);
      }
  }
}

void PbftServerImpl::sendPrePrepareToAll(PrePrepareRequest& request) {
  std::string dataString;
  Request* r = request.mutable_r();
  r->data().SerializeToString(&dataString);
  addMACSignature(dataString, r->mutable_sig_vec());

  for (int i = 0; i < clusterSize; i++) {
      if (i != serverId) {
          ResponseData* call = new ResponseData(this, responseCQ.get());
          std::unique_ptr<PbftServer::Stub>& stub_ = serverStubs_.at(serverId);
          std::chrono::time_point deadline = std::chrono::system_clock::now() + std::chrono::seconds(rpcTimeoutSeconds);
          call->sendPrePrepare(request, stub_, deadline);      
      }
  }
}

void PbftServerImpl::sendViewChangeToAll(ViewChangeReq& request) {
  std::string dataString;
  request.data().SerializeToString(&dataString);

  for (int i = 0; i < clusterSize; i++) {
      if (i != serverId) {
          // NOTE: public key cryptography is used in View Change and New View messages
          addECDSASignature(dataString, request.mutable_signature());

          ResponseData* call = new ResponseData(this, responseCQ.get());
          std::unique_ptr<PbftServer::Stub>& stub_ = serverStubs_.at(serverId);
          std::chrono::time_point deadline = std::chrono::system_clock::now() + std::chrono::seconds(rpcTimeoutSeconds);
          call->sendViewChange(request, stub_, deadline);      
      }
  }
}

void PbftServerImpl::sendNewViewToAll(NewViewReq& request) {
  std::string dataString;
  request.data().SerializeToString(&dataString);

  // NOTE: public key cryptography is used in View Change and New View messages
  addECDSASignature(dataString, request.mutable_signature());

  for (int i = 0; i < clusterSize; i++) {
      if (i != serverId) {

          ResponseData* call = new ResponseData(this, responseCQ.get());
          std::unique_ptr<PbftServer::Stub>& stub_ = serverStubs_.at(serverId);
          std::chrono::time_point deadline = std::chrono::system_clock::now() + std::chrono::seconds(rpcTimeoutSeconds);
          call->sendNewView(request, stub_, deadline);      
      }
  }
}

void PbftServerImpl::sendCheckpointToAll(CheckpointReq& request) {
  std::string dataString;
  request.data().SerializeToString(&dataString);

  // NOTE: public key cryptography is used in View Change and New View messages
  addECDSASignature(dataString, request.mutable_signature());

  for (int i = 0; i < clusterSize; i++) {
      if (i != serverId) {
          ResponseData* call = new ResponseData(this, responseCQ.get());
          std::unique_ptr<PbftServer::Stub>& stub_ = serverStubs_.at(serverId);
          std::chrono::time_point deadline = std::chrono::system_clock::now() + std::chrono::seconds(rpcTimeoutSeconds);
          call->sendCheckpoint(request, stub_, deadline);      
      }
  }
}

void PbftServerImpl::sendSync(SyncReq& request, int senderId, int receiverId) {
  // Sign the request
  std::string dataString;
  request.data().SerializeToString(&dataString);
  addMACSignature(dataString, receiverId, request.mutable_signature());

  // Send grpc request
  ResponseData* call = new ResponseData(this, responseCQ.get());
  std::unique_ptr<PbftServer::Stub>& stub_ = serverStubs_.at(receiverId);
  std::chrono::time_point deadline = std::chrono::system_clock::now() + std::chrono::seconds(rpcTimeoutSeconds);
  call->sendSync(request, stub_, deadline);
}

void PbftServerImpl::addMACSignature(std::string data, int receiverId, Signature* signature) {
  std::string binPath = Utils::macKeyPath(serverId, receiverId);
  signature->set_sig(crypto::signMAC(data, binPath));
  signature->set_server_id(serverId);
}

void PbftServerImpl::addMACSignature(std::string data, SignatureVec* sigVec) {
  for (int i = 0; i < clusterSize; i++) {
    std::string binPath = Utils::macKeyPath(serverId, i);
    sigVec->add_signatures(crypto::signMAC(data, binPath));
  }

  sigVec->set_server_id(serverId);
}

void PbftServerImpl::addECDSASignature(std::string data, Signature* signature) {
  std::string pemPath = Utils::serverPrECDSAKeyPath(serverId);
  signature->set_sig(crypto::signECDSA(data, pemPath));
  signature->set_server_id(serverId);
}

bool PbftServerImpl::verifySignature(const Request& request) {
  std::string dataString;
  request.data().SerializeToString(&dataString);
  std::string binPath = Utils::macKeyPath(serverId, request.sig_vec().server_id());
  return crypto::verifyMAC(dataString, request.sig_vec().signatures(serverId), binPath);
}

bool PbftServerImpl::verifySignature(const Message& request) {
  std::string dataString;
  request.data().SerializeToString(&dataString);
  std::string pemPath = Utils::clientPbECDSAKeyPath(request.signature().server_id());
  return crypto::verifyECDSA(dataString, request.signature().sig(), pemPath);
}

bool PbftServerImpl::verifySignature(const ViewChangeReq& request) {
  std::string dataString;
  request.data().SerializeToString(&dataString);
  std::string pemPath = Utils::serverPbECDSAKeyPath(request.signature().server_id());
  return crypto::verifyECDSA(dataString, request.signature().sig(), pemPath);
}

bool PbftServerImpl::verifySignature(const CheckpointReq& request) {
  std::string dataString;
  request.data().SerializeToString(&dataString);
  std::string binPath = Utils::macKeyPath(serverId, request.signature().server_id());
  return crypto::verifyMAC(dataString, request.signature().sig(), binPath);
}

bool PbftServerImpl::verifySignature(const NewViewReq& request) {
  std::string dataString;
  request.data().SerializeToString(&dataString);
  std::string pemPath = Utils::serverPbECDSAKeyPath(request.signature().server_id());
  return crypto::verifyECDSA(dataString, request.signature().sig(), pemPath);
}

bool PbftServerImpl::verifySignature(const SyncReq& request) {
  std::string dataString;
  request.data().SerializeToString(&dataString);
  std::string binPath = Utils::macKeyPath(serverId, request.signature().server_id());
  return crypto::verifyMAC(dataString, request.signature().sig(), binPath);
}

void PbftServerImpl::processTransfer(Message& message) {
  // Ignore if view change is in progress
  if (state_ == VIEW_CHANGE) return;

  // Compute message digest
  std::string dataString;
  const TransactionData& data = message.data();
  data.SerializeToString(&dataString);
  std::string messageDigest = crypto::sha256Digest(dataString);

  // Verify the client's signature
  if (!verifySignature(message)) return;

  MessageInfo* entry = digestToEntry.find(messageDigest) == digestToEntry.end() ? nullptr : digestToEntry[messageDigest];

  // Process request
  if (serverId != getLeaderId()) {
    // Check if the request is already executed. If yes, simply send the reply to client
    if (entry != nullptr && entry->mstatus == types::EXECUTED) {
      notifyClient(message.signature().server_id(), message, entry->result);
      return;
    }

    // Forward the request to leader  
    // If the request is the result of a retry attempt by client, the node will wait for the leader to broadcast pre-prepares
    ResponseData *call = new ResponseData(this, responseCQ.get());
    std::chrono::time_point deadline = std::chrono::system_clock::now() + std::chrono::seconds(rpcTimeoutSeconds);
    call->sendTransfer(message, serverStubs_[getLeaderId()], deadline);    
    
    // Start a timer for view change. Triggers view change if the leader does not initiate the protocol on time
    setViewChangeTimer(false, currentView + 1, viewChangeTimeoutDelta, messageDigest);
    return;
  }

  // Ignore the request if the protocol is already initiated for it
  if (entry != nullptr) return;

  // Increment sequence number
  ++currentSequence;

  // Compose a pre-prepare request
  PrePrepareRequest prePrepareReq;
  Request* r = prePrepareReq.mutable_r();
  Context *context = r->mutable_data();
  context->set_view_num(currentView);
  context->set_sequence_num(currentSequence);    
  context->set_digest(messageDigest);
  
  Message* m = prePrepareReq.mutable_m();
  m->CopyFrom(message);

  // Create and add a new log entry
  int index = currentSequence - lowWatermark;
  entry = new MessageInfo();
  entry->m.CopyFrom(message);
  entry->mstatus = types::PRE_PREPARED;
  entry->prePrepare.CopyFrom(prePrepareReq);
  entry->prepareProofs.clear();
  entry->commitProofs.clear();
  log[index] = entry;
  digestToEntry[messageDigest] = entry;

  // broadcast pre-prepare request and the original client message
  sendPrePrepareToAll(prePrepareReq);
}

void PbftServerImpl::processPrePrepare(PrePrepareRequest& request) {
  // Ignore if view change is in progress
  if (state_ == VIEW_CHANGE) return;

  // Ignore request if not from leader
  if (request.r().sig_vec().server_id() != getLeaderId()) return;

  // Verify MAC signature
  bool validSignature = verifySignature(request.m());
  if (!validSignature) return;
  
  // Check request validity
  int viewNum = request.r().data().view_num();
  int seqNum = request.r().data().sequence_num();
  std::string digest = request.r().data().digest();
  
  if (viewNum != currentView) return;
  if (seqNum < lowWatermark || seqNum > highWatermark) return;
  
  std::string messageString;
  request.m().data().SerializeToString(&messageString);
  if (digest != crypto::sha256Digest(messageString)) return;
  
  int index = seqNum - lowWatermark;

  // Another entry with same view number and seq number already present
  if (log[index] != nullptr && 
      (viewNum <= log[index]->prePrepare.data().view_num() 
      || digest == log[index]->prePrepare.data().digest())) return;
  
  // Create an entry for this slot if needed
  if (log[index] == nullptr) log[index] = new MessageInfo();
  
  // Update log entry metadata
  log[index]->m.CopyFrom(request.m());
  log[index]->mstatus = types::PRE_PREPARED;
  log[index]->prePrepare.CopyFrom(request);
  log[index]->prepareProofs.clear();
  log[index]->commitProofs.clear();
  digestToEntry[digest] = log[index];

  // Compose pre-prepare-ok
  Request prePrepareOk;
  prePrepareOk.mutable_data()->CopyFrom(request.r().data());

  // Append own's pre-prepare ok to valid prepare proofs
  log[index]->prepareProofs[serverId] = prePrepareOk;

  // send pre-prepare-ok
  sendRequest(prePrepareOk, request.r().sig_vec().server_id(), types::PRE_PREPARE_OK);

  // set a view change timer for this request
  setViewChangeTimer(false, currentView + 1, viewChangeTimeoutDelta, digest);
}

void PbftServerImpl::processPrePrepareOk(Request& request) {
  // Ignore if view change is in progress
  if (state_ == VIEW_CHANGE) return;

  // Ignore request if not intended for leader
  if (serverId != getLeaderId()) return;

  // Verify MAC signature
  bool validSignature = verifySignature(request);
  if (!validSignature) return;

  // Check request validity
  int seqNum = request.data().sequence_num();
  int index = seqNum - lowWatermark;
  
  if (log[index]->prePrepare.data().digest() != request.data().digest()) return;

  // Append pre-prepare ok to prepare proofs
  log[index]->prepareProofs[request.sig_vec().server_id()] = request;
  
  // Check if 2f matching acks. If yes, set an optimistic timer
  if (!isByzantine && log[index]->prepareProofs.size() == 2 * f) {
    log[index]->mstatus = types::PREPARED;

    // Wait to collect 3*f prepares. Then initiate either prepare or commit phase
    setOptimisticTimer(index);
  }
}

void PbftServerImpl::processPrepare(ProofRequest& request) {
  // Ignore if view change is in progress
  if (state_ == VIEW_CHANGE || isByzantine) return;

  // Check request validity
  int viewNum = request.data().view_num();
  int seqNum = request.data().sequence_num();
  std::string digest = request.data().digest();

  if (viewNum != currentView) return;
  if (seqNum < lowWatermark || seqNum > highWatermark) return;
  
  int index = seqNum - lowWatermark;

  // Ignore if not pre-prepared
  if (log[index] == nullptr || log[index]->mstatus != types::PRE_PREPARED) return;

  // Check for valid matching prepares and update log entry metadata
  std::string dataString;
  const Context& data = request.data();
  data.SerializeToString(&dataString);

  for (int i = 0; i < request.sig_vecs_size(); i++) {
    int senderId = request.sig_vecs(i).server_id();
    std::string binPath = Utils::macKeyPath(serverId, senderId);
    bool validSignature = senderId == serverId ? true : crypto::verifyMAC(dataString, request.sig_vecs(i).signatures(serverId), binPath);
    
    if (validSignature 
            && viewNum == log[index]->prePrepare.data().view_num()
            && digest == log[index]->prePrepare.data().digest()) {
        
        Request r;
        r.mutable_data()->CopyFrom(request.data());
        r.mutable_sig_vec()->CopyFrom(request.sig_vecs(i));
        log[index]->prepareProofs[senderId] = r;
    }
  }

  // Check if 2f valid prepares
  if (!isByzantine && log[index]->prepareProofs.size() == 2 * f) {
      // Update log entry metadata
      log[index]->mstatus = types::PREPARED;

      // Compose commit request
      Request commitReq;
      commitReq.mutable_data()->CopyFrom(request.data());

      // Send commit request
      sendRequest(commitReq, getLeaderId(), types::COMMIT);
  }
}

void PbftServerImpl::processPrepareOk(Request& request) {
  // Ignore if view change is in progress
  if (state_ == VIEW_CHANGE) return;

  // Ignore request if not intended for leader
  if (serverId != getLeaderId()) return;

  // Verify MAC signature
  bool validSignature = verifySignature(request);
  if (!validSignature) return;

  // Check request validity
  int seqNum = request.data().sequence_num();
  int index = seqNum - lowWatermark;
  
  if (log[index]->prePrepare.data().digest() != request.data().digest()) return;

  // Append prepare ok to commit proofs
  log[index]->commitProofs[request.sig_vec().server_id()] = request;
  
  // Check if 2f + 1 matching acks. If yes, broadcast commit
  if (log[index]->commitProofs.size() == 2 * f + 1) {
    log[index]->mstatus = types::COMMITTED;
    ProofRequest commit;
    commit.mutable_data()->CopyFrom(log[index]->prePrepare.data());

    // Attach commit proofs
    for (auto& pair: log[index]->commitProofs) {
      SignatureVec* vec = request.mutable_sig_vec();
      vec->CopyFrom(pair.second.sig_vec());
    }

    sendRequestToAll(commit, types::COMMIT);
  }
}

void PbftServerImpl::processCommit(ProofRequest& request) {
  // Ignore if view change is in progress
  if (state_ == VIEW_CHANGE) return;

  // Check request validity
  int viewNum = request.data().view_num();
  int seqNum = request.data().sequence_num();
  std::string digest = request.data().digest();

  if (viewNum != currentView) return;
  if (seqNum < lowWatermark || seqNum > highWatermark) return;
  
  int index = seqNum - lowWatermark;

  // Ignore if not pre-prepared
  if (log[index] == nullptr || !(log[index]->mstatus != types::PRE_PREPARED || log[index]->mstatus != types::PREPARED)) return;

  // Check for valid matching commits and update log entry metadata
  std::string dataString;
  const Context& data = request.data();
  data.SerializeToString(&dataString);

  for (int i = 0; i < request.sig_vecs_size(); i++) {
    int senderId = request.sig_vecs(i).server_id();
    std::string binPath = Utils::macKeyPath(serverId, senderId);
    bool validSignature = senderId == serverId ? true : crypto::verifyMAC(dataString, request.sig_vecs(i).signatures(serverId), binPath);
    
    if (validSignature 
            && viewNum == log[index]->prePrepare.data().view_num()
            && digest == log[index]->prePrepare.data().digest()) {
        
        Request r;
        r.mutable_data()->CopyFrom(request.data());
        r.mutable_sig_vec()->CopyFrom(request.sig_vecs(i));
        log[index]->commitProofs[senderId] = r;
    }
  }

  // Check if 2f + 1 valid commits
  if (log[index]->commitProofs.size() == 2 * f + 1) {
      // Update log entry metadata
      log[index]->mstatus = types::COMMITTED;
      executePending(index);
  }
}

void PbftServerImpl::processViewChange(ViewChangeReq& request) {
  if (!verifyViewChange(request)) return;
  
  // Check request validity
  int newView = request.data().view_num();
  if (newView <= currentView) return;

  // Accept the view change message
  if (viewChangeMessages.find(newView) == viewChangeMessages.end()) {
    viewChangeMessages[newView] = std::vector<ViewChangeReq>();
  }

  viewChangeMessages[newView].push_back(request);

  // Check if non-leader and f + 1 valid view change messages
  if (newView % clusterSize != serverId && viewChangeMessages[newView].size() == f + 1) {
      triggerViewChange(newView);
      // Set timer for consecutive view change
      setViewChangeTimer(true, newView + 1, viewChangeTimeoutDelta, "");
      return;
  }

  // Check if leader and 2f matching view change messages
  if (!isByzantine && newView % clusterSize == serverId && viewChangeMessages[newView].size() == 2 * f) {
    // Add leader's view change request
    ViewChangeReq viewChange;
    composeViewChangeRequest(newView, viewChange);
    viewChangeMessages[newView].push_back(viewChange);

    // Compose new view request
    NewViewReq newViewReq;
    NewViewData* data = newViewReq.mutable_data();
    data->set_view_num(newView);

    
    for (auto& vproof: viewChangeMessages[newView]) {
      ViewChangeReq *v = data->add_vproofs();
      v->CopyFrom(vproof);
    }

    std::vector<Request> bigO;
    CheckpointData lastStableCheckpoint;
    int checkpointServerId;
    computeBigO(viewChangeMessages[newView], bigO, lastStableCheckpoint, checkpointServerId);

    for (auto& prePrepare: bigO) {
      Request* r = data->add_pre_prepares();
      r->CopyFrom(prePrepare);
    }

    // Send out new view request
    sendNewViewToAll(newViewReq);
  }
}

void PbftServerImpl::processNewView(const NewViewReq& request) {
  // Verify the MAC signature
  if (!verifySignature(request)) return;

  // Ignore if the view change message is old
  if (request.data().view_num() <= currentView) return;

  // Verify the view change messages
  int validViewChanges = 0;
  for (int i = 0; i < request.data().vproofs_size(); i++) {
    const ViewChangeReq& proof = request.data().vproofs(i);
    if (verifyViewChange(proof)) validViewChanges++;
  }

  if (validViewChanges < 2 * f + 1) return;

  // Iterate over view change messages and compute the set O
  std::vector<Request> bigO;
  CheckpointData lastStableCheckpoint;
  int checkpointServerId;

  std::vector<ViewChangeReq> viewChanges = std::vector<ViewChangeReq>();
  for (int i = 0; i < request.data().vproofs_size(); i++) {
    viewChanges.push_back(request.data().vproofs(i));
  }
  computeBigO(viewChanges, bigO, lastStableCheckpoint, checkpointServerId);

  // Request if sync if node is lagging
  if (lastStableCheckpoint.c_seq_num() > lastStableCheckpointSeqNum) {
    // Compose a sync request
    SyncReq syncReq;
    syncReq.mutable_data()->CopyFrom(lastStableCheckpoint);

    // Send a sync request
    sendSync(syncReq, serverId, checkpointServerId);
    retryNewView(request, retryTimeoutSeconds);
    return;
  }

  // Verify the correctness of bigO sent in new view message
  for (int i = 0; i < bigO.size(); i++) {
    Request& prePrepare = bigO[i];
    if (prePrepare.data().view_num() != request.data().pre_prepares(i).data().view_num()
          || prePrepare.data().sequence_num() != request.data().pre_prepares(i).data().sequence_num()
          || prePrepare.data().digest() != request.data().pre_prepares(i).data().digest()) return;
  }

  // Big O verified. Enter new view
  state_ = NORMAL;
  currentView = request.data().view_num();
  viewChangeTimeoutSeconds = 0;

  // Send out a prepare message for each of the pre prepares
  for (int i = 0; i < request.data().pre_prepares_size(); i++) {
    const Request& prePrepare = request.data().pre_prepares(i);
    Request prePrepareOk;
    prePrepareOk.mutable_data()->CopyFrom(prePrepare.data());
    sendRequest(prePrepareOk, getLeaderId(), types::PRE_PREPARE_OK);
  }

  return;
}

void PbftServerImpl::processCheckpoint(CheckpointReq& request) {
  // Verify the signature
  if (!verifySignature(request)) return;

  int checkpointedSeqNum = request.data().c_seq_num();
  std::string checkpointedDigest = request.data().c_digest();

  // Ignore if the replica/leader has not checkpointed this sequence number
  if (checkpoints.find(checkpointedSeqNum) == checkpoints.end()) return;
  if (checkpoints[checkpointedSeqNum]->digest != checkpointedDigest) return;

  // Accept the checkpoint request
  checkpoints[checkpointedSeqNum]->checkpointProofs[request.signature().server_id()] = request;

  // Mark it as a stable checkpoint if enough proofs are available
  if (checkpoints[checkpointedSeqNum]->checkpointProofs.size() == f + 1) {
    lastStableCheckpointSeqNum = checkpointedSeqNum;
    lastStableCheckpointDigest = checkpoints[checkpointedSeqNum]->digest;
    lastStableCheckpointState = checkpoints[checkpointedSeqNum]->state;
    lastStableCheckpointProofs = checkpoints[checkpointedSeqNum]->checkpointProofs;

    // Remove the checkpoint from the list of checkpoints
    checkpoints.erase(checkpointedSeqNum);

    // Adjust the watermarks
    lowWatermark = lastExecuted + 1;
    highWatermark = lowWatermark + k - 1;

    int entriesToRemove = lastStableCheckpointSeqNum - lowWatermark;
    // Clear digestToEntry
    for (int i = 0; i < entriesToRemove; i++) {
      digestToEntry.erase(log[i]->prePrepare.data().digest());
    }

    // Truncate the log
    log.erase(log.begin(), log.end() + entriesToRemove);

    // Add empty log entries
    std::vector<MessageInfo*> blankEntries = std::vector<MessageInfo*>(entriesToRemove);
    log.insert(log.begin(), blankEntries.begin(), blankEntries.end());
  }
}

void PbftServerImpl::processSync(SyncReq& request, SyncResp& reply) {
  // Verify the signature
  if (!verifySignature(request)) return;

  // Check for request validity
  if (lastStableCheckpointSeqNum < request.data().c_seq_num()) return;
  // if (lastStableCheckpointDigest != request.data().c_digest()) return;

  SyncResData* data = reply.mutable_data();
  CheckpointData* cdata = data->mutable_cdata();
  cdata->set_c_seq_num(lastStableCheckpointSeqNum);
  cdata->set_c_digest(lastStableCheckpointDigest);

  for (auto& pair: balances) {
    (*data->mutable_balances())[pair.first] = pair.second;
  }

  // Sign the reply
  std::string dataString;
  data->SerializeToString(&dataString);
  addMACSignature(dataString, request.signature().server_id(), request.mutable_signature());
}

bool PbftServerImpl::verifyViewChange(const ViewChangeReq& request) {
  // Verify MAC signature
  if (!verifySignature(request)) return false;

  // Verify CheckpointReq proof
  int validCProofs = 0;
  for (int i = 0; i < request.data().cproofs_size(); i++) {
    const CheckpointReq& c = request.data().cproofs(i);
    if (c.data().c_seq_num() == request.data().last_checkpoint().c_seq_num()
          && c.data().c_digest() == request.data().last_checkpoint().c_digest()
          && verifySignature(c)) {
       validCProofs++;
    }
  }
  if (validCProofs < 2 * f + 1) return false;  

  // Verify Prepare proofs
  for (int i = 0; i < request.data().pproofs_size(); i++) {
    const PrepareProof& p = request.data().pproofs(i);
    const Request& prePrepare = p.pre_prepare();
    
    int validPProofs = 0;
    for (int j = 0; i < p.prepares_size(); j++) {
      const Request& prepare = p.prepares(j);
      if (prepare.data().digest() == prePrepare.data().digest() && verifySignature(prepare)) {
        validPProofs++;
      }
    }

    if (validPProofs != 2 * f) return false;
  }

  return true;
}

void PbftServerImpl::computeBigO(std::vector<ViewChangeReq>& viewChanges, std::vector<Request>& bigO, CheckpointData& lastStableCheckpoint, int& checkpointServerId) {
  int maxStableCheckpoint = 0;
  int maxSequenceNum = 0;
  std::map<int, Request> megaPrePrepares = std::map<int, Request>();

  for (auto& vproof: viewChanges) {
    if (maxStableCheckpoint < vproof.data().last_checkpoint().c_seq_num()) {
      maxStableCheckpoint = vproof.data().last_checkpoint().c_seq_num();
      lastStableCheckpoint.CopyFrom(vproof.data().last_checkpoint());
      checkpointServerId = vproof.signature().server_id();
    }
  }

  for (auto& vproof: viewChanges) {
    // Verify Prepared proofs
    for (int i = 0; i < vproof.data().pproofs_size(); i++) {
      const PrepareProof& p = vproof.data().pproofs(i);
      const Request& prePrepare = p.pre_prepare();
      if (prePrepare.data().sequence_num() <= maxStableCheckpoint) continue;
      
      maxSequenceNum = std::max(maxSequenceNum, prePrepare.data().sequence_num());

      int validPProofs = 0;
      for (int j = 0; j < p.prepares_size(); j++) {
        const Request& prepare = p.prepares(j);
        if (prepare.data().digest() == prePrepare.data().digest() && verifySignature(prepare)) {
          validPProofs++;
        }
      }

      if (validPProofs == 2 * f) {
        if (megaPrePrepares.find(prePrepare.data().sequence_num()) == megaPrePrepares.end()
              || prePrepare.data().view_num() > megaPrePrepares[prePrepare.data().sequence_num()].data().view_num()) {
          megaPrePrepares[prePrepare.data().sequence_num()] = prePrepare;
        }
      }
    }
  }

  for (int i = maxStableCheckpoint + 1; i < maxSequenceNum; i++) {
    if (megaPrePrepares.find(i) != megaPrePrepares.end()) {
      bigO.push_back(megaPrePrepares[i]);
    } else {
      Request prePrepare;
      Context* c = prePrepare.mutable_data();

      // (-1, -1, "") indicates NOP
      c->set_view_num(-1);
      c->set_sequence_num(-1);
      c->set_digest("");
      

      std::string dataString;
      c->SerializeToString(&dataString);

      // generate a MAC vector
      addMACSignature(dataString, prePrepare.mutable_sig_vec());
      bigO.push_back(prePrepare);
    }
  }

}

void PbftServerImpl::composeViewChangeRequest(int viewNum, ViewChangeReq& request) {
  ViewChangeData* data = request.mutable_data();

  // Set the new view number and last stable checkpoint
  data->set_view_num(viewNum);

  CheckpointData* cdata = data->mutable_last_checkpoint();
  cdata->set_c_seq_num(lastStableCheckpointSeqNum);
  cdata->set_c_digest(lastStableCheckpointDigest);

  // Attach proof for last stable checkpoint
  for (auto& pair: lastStableCheckpointProofs) {
    CheckpointReq* r = data->add_cproofs();
    r->CopyFrom(pair.second);
  }

  // For every seq number > s, if the entry is prepared, attach its valid pre-prepare and 2f matching prepares
  int checkpointOffset = lastStableCheckpointSeqNum - lowWatermark;
  for (int i = checkpointOffset + 1; i < k; i++) {
    // If log entry is prepared, attach its proof
    if (log[i]->mstatus == types::PREPARED || log[i]->mstatus == types::COMMITTED || log[i]->mstatus == types::EXECUTED) {
        PrepareProof* pproof = data->add_pproofs();
        pproof->mutable_pre_prepare()->CopyFrom(log[i]->prePrepare);

        for (int j = 0; j < log[i]->prepareProofs.size(); j++) {
          Request* prepare = pproof->add_prepares();
          prepare->CopyFrom(log[i]->prepareProofs[j]);
        }
    }
  }
}

void PbftServerImpl::triggerViewChange(int viewNum) {
  state_ = VIEW_CHANGE;

  ViewChangeReq request;
  composeViewChangeRequest(viewNum, request);
  
  // Broadcast the view change request
  sendViewChangeToAll(request);

  // Set timer for consecutive view change
  viewChangeTimeoutSeconds += viewChangeTimeoutDelta;
  setViewChangeTimer(true, viewNum + 1, viewChangeTimeoutSeconds, "");
}

void PbftServerImpl::setViewChangeTimer(bool consecutiveViewChange, int newView, int timeoutSeconds, std::string digest) {
  std::future<void> f = std::async(std::launch::async, [this, consecutiveViewChange, newView, digest, timeoutSeconds] () {
    std::this_thread::sleep_for(std::chrono::seconds(timeoutSeconds));

    if (consecutiveViewChange && state_ == VIEW_CHANGE) {
      triggerViewChange(newView);

    } else if (!consecutiveViewChange && state_ == NORMAL) {
      // View change happened already. Ignore
      if (currentView >= newView) return;
          
      // Cases
      // 1. The client sent a broadcast request and the leader has not initiated the protocol
      // 2. The leader initiated the protocol but hasn't completed it
      if (digestToEntry.find(digest) == digestToEntry.end() 
                || digestToEntry[digest]->mstatus != types::EXECUTED) {
        // Trigger view change
        triggerViewChange(newView);
      }
    }
  });

  // Store the variable to prevent it from going out-of-scope which causes blocking
  futures.push(std::move(f));
}

void PbftServerImpl::setOptimisticTimer(int index) {
  std::future<void> f = std::async(std::launch::async, [this, index] () {
    std::this_thread::sleep_for(std::chrono::seconds(optimisticTimeoutSeconds));

    // In View Change state. Ignore.
    if (state_ == VIEW_CHANGE) return;

    ProofRequest request;
    request.mutable_data()->CopyFrom(log[index]->prePrepare.data());

    // Attack prepare proofs
    for (auto& pair: log[index]->prepareProofs) {
      SignatureVec* vec = request.add_sig_vecs();
      vec->CopyFrom(pair.second.sig_vec());
    }

    if (log[index]->prepareProofs.size() == 3 * this->f) {
      // Directly broadcast out a commit request
      log[index]->mstatus = types::COMMITTED;
      log[index]->commitProofs = log[index]->prepareProofs;
      
      // Add leader's own commit proof
      Request r;
      r.mutable_data()->CopyFrom(log[index]->prePrepare.data());
      std::string dataString;
      log[index]->prePrepare.data().SerializeToString(&dataString);
      addMACSignature(dataString, r.mutable_sig_vec());
      log[index]->commitProofs[serverId] = r;
      request.add_sig_vecs()->CopyFrom(r.sig_vec());

      sendRequestToAll(request, types::COMMIT);
      executePending(index);

    } else {
      // Broadcast a prepare request
      sendRequestToAll(request, types::PREPARE);
    }

  });

  // Store the variable to prevent it from going out-of-scope which causes blocking
  futures.push(std::move(f));
}


void PbftServerImpl::retryNewView(const NewViewReq& request, int retryTimeoutSeconds) {
    std::future<void> f = std::async(std::launch::async, [this, request, retryTimeoutSeconds] () {
      std::this_thread::sleep_for(std::chrono::seconds(retryTimeoutSeconds));

      if (request.data().view_num() > currentView) {
        processNewView(request);
      }
    });

    futures.push(std::move(f));
}


void PbftServerImpl::executePending(int lastCommitted) {
  for (int i = lastExecuted + 1; i <= lastCommitted; i++) {
    if (log[i]->mstatus == types::COMMITTED) {
      std::string sender = log[i]->m.data().sender();
      std::string receiver = log[i]->m.data().receiver();
      int amount = log[i]->m.data().amount();

      log[i]->mstatus = types::EXECUTED;
      bool result = false;
      if (balances[sender] >= amount) {
        balances[sender] -= amount;
        balances[receiver] += amount;
        result = true;
      }

      log[i]->result = result;
      lastExecuted = i;
      notifyClient(log[i]->m.signature().server_id(), log[i]->m, result);

      if (i % checkpointStepSize == checkpointStepSize - 1) {
        checkpoint();
      }

    } else {
      break;
    }
  }
}

void PbftServerImpl::checkpoint() {
  // Save the DB state as the checkpoint
  CheckpointInfo* cinfo = new CheckpointInfo;
  cinfo->state = balances;

  std::string balancesString = "{";
  for (const auto& pair : balances) {
      balancesString += pair.first + ": " + std::to_string(pair.second) + ", ";
  }
  // Remove the last comma and space
  if (!balancesString.empty()) {
      balancesString.pop_back();
      balancesString.pop_back();
  }
  balancesString += "}";

  cinfo->digest = crypto::sha256Digest(balancesString);
  cinfo->checkpointProofs = std::map<int, CheckpointReq>();
  checkpoints[lastExecuted] = cinfo;

  // Send out a checkpoint request
  CheckpointReq request;
  CheckpointData* cdata = request.mutable_data();
  cdata->set_c_seq_num(lastExecuted);
  cdata->set_c_digest(cinfo->digest);
  sendCheckpointToAll(request);
}

void PbftServerImpl::notifyClient(int clientId, Message& m, bool res) {
  Response reply;
  pbft::ResponseData* replyData = reply.mutable_data();
  replyData->mutable_tdata()->CopyFrom(m.data());
  replyData->set_ack(res);
  replyData->set_view_num(currentView);

  std::string replyDataString;
  replyData->SerializeToString(&replyDataString);
  addECDSASignature(replyDataString, reply.mutable_signature());

  ResponseData *call = new ResponseData(this, responseCQ.get());
  std::unique_ptr<PbftClient::Stub>& stub_ = clientStubs_[clientId];
  std::chrono::time_point deadline = std::chrono::system_clock::now() + std::chrono::seconds(rpcTimeoutSeconds);
  call->sendNotify(reply, stub_, deadline);
}

void PbftServerImpl::GetLog(GetLogRes& reply) {
  for (int i = 0; i < currentSequence; i++) {
    LogEntry* e = reply.add_entries();
    TransactionData* tdata = e->mutable_t();
    tdata->CopyFrom(log[i]->m.data());
    e->set_matching_prepares(log[i]->prepareProofs.size());
    e->set_matching_commits(log[i]->commitProofs.size());
  }
}

void PbftServerImpl::GetDB(GetDBRes& reply) {
  for (int i = 0; i < Constants::clientAddresses.size(); i++) {
    std::string client = std::string(1, 'A' + i);
    reply.add_balances(balances[client]);
  }
}

void PbftServerImpl::GetStatus(GetStatusReq& request, GetStatusRes& reply) {
  int seqNum = request.seq_num();
  int index = seqNum - lowWatermark;

  std::string status;
  switch (log[index]->mstatus) {
    case types::NO_STATUS:
      status = "NS";
      break;
    case types::PRE_PREPARED:
      status = "PP";
      break;
    case types::PREPARED:
      status = "P";
      break;
    case types::COMMITTED:
      status = "C";
      break;
    case types::EXECUTED:
      status = "E";
      break;
  }
  reply.set_status(status);
}

void PbftServerImpl::GetViewChanges(GetViewChangesRes& reply) {
  for (auto& pair: viewChangeMessages) {  
    for (auto& v: pair.second) {
      ViewChangesResEntry* e = reply.add_view_changes();
      e->set_view_num(pair.first);
      e->set_initiator("S" + std::to_string(v.signature().server_id() + 1));
      e->set_stable_checkpoint(v.data().last_checkpoint().c_seq_num());
    }
  }
}

void RunServer(int serverId, std::string serverName, std::string serverAddress, bool isByzantine) {
  PbftServerImpl server(serverId, serverName, isByzantine);
  server.run(serverAddress);
}

int main(int argc, char** argv) {
  
  if (argc < 5) {
    std::cerr << "Usage: " << argv[0] << "<server_id> <server_name> <target_address> <is_byzantine>" << std::endl;
    return 1;
  }

  try {
    RunServer(std::stoi(argv[1]), argv[2], argv[3], argv[4]);
  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << std::endl;
  }


  return 0;
}