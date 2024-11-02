#include <string>
#include <vector>

namespace types {
    enum Commands {
        PROCESS_NEXT_SET,
        PRINT_BALANCE,
        PRINT_LOG,
        PRINT_DB,
        PRINT_PERFORMANCE,
        EXIT
    };

    struct AppCommand {
        Commands command;
        std::string serverName;
    };
}
