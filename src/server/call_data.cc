#include "call_data.h"
#include "../types/transaction.h"
#include <shared_mutex>
#include <string.h>
#include "absl/log/check.h"
#include <random>

using grpc::Status;
using grpc::Alarm;

CallData::CallData(Paxos::AsyncService* service, PaxosServer* server, ServerCompletionQueue* cq, types::RequestTypes type):
            transferResponder(&ctx_), 
            getBalanceResponder(&ctx_), 
            getLogsResponder(&ctx_), 
            getDBLogsResponder(&ctx_),
            prepareResponder(&ctx_),
            acceptResponder(&ctx_),
            commitResponder(&ctx_),
            syncResponder(&ctx_) {
        service_ = service;
        server_ = server;
        cq_ = cq;
        status_ = CREATE;
        callType = type;
        retry_count_ = 0;
        
        Proceed();
      }

void CallData::Proceed() {
    if (status_ == CREATE) {
        status_ = PROCESS;
        switch (callType) {
            case types::TRANSFER: 
                service_->RequestTransfer(&ctx_, &transferReq, &transferResponder, cq_, cq_, this);
                break;
            case types::GET_BALANCE:
                service_->RequestGetBalance(&ctx_, &getBalanceReq, &getBalanceResponder, cq_, cq_, this);
                break;
            case types::GET_LOGS:
                service_->RequestGetLogs(&ctx_, &getLogsReq, &getLogsResponder, cq_, cq_, this);
                break;
            case types::GET_DB_LOGS:
                service_->RequestGetDBLogs(&ctx_, &getDBLogsReq, &getDBLogsResponder, cq_, cq_, this);
                break;
            case types::PREPARE:
                service_->RequestPrepare(&ctx_, &prepareReq, &prepareResponder, cq_, cq_, this);
                break;
            case types::ACCEPT:
                service_->RequestAccept(&ctx_, &acceptReq, &acceptResponder, cq_, cq_, this);
                break;
            case types::COMMIT:
                service_->RequestCommit(&ctx_, &commitReq, &commitResponder, cq_, cq_, this);
                break;
            case types::SYNC:
                service_->RequestSync(&ctx_, &syncReq, &syncResponder, cq_, cq_, this);
                break;
        }
        
    } else if (status_ == PROCESS || status_ == RETRY) {
        // Spawn a new CallData instance to serve new clients while we process
        // the one for this CallData. The instance will deallocate itself as
        // part of its FINISH state.
        if (status_ == PROCESS) new CallData(service_, server_, cq_, callType);

        // And we are done! Let the gRPC runtime know we've finished, using the
        // memory address of this instance as the uniquely identifying tag for
        // the event.
        switch (callType) {
            case types::TRANSFER:
                if (server_->processTransferCall(&transferReq, &transferRes)) {
                    transferResponder.Finish(transferRes, Status::OK, this);
                    status_ = FINISH;
                } else {
                    // std::cout << "retry transfer" << std::endl;
                    status_ = RETRY;
                    Retry();
                }
                break;
            case types::GET_BALANCE:
                if (server_->processGetBalanceCall(&getBalanceRes)) {
                    getBalanceResponder.Finish(getBalanceRes, Status::OK, this);
                    status_ = FINISH;
                } else {
                    status_ = RETRY;
                    Retry();
                }
                break;
            case types::GET_LOGS:
                if (server_->processGetLogsCall(&getLogsRes)) {
                    getLogsResponder.Finish(getLogsRes, Status::OK, this);
                    status_ = FINISH;
                } else {
                    status_ = RETRY;
                    Retry();
                }
                break;
            case types::GET_DB_LOGS:
                if (server_->processGetDBLogsCall(&getDBLogsRes)) {
                    getDBLogsResponder.Finish(getDBLogsRes, Status::OK, this);
                    status_ = FINISH;
                } else {
                    status_ = RETRY;
                    Retry();
                }
                break;
            case types::PREPARE:
                if (server_->processPrepareCall(&prepareReq, &prepareRes)) {
                    prepareResponder.Finish(prepareRes, Status::OK, this);
                    status_ = FINISH;
                } else {
                    status_ = RETRY;
                    Retry();
                }
                break;
            case types::ACCEPT:
                if (server_->processAcceptCall(&acceptReq, &acceptRes)) {
                    acceptResponder.Finish(acceptRes, Status::OK, this);
                    status_ = FINISH;
                } else {
                    status_ = RETRY;
                    Retry();
                }
                break;
            case types::COMMIT:
                if (server_->processCommitCall(&commitReq)) {
                    commitResponder.Finish(commitRes, Status::OK, this);
                    status_ = FINISH;
                } else {
                    status_ = RETRY;
                    Retry();
                }
                break;
            case types::SYNC:
                if (server_->processSyncCall(&syncReq, &syncRes)) {
                    syncResponder.Finish(syncRes, Status::OK, this);
                    status_ = FINISH;
                } else {
                    status_ = RETRY;
                    Retry();
                }
                break;
        }
    } else {
        CHECK_EQ(status_, FINISH);
        // Once in the FINISH state, deallocate ourselves (CallData).
        delete this;
    }
}

// Helper function to generate random backoff in a given range.
int CallData::randomBackoff(int min_ms, int max_ms) {
    static std::random_device rd;   // Random device for seed.
    static std::mt19937 gen(rd());  // Mersenne Twister generator.
    std::uniform_int_distribution<> dist(min_ms, max_ms);
    return dist(gen);
}

void CallData::Retry() {  
    int delayMs = 10;
    alarm_ = std::make_unique<grpc::Alarm>();
    alarm_->Set(cq_, gpr_time_from_millis(delayMs, GPR_TIMESPAN), this);
    // alarm_->Set(cq_, gpr_time_0(GPR_CLOCK_REALTIME), this);
}