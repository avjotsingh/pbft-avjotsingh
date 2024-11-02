#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <regex>
#include <vector>
#include <thread>

#include "client/app_client.h"
#include "utils/commands_parser.h"
#include "utils/csv_reader.h"
#include "utils/utils.h"
#include "constants.h"

void mainloop(CSVReader* reader, AppClient* client) {
    CommandsParser parser;
    bool exit = false;
    std::string command;
    types::AppCommand c;
    types::TransactionSet set;
    int balance;
    std::vector<types::Transaction> logs;
    std::vector<types::TransactionBlock> dbLogs;
    double performance;


    while(!exit) {
        parser.promptUserForCommand(command);
        try {
            c = parser.parseCommand(command);
        } catch (const std::invalid_argument& e) {
            std::cout << "Invalid command. Try again. " << std::endl;
            continue;
        }

        try {
            switch (c.command) {
                case types::PROCESS_NEXT_SET:
                    if (!reader->readNextSet(set)) {
                        std::cout << "No more transaction sets to read..." << std::endl;
                    } else {
                        for (std::string& s: Constants::serverNames) {
                            if (std::find(set.servers.begin(), set.servers.end(), s) != set.servers.end()) {
                                // ensure that the desired servers are running
                                Utils::startServer(s);
                            } else {
                                // ensure that the servers not in the transaction set are stopped
                                Utils::killServer(s);
                            }
                        }

                        sleep(5);   // wait for servers to start before issuing transactions
                        client->processTransactions(set.transactions);
                    }
                    break;

                case types::PRINT_BALANCE:
                    client->GetBalance(c.serverName, balance);
                    std::cout << "Balance on " << c.serverName << ": " << balance << std::endl;
                    break;

                case types::PRINT_LOG:
                    logs.clear();
                    client->GetLogs(c.serverName, logs);
                    std::cout << "Local logs on " << c.serverName << ": " << std::endl;
                    for (types::Transaction& t: logs) {
                        std::cout << "(" << t.id << ", " << t.sender << ", " << t.receiver << ", " << t.amount << ")" << std::endl;
                    }
                    break;

                case types::PRINT_DB:
                    dbLogs.clear();
                    client->GetDBLogs(c.serverName, dbLogs);
                    std::cout << "DB logs on " << c.serverName << ": " << std::endl;
                    for (auto& block : dbLogs) {
                        std::cout << "> Block" << block.id << std::endl;
                        for (types::Transaction& t: block.block) {
                            std::cout << "(" << t.id << ", " << t.sender << ", " << t.receiver << ", " << t.amount << ")" << std::endl;
                        }
                    }
                    break;

                case types::PRINT_PERFORMANCE:
                    performance = client->GetPerformance();
                    std::cout << performance << " transactions/second" << std::endl;
                    break;

                case types::EXIT:
                    std::cout << "Exiting..." << std::endl;
                    Utils::killAllServers();
                    exit = true;
                    break;

                default:
                    std::runtime_error("Unknown command type: " + std::to_string(c.command));
                    break;
            }
        } catch (std::exception& e) {
            std::cerr << "Exception: " << e.what() << std::endl;
        }
    }
}


int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <csv_filepath>" << std::endl;
        exit(1);
    }

    std::string filename = argv[1];
    std::thread t;
    try {
        Utils::initializeServers();
        AppClient* client = new AppClient(); 
        CSVReader* reader = new CSVReader(filename);
        std::thread t(&AppClient::consumeTransferReplies, client);
        mainloop(reader, client);
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        t.join();
        exit(1);
    } 
    
    return 0;
}