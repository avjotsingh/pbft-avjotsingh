#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <chrono>
#include <shared_mutex>
#include <map>
#include <queue>
#include <future>

#include <grpcpp/grpcpp.h>
#include "pbft.grpc.pb.h"
#include "absl/log/check.h"

#include "../types/request_types.h"
#include "../types/transaction.h"

using grpc::Channel;
using grpc::ServerAsyncResponseWriter;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::ServerCompletionQueue;
using grpc::CompletionQueue;
using grpc::Status;
using grpc::ServerContext;
using grpc::Server;
using google::protobuf::Empty;

using pbft::PbftServer;
using pbft::PbftClient;
using pbft::Response;
using pbft::Message;
using pbft::Transactions;

class PbftClientImpl final {
public:
    PbftClientImpl(int id, std::string name);
    ~PbftClientImpl();
    void run(std::string targetAddress);

private:
    struct TransferInfo {
        types::Transaction t;
        long timestamp;
        std::set<int> successes;
        std::set<int> failures;
    };

    class RequestData {

        public:
            RequestData(PbftClient::AsyncService* service, PbftClientImpl* server, ServerCompletionQueue* cq, types::RequestTypes type):
                service_(service), server_(server), cq_(cq), responder(&ctx_), type_(type) {
                    status_ = CREATE;
                    Proceed();
                }

            void Proceed() {
                if (status_ == CREATE) {
                    status_ = PROCESS;
                    switch (type_) {
                        case types::NOTIFY:
                            service_->RequestNotify(&ctx_, &notifyReq, &responder, cq_, cq_, this);
                            break;
                        
                        case types::PROCESS:
                            service_->RequestProcess(&ctx_, &processReq, &responder, cq_, cq_, this);

                        default:
                            break;
                    }
                } else if (status_ == PROCESS) {
                    status_ = FINISH;
                    switch (type_) {
                        case types::NOTIFY:
                            server_->processNotify(notifyReq);
                            responder.Finish(response, Status::OK, this);
                            break;

                        case types::PROCESS:
                            server_->processProcess(processReq);
                            responder.Finish(response, Status::OK, this);
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
            PbftClient::AsyncService* service_;
            PbftClientImpl* server_;
            ServerCompletionQueue* cq_;
            ServerContext ctx_;

            // Different request and response types PBFT client can expect to send and receive
            Response notifyReq;
            Transactions processReq;
            Empty response;

            // The means to get back to the client.
            ServerAsyncResponseWriter<google::protobuf::Empty> responder;
            
            enum RequestStatus { CREATE, PROCESS, FINISH };
            RequestStatus status_;  // The current serving state.
            types::RequestTypes type_;
    };

    class ResponseData {
        public:
            ResponseData(PbftClientImpl* server, CompletionQueue* cq):
                server_(server), cq_(cq) {}

            void sendMessage(Message& request, std::unique_ptr<PbftServer::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline) {
                    context.set_deadline(deadline);
                    responseReader = stub_->PrepareAsyncTransfer(&context, request, cq_);
                    responseReader->StartCall();
                    responseReader->Finish(&reply, &status, (void*) this);
            }

            void HandleRPCResponse() {
                delete this;
            }
            
        private:
            PbftClientImpl* server_;
            CompletionQueue* cq_;

            Empty reply;
            ClientContext context;
            Status status;
            std::unique_ptr<ClientAsyncResponseReader<Empty>> responseReader;
    };

    void doTransfers();
    int getLeaderId();
    void HandleRPCs();
    void processNotify(Response& request);
    void processProcess(Transactions& request);
    void transferBroadcast();
    void setTransferTimer(TransferInfo& info, int transferTimeoutSeconds);

    PbftClient::AsyncService service_;
    std::unique_ptr<Server> server_;

    int clientId;
    std::string clientName;
    std::unique_ptr<ServerCompletionQueue> requestCQ;
    std::unique_ptr<CompletionQueue> responseCQ;

    std::vector<std::unique_ptr<PbftServer::Stub>> stubs_;
    std::queue<std::future<void>> transferTimers;
    std::queue<TransferInfo> transfers;
    std::queue<types::Transaction> toProcess;

    int f;
    int currentView;
    int clusterSize;

    int rpcTimeoutSeconds;
    int transferTimeoutSeconds;

    double performance;
    std::shared_mutex performanceMutex;
    
    int transactionsIssued;
    int transactionsProcessed;

    std::map<int, std::chrono::system_clock::time_point> startTimes;
    double totalTimeTaken;
};