#pragma once

#include <string.h>
#include <chrono>
#include <future>
#include <queue>
#include <shared_mutex>

#include <grpcpp/grpcpp.h>
#include "absl/log/check.h"
#include "pbft.grpc.pb.h"

#include "../types/request_types.h"
#include "../types/transaction.h"

using grpc::Channel;
using grpc::Server;
using grpc::CompletionQueue;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::ServerAsyncResponseWriter;
using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;
using grpc::ServerBuilder;
using google::protobuf::Empty;

using pbft::PbftServer;
using pbft::PbftClient;
using pbft::Request;
using pbft::Message;
using pbft::PrePrepareRequest;
using pbft::ProofRequest;
using pbft::ViewChangeReq;
using pbft::NewViewReq;
using pbft::CheckpointReq;
using pbft::GetLogRes;
using pbft::GetDBRes;
using pbft::GetStatusReq;
using pbft::GetStatusRes;
using pbft::GetViewChangesRes;
using pbft::SyncReq;
using pbft::SyncResp;
using pbft::CheckpointData;
using pbft::Response;
using pbft::Signature;
using pbft::SignatureVec;
using pbft::SyncReq;
using pbft::SyncResp;


// Server Implementation
class PbftServerImpl final {
public:
    PbftServerImpl(int id, std::string name, bool byzantine);
    ~PbftServerImpl();
    void run(std::string targetAddress);
    void HandleRPCs();

    void processTransfer(Message& message);
    void processPrePrepare(PrePrepareRequest& request);
    void processPrePrepareOk(Request& request);
    void processPrepare(ProofRequest& request);
    void processPrepareOk(Request& request);
    void processCommit(ProofRequest& request);
    void processViewChange(ViewChangeReq& request);
    void processNewView(const NewViewReq& request);
    void processCheckpoint(CheckpointReq& request);
    void processSync(SyncReq& request, SyncResp& reply);

    void processGetLog(GetLogRes& reply);
    void processGetDB(GetDBRes& reply);
    void processGetStatus(GetStatusReq& request, GetStatusRes& reply);
    void processGetViewChanges(GetViewChangesRes& reply);

private:

    class RequestData {
        public:
            RequestData(PbftServer::AsyncService* service, PbftServerImpl* server, ServerCompletionQueue* cq, types::RequestTypes type):
                service_(service), server_(server), cq_(cq), 
                responder(&ctx_), syncResponder(&ctx_), getLogResponder(&ctx_), getDbResponder(&ctx_), getStatusResponder(&ctx_), getViewChangesResponder(&ctx_),
                type_(type) {
                    status_ = CREATE;
                    Proceed();
                }
            
