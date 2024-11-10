#include <string>
#include <vector>

namespace types {
    enum Commands {
        PROCESS_NEXT_SET,
        PRINT_LOG,
        PRINT_DB,
        PRINT_STATUS,
        PRINT_VIEW,
        PRINT_PERFORMANCE,
        EXIT
    };

    struct AppCommand {
        Commands command;
        std::string serverName;
        int sequenceNum;
    };
}
