## Abstract
The objective of the second project is to implement a variant of the PBFT consensus protocol. PBFT can achieve consensus with at least 3f + 1 nodes where f is the maximum number of concurrent faulty (Byzantine) nodes. You are supposed to implement the normal case operation and the view-change routine of a BFT protocol called linear-PBFT. In contrast to PBFT, the normal case operation of linear-PBFT achieves consensus with linear communication complexity. The view-change routine, however, is the same as PBFT. Similar to the previous project, we use a basic distributed banking application. However, all client requests are processed by all nodes.


## Description
A simple banking application is deployed where clients send their transfer transactions in the form of (S, R, amt) to the servers where S is the sender, R is the receiver, and amt is the amount of money to transfer. Since no single server is trusted, in contrast to project 1, all transactions need to be maintained by all servers, i.e., consensus is needed for every single transaction. Both clients and servers can be faulty, so all messages need to be checked to ensure validity. For simplicity, we do not implement any access control or blocking mechanism to restrict clients’ malicious behaviour. However, all messages need to be signed. _Figure 1_ shows the overview of the system. The outgoing line form the client to the primary server is the request message and all incoming lines to the client show the reply messages.

<p align="center">
      <img src="https://github.com/user-attachments/assets/e48c4348-2e55-4bf2-9c0b-9d9dc9cebdcd">
      <br>
      <em>Figure 1: System overview</em>
</p>


## PBFT Protocol
PBFT, as discussed in the class, is a leader-based consensus protocol. To perform this project, you need to be fully aware of all details of the PBFT protocol [1]. Please carefully read the paper before starting the project. Here, we briefly give a high-level overview of the normal operation and view-change routine of the protocol.
PBFT assumes the partial synchrony model. In PBFT, while there is no upper bound on the number of faulty clients, the maximum number of concurrent malicious replicas is assumed to be f. Replicas are connected via an unreliable network that might drop, corrupt, or delay messages and the network uses point-to-point bi-directional communication channels to connect replicas. In PBFT, a strong adversary can coordinate malicious replicas and delay communication. However, the adversary cannot subvert cryptographic assumptions.

PBFT, as shown in _Figure 2_, operates in a succession of configurations called views. Each view is coordinated by a stable leader (primary). In PBFT, the number of replicas, n, is assumed to be 3f + 1 and the ordering stage consists of pre-prepare, prepare, and commit phases. The pre-prepare phase assigns an order to the request, the prepare phase guarantees the uniqueness of the assigned order and the commit phase guarantees that the next leader can safely assign the order.

<p align="center">
      <img src="https://github.com/user-attachments/assets/4b32b00d-f861-4e90-a447-f6a2617f46c0">
      <br>
      <em>Figure 2: Different stages of PBFT protocol</em>
</p>

During a normal case execution of PBFT, clients send their signed request messages to the leader. In the pre-prepare phase, the leader assigns a sequence number to the request to determine the execution order of the request and multicasts a pre-prepare message to all backups. Upon receiving a valid pre-prepare message from the leader, each backup replica multicasts a prepare message to all replicas and waits for prepare messages from 2f different replicas (including the replica itself) that 
match the pre-prepare message. The goal of the prepare phase is to guarantee safety within the view, i.e., 2f replicas received matching pre-prepare messages from the leader replica and agree with the order of the request.

Each replica then multicasts a commit message to all replicas. Once a replica receives 2f + 1 valid commit messages from different replicas, including itself, that match the pre-prepare message, it commits the request. The goal of the commit phase is to ensure safety across views, i.e., the request has been replicated on a majority of non-faulty replicas and can be recovered after (leader) failures. The second and third phases of PBFT follow the clique topology, i.e., have O(n2) message complexity. If the replica has executed all requests with lower sequence numbers, it executes the request and sends a reply to the client. The client waits for f+1 matching results from different replicas.

In the view change stage, upon detecting the failure of the leader of view v (being suspicious that the leader is faulty) using timeouts, backups exchange view-change messages including their latest stable checkpoints and the later requests that the replicas have prepared. After receiving 2f + 1 view-change messages, the designated stable leader of view v + 1 (the replica with ID = v + 1 mod n) proposes a new view message, including a pre-prepare message for each request that should be processed in the new view.

In PBFT, replicas periodically generate checkpoint messages and send them to all replicas. If a replica receives 2f + 1 matching checkpoint messages, the checkpoint is stable. PBFT uses either signatures or MACs for authentication. Using MACs, replicas need to send view-change-ack messages to the leader after receiving view-change messages. Since new view messages are not signed, these view-change-ack messages enable replicas to verify the authenticity of new view messages.