            void Proceed() {
                if (status_ == CREATE) {
                    status_ = PROCESS;
                    
                    switch (type_) {
                        case types::TRANSFER:
                            service_->RequestTransfer(&ctx_, &transferReq, &responder, cq_, cq_, this);
                            break;
                        case types::PRE_PREPARE:
                            service_->RequestPrePrepare(&ctx_, &prePrepareReq, &responder, cq_, cq_, this);
                            break;
                        case types::PRE_PREPARE_OK:
                            service_->RequestPrePrepareOk(&ctx_, &prePrepareOkReq, &responder, cq_, cq_, this);
                            break;
                        case types::PREPARE:
                            service_->RequestPrepare(&ctx_, &prepareReq, &responder, cq_, cq_, this);
                            break;
                        case types::PREPARE_OK:
                            service_->RequestPrepareOk(&ctx_, &prepareOkReq, &responder, cq_, cq_, this);
                            break;
                        case types::COMMIT:
                            service_->RequestCommit(&ctx_, &commitReq, &responder, cq_, cq_, this);
                            break;
                        case types::VIEW_CHANGE:
                            service_->RequestViewChange(&ctx_, &viewChangeReq, &responder, cq_, cq_, this);
                            break;
                        case types::NEW_VIEW:
                            service_->RequestNewView(&ctx_, &newViewReq, &responder, cq_, cq_, this);
                            break;
                        case types::CHECKPOINT:
                            service_->RequestCheckpoint(&ctx_, &checkpointReq, &responder, cq_, cq_, this);
                            break;
                        case types::SYNC:
                            service_->RequestSync(&ctx_, &syncReq, &syncResponder, cq_, cq_, this);
                            break;
                        case types::GET_LOG:
                            service_->RequestGetLog(&ctx_, &emptyReq, &getLogResponder, cq_, cq_, this);
                            break;
                        case types::GET_DB:
                            service_->RequestGetDb(&ctx_, &emptyReq, &getDbResponder, cq_, cq_, this);
                            break;
                        case types::GET_STATUS:
                            service_->RequestGetStatus(&ctx_, &getStatusReq, &getStatusResponder, cq_, cq_, this);
                            break;
                        case types::GET_VIEW_CHANGES:
                            service_->RequestGetViewChanges(&ctx_, &emptyReq, &getViewChangesResponder, cq_, cq_, this);
                            break;
                        default:
                            break;
                    }
                } else if (status_ == PROCESS) {
                    new RequestData(service_, server_, cq_, type_);
                    
                    status_ = FINISH;
                    switch (type_) {
                        case types::TRANSFER:
                            server_->processTransfer(transferReq);
                            responder.Finish(reply, Status::OK, this);
                            status_ = FINISH;
                            break;
                        case types::PRE_PREPARE:
                            server_->processPrePrepare(prePrepareReq);
                            responder.Finish(reply, Status::OK, this);
                            status_ = FINISH;
                            break;
                        case types::PRE_PREPARE_OK:
                            server_->processPrePrepareOk(prePrepareOkReq);
                            responder.Finish(reply, Status::OK, this);
                            status_ = FINISH;
                            break;
                        case types::PREPARE:
                            server_->processPrepare(prepareReq);
                            responder.Finish(reply, Status::OK, this);
                            status_ = FINISH;
                            break;
                        case types::PREPARE_OK:
                            server_->processPrepareOk(prepareOkReq);
                            responder.Finish(reply, Status::OK, this);
                            status_ = FINISH;
                            break;
                        case types::COMMIT:
                            server_->processCommit(commitReq);
                            responder.Finish(reply, Status::OK, this);
                            status_ = FINISH;
                            break;
                        case types::VIEW_CHANGE:
                            server_->processViewChange(viewChangeReq);
                            responder.Finish(reply, Status::OK, this);
                            status_ = FINISH;
                            break;
                        case types::NEW_VIEW:
                            server_->processNewView(newViewReq);
                            responder.Finish(reply, Status::OK, this);
                            status_ = FINISH;
                            break;
                        case types::CHECKPOINT:
                            server_->processCheckpoint(checkpointReq);
                            responder.Finish(reply, Status::OK, this);
                            status_ = FINISH;
                            break;
                        case types::SYNC:
                            server_->processSync(syncReq, syncResp);
                            syncResponder.Finish(syncResp, Status::OK, this);
                            status_ = FINISH;
                            break;
                        case types::GET_LOG:
                            server_->GetLog(getLogRes);
                            getLogResponder.Finish(getLogRes, Status::OK, this);
                            status_ = FINISH;
                            break;
                        case types::GET_DB:
                            server_->GetDB(getDbRes);
                            getDbResponder.Finish(getDbRes, Status::OK, this);
                            status_ = FINISH;
                            break;
                        case types::GET_STATUS:
                            server_->GetStatus(getStatusReq, getStatusRes);
                            getStatusResponder.Finish(getStatusRes, Status::OK, this);
                            status_ = FINISH;
                            break;
                        case types::GET_VIEW_CHANGES:
                            server_->GetViewChanges(getViewChangesRes);
                            getViewChangesResponder.Finish(getViewChangesRes, Status::OK, this);
                            status_ = FINISH;
                            break;
                        default:
                            break;
                    }
                } else {
                    CHECK_EQ(status_, FINISH);
                    delete this;
                }
            }

        private:
            PbftServer::AsyncService* service_;
            PbftServerImpl* server_;
            ServerCompletionQueue* cq_;
            ServerContext ctx_;

            // Different request and response types that server can expect to receive and send to the client
            Message transferReq;
            PrePrepareRequest prePrepareReq;
            Request prePrepareOkReq;
            ProofRequest prepareReq;
            Request prepareOkReq;
            ProofRequest commitReq;
            Request commitOkReq;
            ViewChangeReq viewChangeReq;
            NewViewReq newViewReq;
            CheckpointReq checkpointReq;
            SyncReq syncReq;
            SyncResp syncResp;
            GetLogRes getLogRes;
            GetDBRes getDbRes;
            GetStatusReq getStatusReq;
            GetStatusRes getStatusRes;
            GetViewChangesRes getViewChangesRes;
            Empty emptyReq;
            
            Empty reply;


