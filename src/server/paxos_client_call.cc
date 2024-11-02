#include "absl/log/check.h"
#include "paxos_client_call.h"

PaxosClientCall::PaxosClientCall(PaxosServer* server, CompletionQueue* cq, types::RequestTypes callType) {
    server_ = server;
    cq_ = cq;
    callType_ = callType;
}

void PaxosClientCall::HandleRPCResponse() {
    switch (callType_) {
        case types::PREPARE:
            server_->handlePrepareReply(prepareReply);
            break;
        case types::ACCEPT:
            server_->handleAcceptReply(acceptReply);
            break;
        case types::COMMIT:
            break;
        case types::SYNC:
            server_->handleSyncReply(syncReply);
            break;
        default:
            break;
    }

    delete this;
}

void PaxosClientCall::sendPrepare(PrepareReq& request, std::unique_ptr<Paxos::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline) {    
    context.set_deadline(deadline);
    prepareResponseReader = stub_->PrepareAsyncPrepare(&context, request, cq_);
    prepareResponseReader->StartCall();
    prepareResponseReader->Finish(&prepareReply, &status, (void*)this);
}

void PaxosClientCall::sendAccept(AcceptReq& request, std::unique_ptr<Paxos::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline) {    
    context.set_deadline(deadline);
    acceptResponseReader = stub_->PrepareAsyncAccept(&context, request, cq_);
    acceptResponseReader->StartCall();
    acceptResponseReader->Finish(&acceptReply, &status, (void*)this);
}

void PaxosClientCall::sendCommit(CommitReq& request, std::unique_ptr<Paxos::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline) {    
    context.set_deadline(deadline);
    commitResponseReader = stub_->PrepareAsyncCommit(&context, request, cq_);
    commitResponseReader->StartCall();
    commitResponseReader->Finish(&commitReply, &status, (void*)this);
}

void PaxosClientCall::sendSync(SyncReq& request, std::unique_ptr<Paxos::Stub>& stub_, std::chrono::time_point<std::chrono::system_clock> deadline) {    
    context.set_deadline(deadline);
    syncResponseReader = stub_->PrepareAsyncSync(&context, request, cq_);
    syncResponseReader->StartCall();
    syncResponseReader->Finish(&syncReply, &status, (void*)this);
}