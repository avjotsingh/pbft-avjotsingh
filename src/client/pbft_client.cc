#include <math.h>
#include <execinfo.h>

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


PbftClientImpl::PbftClientImpl(int id, std::string name) {
    clientId = id;
    clientName = name;

    f = 2;
    currentView = 1;
    clusterSize = 7;
    
    rpcTimeoutSeconds = 2;
    transferTimeoutSeconds = 60;

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
    new RequestData(&service_, this, requestCQ.get(), types::GET_PERFORMANCE);

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

        // printf("Transfer timers size %d\n", transferTimers.size());
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

    TransferInfo* tinfo = transfers.front();
    Message request;
    TransactionData *tdata = request.mutable_data();

    // set transaction data
    tdata->set_sender(tinfo->t.sender);
    tdata->set_receiver(tinfo->t.receiver);
    tdata->set_amount(tinfo->t.amount);
    tdata->set_timestamp(tinfo->timestamp);

    // sign the data
    Signature *sig = request.mutable_signature();
    std::string dataString;
    tdata->SerializeToString(&dataString);
    std::string pemPath = Utils::clientPrECDSAKeyPath(clientId);

    std::cout << "signing message. client id: " << clientId << std::endl;
    sig->set_sig(crypto::signECDSA(dataString, pemPath));
    sig->set_server_id(clientId);

    // Start a timer for the transfer request. If f + 1 matching replies are not received by this time, then broadcast
    setTransferTimer(tinfo, transferTimeoutSeconds);

    printf("=====Send transfer request %s %s %d\nDigest %s\n========", tinfo->t.sender.c_str(), tinfo->t.receiver.c_str(), tinfo->t.amount, crypto::sha256Digest(dataString).c_str());
    printf("Transfer req: %s\n", request.DebugString().c_str());


    // Send the request to leader for processing
    int leaderId = getLeaderId();
    std::chrono::time_point rpcDeadline = std::chrono::system_clock::now() + std::chrono::seconds(rpcTimeoutSeconds);
    ResponseData *call = new ResponseData(this, responseCQ.get());

    // for performance measurement
    tinfo->startTime = std::chrono::system_clock::now();
    call->sendMessage(request, stubs_[leaderId], rpcDeadline);
}

void PbftClientImpl::transferBroadcast() {

    TransferInfo* info = transfers.front();
    Message request;
    TransactionData *tdata = request.mutable_data();
    
    // set transaction data
    tdata->set_sender(info->t.sender);
    tdata->set_receiver(info->t.receiver);
    tdata->set_amount(info->t.amount);
    tdata->set_timestamp(info->timestamp);

    // sign the data
    Signature *sig = request.mutable_signature();
    std::string dataString;
    tdata->SerializeToString(&dataString);
    std::string pemPath = Utils::clientPrECDSAKeyPath(clientId);
    sig->set_sig(crypto::signECDSA(dataString, pemPath));
    sig->set_server_id(clientId);

    printf("=======Broadcasting %s %s %d\nDigest %s\n=========", info->t.sender.c_str(), info->t.receiver.c_str(), info->t.amount, crypto::sha256Digest(dataString).c_str());

    // Broadcast the request to all replicas
    std::chrono::time_point rpcDeadline = std::chrono::system_clock::now() + std::chrono::seconds(rpcTimeoutSeconds);
    for (auto& stub: stubs_) {
        printf("Sending broadcast\n");
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
    
    printf("Received response for %s %s %d\n", sender.c_str(), receiver.c_str(), amount);

    // Update the client's current view
    currentView = std::max(currentView, viewNum);

    // Update the number of matching replies
    if (!transfers.empty()) {
        TransferInfo* info = transfers.front();
        if (info->timestamp == timestamp) {
            printf("[notify] updating metadata for transfer. replicaId %d\n", replicaId);
            std::cout << request.DebugString() << std::endl;


            if (request.data().ack()) {
                info->successes.insert(replicaId);
            } else {
                info->failures.insert(replicaId);
            }

            printf("[notify] successes so far %ld, failures %ld\n", info->successes.size(), info->failures.size());
            
            
            
            if (info->successes.size() >= f + 1 || info->failures.size() >= f + 1) {
                // Got a valid response
                printf("Received enough responses. Transfer complete. %s %s %d\n", info->t.sender.c_str(), info->t.receiver.c_str(), info->t.amount);
                transfers.pop();
                ++transactionsProcessed;

                // Performance measurement
                std::chrono::system_clock::time_point endTime = std::chrono::system_clock::now();
                totalTimeTaken += std::chrono::duration_cast<std::chrono::milliseconds>(endTime - info->startTime).count();

                doTransfers();
            }
            if (info->failures.size() >= f + 1) {
                std::cout << "Failed to process transaction (" << sender << ", " << receiver << ", " << amount << ")" << std::endl;
            }
        }
    }
    
}

void PbftClientImpl::processProcess(Transactions& transactions) {
    for (int i = 0; i < transactions.transactions_size(); i++) {
        const Transaction& t = transactions.transactions(i);

        printf("client name %s\n", clientName.c_str());
        printf("transaction %s %s %d\n", t.sender().c_str(), t.receiver().c_str(), t.amount());
        
        if (t.sender() != clientName) continue;

        auto epoch = std::chrono::system_clock::now().time_since_epoch();
        long seconds = std::chrono::duration_cast<std::chrono::nanoseconds>(epoch).count();

        TransferInfo* tinfo = new TransferInfo();
        tinfo->t.id = i;
        tinfo->t.sender = t.sender();
        tinfo->t.receiver = t.receiver();
        tinfo->t.amount = t.amount();
        tinfo->timestamp = seconds;

        tinfo->successes = std::set<int>();
        tinfo->failures = std::set<int>();
        
        transfers.push(tinfo);
    }

    doTransfers();
}

void PbftClientImpl::processPerformance(GetPerformanceRes& reply) {
    double res = 1.0; // (1000 * transactionsProcessed) / totalTimeTaken;
    reply.set_performance(res);
}

void PbftClientImpl::setTransferTimer(TransferInfo* info, int timeoutSeconds) {
    std::future<void> f = std::async(std::launch::async, [this, info, timeoutSeconds] () {
        std::this_thread::sleep_for(std::chrono::seconds(timeoutSeconds));

        if (!transfers.empty()) {
            TransferInfo* front = transfers.front();
            if (front->t.toString() == info->t.toString() && front->timestamp == info->timestamp) {
                printf("Transfer timer expired. Broadcasting\n");
                transferBroadcast();
            } else {
                printf("Transfer timer expired. Not Broadcasting\n");
            }
        }
        
    });

    // Store the variable to prevent it from going out-of-scope which causes blocking
    transferTimers.push(std::move(f));
}

void RunServer(int clientId, std::string clientName, std::string targetAddress) {
  PbftClientImpl client(clientId, clientName);
  client.run(targetAddress);
}

void printStackTrace() {
    const int maxFrames = 10;
    void* addrlist[maxFrames];

    // Get void*'s for all entries on the stack
    int numFrames = backtrace(addrlist, maxFrames);

    // Print all the frames to stderr
    char** symbols = backtrace_symbols(addrlist, numFrames);
    if (symbols != nullptr) {
        for (int i = 0; i < numFrames; ++i) {
            std::cerr << symbols[i] << std::endl;
        }
        free(symbols);
    } else {
        std::cerr << "Failed to generate symbols for stack trace." << std::endl;
    }
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
    printStackTrace();
  }


  return 0;
}