            // The means to get back to the client.
            ServerAsyncResponseWriter<Empty> responder;
            ServerAsyncResponseWriter<SyncResp> syncResponder;
            ServerAsyncResponseWriter<GetLogRes> getLogResponder;
            ServerAsyncResponseWriter<GetDBRes> getDbResponder;
            ServerAsyncResponseWriter<GetStatusRes> getStatusResponder;
            ServerAsyncResponseWriter<GetViewChangesRes> getViewChangesResponder;

            
            // Let's implement a tiny state machine with the following states.
            enum CallStatus { CREATE, PROCESS, FINISH };
            CallStatus status_;  // The current serving state.
            types::RequestTypes type_;
        };

    class ResponseData {
        public:
            ResponseData(PbftServerImpl* server, CompletionQueue* cq):
                server_(server), cq_(cq) {}

            void sendTransfer(Message& request, std::unique_ptr<PbftServer::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline) {
                context.set_deadline(deadline);
                responseReader = stub_->PrepareAsyncTransfer(&context, request, cq_);
                responseReader->StartCall();
                responseReader->Finish(&reply, &status, (void*) this);
            }

            void sendPrePrepare(PrePrepareRequest& request, std::unique_ptr<PbftServer::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline) {
                context.set_deadline(deadline);
                responseReader = stub_->PrepareAsyncPrePrepare(&context, request, cq_);
                responseReader->StartCall();
                responseReader->Finish(&reply, &status, (void*) this);
            }

            void sendPrePrepareOk(Request& request, std::unique_ptr<PbftServer::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline) {
                context.set_deadline(deadline);
                responseReader = stub_->PrepareAsyncPrePrepareOk(&context, request, cq_);
                responseReader->StartCall();
                responseReader->Finish(&reply, &status, (void*) this);
            }

            void sendPrepare(ProofRequest& request, std::unique_ptr<PbftServer::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline) {
                context.set_deadline(deadline);
                responseReader = stub_->PrepareAsyncPrepare(&context, request, cq_);
                responseReader->StartCall();
                responseReader->Finish(&reply, &status, (void*) this);
            }

            void sendPrepareOk(Request& request, std::unique_ptr<PbftServer::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline) {
                context.set_deadline(deadline);
                responseReader = stub_->PrepareAsyncPrepareOk(&context, request, cq_);
                responseReader->StartCall();
                responseReader->Finish(&reply, &status, (void*) this);
            }

            void sendCommit(ProofRequest& request, std::unique_ptr<PbftServer::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline) {
                context.set_deadline(deadline);
                responseReader = stub_->PrepareAsyncCommit(&context, request, cq_);
                responseReader->StartCall();
                responseReader->Finish(&reply, &status, (void*) this);
            }

            void sendViewChange(ViewChangeReq& request, std::unique_ptr<PbftServer::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline) {
                context.set_deadline(deadline);
                responseReader = stub_->PrepareAsyncViewChange(&context, request, cq_);
                responseReader->StartCall();
                responseReader->Finish(&reply, &status, (void*) this);
            }

            void sendNewView(NewViewReq& request, std::unique_ptr<PbftServer::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline) {
                context.set_deadline(deadline);
                responseReader = stub_->PrepareAsyncNewView(&context, request, cq_);
                responseReader->StartCall();
                responseReader->Finish(&reply, &status, (void*) this);
            }

            void sendCheckpoint(CheckpointReq& request, std::unique_ptr<PbftServer::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline) {
                context.set_deadline(deadline);
                responseReader = stub_->PrepareAsyncCheckpoint(&context, request, cq_);
                responseReader->StartCall();
                responseReader->Finish(&reply, &status, (void*) this);
            }

            void sendSync(SyncReq& request, std::unique_ptr<PbftServer::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline) {
                context.set_deadline(deadline);
                syncResponseReader = stub_->PrepareAsyncSync(&context, request, cq_);
                responseReader->StartCall();
                responseReader->Finish(&reply, &status, (void*) this);
            }

            void sendNotify(Response& request, std::unique_ptr<PbftClient::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline) {
                context.set_deadline(deadline);
                responseReader = stub_->PrepareAsyncNotify(&context, request, cq_);
                responseReader->StartCall();
                responseReader->Finish(&reply, &status, (void*) this);
            }

            void HandleRPCResponse() {
                delete this;
            }
            
        private:
            PbftServerImpl* server_;
            CompletionQueue* cq_;

