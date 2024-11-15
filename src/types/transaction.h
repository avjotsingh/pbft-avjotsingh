#pragma once
#include <string>
#include <vector>


namespace types {

    struct Transaction {        // An individual transaction
        int id;
        std::string sender;
        std::string receiver;
        int amount;

        std::string toString() const {
            return "{" + std::to_string(id) + "," + sender + "," + receiver + "," + std::to_string(amount) + "}";
        }
    };

    struct TransactionSet {     // A transaction set in input CSV file
        int setNo;
        std::vector<Transaction> transactions;
        std::vector<std::string> aliveServers;
        std::vector<std::string> byzantineServers;
    };
    
    enum MessageStatus { NO_STATUS, PRE_PREPARED, PREPARED, COMMITTED, EXECUTED };

    struct PbftLogEntry {
        types::Transaction t;
        int matchingPrepares;
        int matchingCommits;
        bool valid;
    };

    struct ServerInfo {
        int lastCommitted;
        int lastExecuted;
        int lastCheckpoint;
        int lastStableCheckpoint;
    };

    struct ViewChangeInfo {
        int viewNum;
        int stableCheckpoint;
        std::string initiator;
        // std::vector<int> preparedEntries;
    };
}
