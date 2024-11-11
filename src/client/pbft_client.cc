#include <math.h>

#include "pbft_client.h"
#include "../utils/utils.h"
#include "../constants.h"
#include "../crypto/crypto.h"

using grpc::ServerBuilder;

using pbft::Message;
using pbft::TransactionData;
using pbft::Signature;
using pbft::Response;
using pbft::Transactions;
using pbft::Transaction;


// TODO implement get performance

PbftClientImpl::PbftClientImpl(int id, std::string name) {
    clientId = id;
    clientName = name;

    f = 2;
    currentView = 0;
    clusterSize = 7;
    
    rpcTimeoutSeconds = 2;
    transferTimeoutSeconds = 4;

    transactionsIssued = 0;
    transactionsProcessed = 0;
    totalTimeTaken = 0;
}

PbftClientImpl::~PbftClientImpl() {
    server_->Shutdown();
    requestCQ->Shutdown();
    responseCQ->Shutdown();
}

void PbftClientImpl::run(std::string targetAddress) {
  ServerBuilder builder;
  builder.AddListeningPort(targetAddress, grpc::InsecureServerCredentials());
  builder.RegisterService(&service_);
  requestCQ = builder.AddCompletionQueue();
  responseCQ = std::make_unique<CompletionQueue>();
  server_ = builder.BuildAndStart();

  for (auto it = Constants::serverAddresses.begin(); it != Constants::serverAddresses.end(); it++) {
    std::string server = it->first;
    std::string targetAddress = it->second;
    stubs_.push_back(PbftServer::NewStub(grpc::CreateChannel(targetAddress, grpc::InsecureChannelCredentials())));    
  }

  std::cout << "Client running on " << targetAddress << std::endl;
  HandleRPCs();
}

void PbftClientImpl::HandleRPCs() {
    new RequestData(&service_, this, requestCQ.get(), types::NOTIFY);
    new RequestData(&service_, this, requestCQ.get(), types::PROCESS);

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

        // Clean up the queue of transfer timers
        if (!transferTimers.empty()) {
            std::future<void>& f = transferTimers.front();
            std::future_status status = f.wait_for(std::chrono::milliseconds(0));
            if (status == std::future_status::ready) transferTimers.pop();
        }
    }
}

int PbftClientImpl::getLeaderId() {
    return currentView % clusterSize;
}

void PbftClientImpl::doTransfers() {
    
    if (transfers.empty()) return;

    auto tinfo = transfers.front();
    std::string receiver = tinfo.t.receiver;
    int amount = tinfo.t.amount;

    int leaderId = getLeaderId();

    Message request;
    TransactionData *tdata = request.mutable_data();
    
    auto epoch = std::chrono::system_clock::now().time_since_epoch();
    long seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
    
    // set transaction data
    tdata->set_sender(clientName);
    tdata->set_receiver(receiver);
    tdata->set_amount(amount);
    tdata->set_timestamp(seconds);

    // sign the data
    Signature *sig = request.mutable_signature();
    std::string dataString;
    tdata->SerializeToString(&dataString);
    std::string pemPath = Utils::clientPrECDSAKeyPath(clientId);
    sig->set_sig(crypto::signECDSA(dataString, pemPath));
    sig->set_server_id(clientId);

    // Start a timer for the transfer request. If f + 1 matching replies are not received by this time, then broadcast
    setTransferTimer(tinfo, transferTimeoutSeconds);

    // Send the request to leader for processing
    std::chrono::time_point rpcDeadline = std::chrono::system_clock::now() + std::chrono::seconds(rpcTimeoutSeconds);
    ResponseData *call = new ResponseData(this, responseCQ.get());
    call->sendMessage(request, stubs_[leaderId], rpcDeadline);
}