            Empty reply;
            ClientContext context;
            Status status;
            std::unique_ptr<ClientAsyncResponseReader<google::protobuf::Empty>> responseReader;
            std::unique_ptr<ClientAsyncResponseReader<SyncResp>> syncResponseReader;
    };

    struct MessageInfo {
        Message m;
        types::MessageStatus mstatus;
        bool result;
        Request prePrepare;
        std::map<int, Request> prepareProofs;
        std::map<int, Request> commitProofs;
        
        MessageInfo() {
            mstatus = types::NO_STATUS;
        }
    };

    struct CheckpointInfo {
        std::map<std::string, int> state;
        std::string digest;
        std::map<int, CheckpointReq> checkpointProofs;
    };

    PbftServer::AsyncService service_;
    std::unique_ptr<Server> server_;

    int serverId;
    std::string serverName;
    bool isByzantine;
    std::unique_ptr<ServerCompletionQueue> requestCQ;               // queue for server requests
    std::unique_ptr<CompletionQueue> responseCQ;                    // queue for protocol responses
    std::vector<std::unique_ptr<PbftServer::Stub>> serverStubs_;    
    std::vector<std::unique_ptr<PbftClient::Stub>> clientStubs_;

    int f;                      // Parameter 'f' in PBFT
    int k;                      // max size of the sequence number window
    int currentView;
    int currentSequence;
    int clusterSize;
    int lowWatermark;
    int highWatermark;
    std::vector<struct MessageInfo*> log;
    std::map<std::string, MessageInfo*> digestToEntry;

    int checkpointStepSize;
    int lastStableCheckpointSeqNum;
    std::map<std::string, int> lastStableCheckpointState;
    std::map<int, CheckpointReq> lastStableCheckpointProofs;
    std::string lastStableCheckpointDigest;
    std::map<int, CheckpointInfo*> checkpoints;
    int lastExecuted;


    int rpcTimeoutSeconds;
    int viewChangeTimeoutSeconds;
    int viewChangeTimeoutDelta;
    int optimisticTimeoutSeconds;
    int retryTimeoutSeconds;

    std::queue<std::future<void>> futures;
    std::map<int, std::vector<ViewChangeReq>> viewChangeMessages;
    std::map<int, std::map<long, bool>> lastExecutedResult;

    enum ServerState { NORMAL, VIEW_CHANGE };
    ServerState state_;

    std::map<std::string, int> balances;

    int getLeaderId();
    void sendRequest(Request& request, int receiverId, types::RequestTypes type);
    void sendRequest(ProofRequest& request, int receiverId, types::RequestTypes type);
    void sendRequestToAll(ProofRequest& request, types::RequestTypes type);
  
    void sendPrePrepareToAll(PrePrepareRequest& request);
    void sendViewChangeToAll(ViewChangeReq& request);
    void sendNewViewToAll(NewViewReq& request);
    void sendCheckpointToAll(CheckpointReq& request);
    void sendSync(SyncReq& request, int senderId, int receiverId);
    void sendSyncRes(SyncResp& request, int senderId, int receiverId);

    void addMACSignature(std::string data, int receiveId, Signature* sigVec);
    void addMACSignature(std::string data, SignatureVec* sigVec);
    void addECDSASignature(std::string data, Signature* signature);
    bool verifySignature(const Request& request);
    bool verifySignature(const Message& request);
    bool verifySignature(const ViewChangeReq& request);
    bool verifySignature(const CheckpointReq& request);
    bool verifySignature(const NewViewReq& request);
    bool verifySignature(const SyncReq& request);

    bool verifyViewChange(const ViewChangeReq& request);
    void computeBigO(std::vector<ViewChangeReq>& viewChanges, std::vector<Request>& bigO, CheckpointData& lastStableCheckpoint, int& checkpointServerId);
    void composeViewChangeRequest(int viewNum, ViewChangeReq& request);
    void triggerViewChange(int viewNum);
    void setViewChangeTimer(bool consecutiveViewChange, int newView, int timeoutSeconds, std::string digest);
    void setOptimisticTimer(int index);
    void retryNewView(const NewViewReq& request, int retryTimeoutSeconds);

    void executePending(int lastCommitted);
    void checkpoint();
    void notifyClient(int clientId, Message& m, bool res);

    void GetLog(GetLogRes& reply);
    void GetDB(GetDBRes& reply);
    void GetStatus(GetStatusReq& request, GetStatusRes& reply);
    void GetViewChanges(GetViewChangesRes& reply);

};