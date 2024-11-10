#include "constants.h"

namespace Constants {
    std::vector<std::string> serverNames = { "S1", "S2", "S3", "S4", "S5", "S6", "S7" };
    std::map<std::string, std::string> serverAddresses = {
        { "S1", "localhost:50051" },
        { "S2", "localhost:50052" },
        { "S3", "localhost:50053" },
        { "S4", "localhost:50054" },
        { "S5", "localhost:50055" },
        { "S6", "localhost:50056" },
        { "S7", "localhost:50057" }
    };

    std::map<std::string, std::string> clientAddresses = {
        { "A", "localhost:60060" },
        { "B", "localhost:60061" },
        { "C", "localhost:60062" },
        { "D", "localhost:60063" },
        { "E", "localhost:60064" },
        { "F", "localhost:60065" },
        { "G", "localhost:60066" },
        { "H", "localhost:60067" },
        { "I", "localhost:60068" },
        { "J", "localhost:60069" },
    };
}