## Linear PBFT protocol
While PBFT is the gold standard for BFT protocol design, it suffers from its quadratic communication complexity where in the prepare and commit phases, all nodes need to communicate with each other. This causes a significant overhead, especially in cases where the protocol is deployed on a large network (i.e., a large number of nodes). One way to reduce this high communication complexity is to linearize the protocol by splitting each all-to-all communication phase into two linear phases: one phase from all replicas to a collector (typically the leader) and one phase from the collector to all replicas. The output protocol requires signatures for authentication. The collector collects a quorum of n − f signatures from replicas and broadcasts its message including the signatures, as a certificate of having received the required signatures. _Figure 3_ presents different stages of the linear PBFT protocol.

<p align="center">
      <img src="https://github.com/user-attachments/assets/1aaf2c8d-fabc-4d26-8245-2050bddba19b">
      <br>
      <em>Figure 3: Different stages of linear PBFT protocol</em>
</p>


## Implementation Details
- The implementation supports 7 servers (3f + 1 servers where f = 2) where clients communicate with the server they assumed to be the leader. As explained in PBFT, if the node that receives a request is not the leader, the node simply transmits the request to the leader. The clients are able to resend the requests to the system (all nodes) if they have not received the reply on time.
- The implementation supports 10 clients. Each client sends its requests to the primary server. Each request is a transfer (S,R,amt) from account S to R.
- The database is represented as a key-value store that keeps the record of all clients.
- The program reads a given input file. This file consists of a set of triplets (S,R,amt). It is assumed that all clients start with 10 units. Then the clients initiate transactions, e.g., (A,B,4), (D,A,2), (C,B,1), ...

The following optimizations have also been implemented:
- **Checkpointing mechanism:** The checkpointing mechanism of PBFT is used to first, garbage-collect data of completed consensus instances to save space and second, restore in-dark replicas (due to network unreliability or leader maliciousness) to ensure all non- faulty replicas are up-to-date. Checkpointing is initiated after a fixed window (every 50 transactions processed) in a decentralized manner without relying on a leader.
- **Optimistic phase reduction:** Given a linear BFT protocol, you can optimistically eliminates two linear phases (i.e., the equivalence of a single quadratic prepare pahse) assuming all replicas are non-faulty, e.g., SBFT. The leader (collector) waits for signed messages from all 3f + 1 replicas in the second phase of ordering, combines signatures and sends a signed message to all replicas. Upon receiving the signed mes- sage from the leader, each replica ensures that all non-faulty replicas have received the request and agreed with the order. As a result, the third phase of communication can be omitted and replicas can directly commit the request. If the leader has not received 3f + 1 messages after a predefined time, the protocol fallbacks to its slow path and runs the third phase of ordering.


## Setup instructions

### Install gRPC

Instructions to install gRPC on MacOS are given below:

For installation instructions on Linux or Windows, please follow the steps here: https://grpc.io/docs/languages/cpp/quickstart/

1. Choose a directory to hold locally installed packages
```
$ export MY_INSTALL_DIR=$HOME/.local
```

2. Ensure that the directory exists
```
$ mkdir -p $MY_INSTALL_DIR
```

3. Add the local `bin` folder to path variable
```
$ export PATH="$MY_INSTALL_DIR/bin:$PATH"
```

4. Install cmake
```
$ brew install cmake
```

5. Install other required tools
```
$ brew install autoconf automake libtool pkg-config
```

6. Clone the gRPC repo
```
$ git clone --recurse-submodules -b v1.66.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc
```

7. Build and install gRPC and Protocol Buffers
```
$ cd grpc
$ mkdir -p cmake/build
$ pushd cmake/build
$ cmake -DgRPC_INSTALL=ON \
      -DgRPC_BUILD_TESTS=OFF \
      -DCMAKE_INSTALL_PREFIX=$MY_INSTALL_DIR \
      ../..
$ make -j 4
$ make install
$ popd
```

### Clone the project repository

1. Clone the project repository which has the source code for running paxos.
```
$ git clone git@github.com:avjotsingh/pbft-avjotsingh.git
```


### Build the project

1. Build the project. This should not take too long
```
$ cd pbft-avjotsingh
$ mkdir build
$ cd ./build
$ cmake -DCMAKE_PREFIX_PATH=$MY_INSTALL_DIR ..
$ cmake --build .
```

2. Run the project
The driver program needs a CSV. An example CSV file is available under the test directory.
```
$ cd build
$ ./driver <csv_filepath>
```
