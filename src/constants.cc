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
    std::map<std::string, std::map<std::string, std::string>> keys = {
        {"S1", 
            {
                {"S2", ""},
                {"S3", ""},
                {"S4", ""},
                {"S5", ""},
                {"S6", ""},
                {"S7", ""}
            }
        },
        {"S2", 
            {
                {"S1", ""},
                {"S3", ""},
                {"S4", ""},
                {"S5", ""},
                {"S6", ""},
                {"S7", ""}
            }
        },
        {"S3", 
            {
                {"S1", ""},
                {"S2", ""},
                {"S4", ""},
                {"S5", ""},
                {"S6", ""},
                {"S7", ""}
            }
        },
        {"S4", 
            {
                {"S1", ""},
                {"S2", ""},
                {"S3", ""},
                {"S5", ""},
                {"S6", ""},
                {"S7", ""}
            }
        },
        {"S5", 
            {
                {"S1", ""},
                {"S2", ""},
                {"S3", ""},
                {"S4", ""},
                {"S6", ""},
                {"S7", ""}
            }
        },
        {"S6", 
            {
                {"S1", ""},
                {"S2", ""},
                {"S3", ""},
                {"S4", ""},
                {"S5", ""},
                {"S7", ""}
            }
        },
        {"S7", 
            {
                {"S1", ""},
                {"S2", ""},
                {"S3", ""},
                {"S4", ""},
                {"S5", ""},
                {"S6", ""}
            }
        },
    };
}