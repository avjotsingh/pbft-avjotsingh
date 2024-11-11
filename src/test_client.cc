/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <iostream>
#include <memory>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include <grpcpp/grpcpp.h>
#include "pbft.grpc.pb.h"


using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using pbft::PbftClient;
using pbft::Transactions;
using pbft::Transaction;
using google::protobuf::Empty;


class GreeterClient {
 public:
  GreeterClient(std::shared_ptr<Channel> channel)
      : stub_(PbftClient::NewStub(channel)) {}

  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  void processTransactions() {
    // Data we are sending to the server.
    Transactions request;
    Transaction* t = request.add_transactions();
    t->set_sender("A");
    t->set_receiver("B");
    t->set_amount(1);
    
    // Container for the data we expect from the server.
    Empty reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->Process(&context, request, &reply);

    // Act upon its status.
    if (status.ok()) {
      std::cout << "successful" << std::endl;
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
    }
  }

 private:
  std::unique_ptr<PbftClient::Stub> stub_;
};

int main(int argc, char** argv) {
  std::string target_str = std::string(argv[1]);
  GreeterClient greeter(grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));

  greeter.processTransactions();

  return 0;
}