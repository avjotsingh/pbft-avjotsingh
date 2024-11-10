#pragma once
#include "transaction.h"
#include <vector>
#include <string>

namespace types {
    struct CSVLine {
        int setNo;
        Transaction transaction;
        std::vector<std::string> aliveServers;   
        std::vector<std::string> byzantineServers;    
    };
}