void PbftClientImpl::transferBroadcast() {

    TransferInfo& info = transfers.front();
    Message request;
    TransactionData *tdata = request.mutable_data();
    
    // set transaction data
    tdata->set_sender(info.t.sender);
    tdata->set_receiver(info.t.receiver);
    tdata->set_amount(info.t.amount);
    tdata->set_timestamp(info.timestamp);

    // sign the data
    Signature *sig = request.mutable_signature();
    std::string dataString;
    tdata->SerializeToString(&dataString);
    std::string pemPath = Utils::clientPrECDSAKeyPath(clientId);
    sig->set_sig(crypto::signECDSA(dataString, pemPath));
    sig->set_server_id(clientId);

    // Broadcast the request to all replicas
    std::chrono::time_point rpcDeadline = std::chrono::system_clock::now() + std::chrono::seconds(rpcTimeoutSeconds);
    for (auto& stub: stubs_) {
        ResponseData *call = new ResponseData(this, responseCQ.get());
        call->sendMessage(request, stub, rpcDeadline);
    }

    // Start a new timer for the transfer request.
    setTransferTimer(info, transferTimeoutSeconds);

}

void PbftClientImpl::processNotify(Response& request) {
    std::string dataString;
    request.data().SerializeToString(&dataString);

    int replicaId = request.signature().server_id();
    std::string pemPath = Utils::serverPbECDSAKeyPath(replicaId);

    // verify replica signature
    bool validSignature = crypto::verifyECDSA(dataString, request.signature().sig(), pemPath);
    if (!validSignature) return;

    std::string sender = request.data().tdata().sender();
    std::string receiver = request.data().tdata().receiver();
    int amount = request.data().tdata().amount();
    long timestamp = request.data().tdata().timestamp();
    int viewNum = request.data().view_num();
    
    // Update the client's current view
    currentView = std::max(currentView, viewNum);

    // Update the number of matching replies
    TransferInfo info = transfers.front();
    if (info.timestamp == timestamp) {
        request.data().ack() ? info.successes.insert(replicaId) : info.failures.insert(replicaId);
        if (info.successes.size() >= f + 1 || info.failures.size() >= f + 1) {
            // Got a valid response
            transfers.pop();
            ++transactionsProcessed;
            doTransfers();
        }
        if (info.failures.size() >= f + 1) {
            std::cout << "Failed to process transaction (" << sender << ", " << receiver << ", " << amount << ")" << std::endl;
        }
    }
}

void PbftClientImpl::processProcess(Transactions& transactions) {
    for (int i = 0; i < transactions.transactions_size(); i++) {
        const Transaction& t = transactions.transactions(i);
        if (t.sender() != clientName) continue;

        TransferInfo tinfo;
        tinfo.t.id = i;
        tinfo.t.sender = t.sender();
        tinfo.t.receiver = t.receiver();
        tinfo.t.amount = t.amount();

        tinfo.successes = std::set<int>();
        tinfo.failures = std::set<int>();
        
        transfers.push(tinfo);
    }

    doTransfers();
}

void PbftClientImpl::setTransferTimer(TransferInfo& info, int timeoutSeconds) {
    std::future<void> f = std::async(std::launch::async, [this, info, timeoutSeconds] () {
        std::this_thread::sleep_for(std::chrono::seconds(timeoutSeconds));

        TransferInfo& front = transfers.front();
        if (front.t.toString() == info.t.toString() && front.timestamp == info.timestamp) {
            transferBroadcast();
        }
    });

    // Store the variable to prevent it from going out-of-scope which causes blocking
    transferTimers.push(std::move(f));
}

void RunServer(int clientId, std::string clientName, std::string targetAddress) {
  PbftClientImpl client(clientId, clientName);
  client.run(targetAddress);
}

int main(int argc, char** argv) {
//   absl::InitializeLog();
  if (argc < 4) {
    std::cerr << "Usage: " << argv[0] << " <client_id> <client_name> <target_address>" << std::endl;
    return 1;
  }

  try {
    RunServer(std::stoi(argv[1]), argv[2], argv[3]);
  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << std::endl;
  }


  return 0;
}