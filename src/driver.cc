#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <regex>
#include <vector>
#include <thread>

#include "app_client.h"
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
    bool firstSet = true;
    std::vector<types::PbftLogEntry> logs;
    std::vector<std::vector<std::string>> db;
    std::vector<std::string> status;
    std::string serverName;
    std::vector<types::ViewChangeInfo> viewChanges;
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
                        if (!firstSet) Utils::killAllServers();
                        for (std::string& s: set.aliveServers) {
                            Utils::startServer(s, false);
                        }

                        for (std::string&s : set.byzantineServers) {
                            Utils::startServer(s, true);
                        }

                        sleep(5);   // wait for servers to start before issuing transactions
                        client->ProcessTransactions(set.transactions);
                    }
                    break;

                case types::PRINT_LOG:
                    logs.clear();
                    client->GetLogs(c.serverName, logs);
                    std::cout << "Log on " << c.serverName << ": " << std::endl;
                    
                    std::cout << std::setw(10) << "index| ";
                    for (int i = 0; i < logs.size(); i++) {
                        std::cout << std::setw(10) << logs[i].t.id << "| ";
                    }
                    std::cout << std::endl;

                    std::cout << std::setw(10) << "transaction| ";
                    for (int i = 0; i < logs.size(); i++) {
                        std::cout << std::setw(10) << "(" << logs[i].t.sender << ", " << logs[i].t.receiver << ", " << logs[i].t.amount << "| ";
                    }
                    std::cout << std::endl;

                    std::cout << std::setw(10) << "prepares| ";
                    for (int i = 0; i < logs.size(); i++) {
                        std::cout << std::setw(10) << logs[i].matchingPrepares << "| ";
                    }
                    std::cout << std::endl;

                    std::cout << std::setw(10) << "commits| ";
                    for (int i = 0; i < logs.size(); i++) {
                        std::cout << std::setw(10) << logs[i].matchingCommits << "| ";
                    }
                    std::cout << std::endl;
                    break;

                case types::PRINT_DB:
                    db.clear();
                    client->GetDb(db, set.aliveServers);
                    std::cout << "DB:" << std::endl;

                    std::cout << std::setw(10) << "Server| ";
                    for (int i = 0; i < Constants::clientAddresses.size(); i++) {
                        std::cout << 'A' + i << "| "; 
                    }
                    std::cout << std::endl;

                    for (int i = 0; i < Constants::serverAddresses.size(); i++) {
                        std::cout << std::setw(10) << 'S' + i << "| ";
                        for (int j = 0; j < Constants::clientAddresses.size(); j++) {
                            std::cout << std::setw(10) << db[i][j] << "| ";
                        }
                    }
                    std::cout << std::endl;
                    break;

                case types::PRINT_STATUS:
                    status.clear();
                    client->GetStatus(c.sequenceNum, status);
                    std::cout << "Server |";
                    for (int i = 0; i < Constants::serverAddresses.size(); i++) {
                        std::cout << std::setw(10) << 'S' + i << "| ";
                    }
                    std::cout << std::endl;

                    for (int i = 0; i < Constants::serverAddresses.size(); i++) {
                        std::cout << std::setw(10) << status[i] << "| ";
                    }

                    std::cout << std::endl;
                    break;

                case types::PRINT_VIEW:
                    viewChanges.clear();
                    // pick a non-byzantine server
                    for (auto&s : set.aliveServers) {
                        if (std::find(set.byzantineServers.begin(), set.byzantineServers.end(), s) == set.byzantineServers.end()) {
                            serverName = s;
                            break;
                        }
                    }

                    client->GetViewChanges(serverName, viewChanges);
                    std::cout << std::setw(10) << "View num|" << std::setw(10) << "Initiator|" << std::setw(10) << "Last st. CP";
                    std::cout << std::endl;
                    for (auto& v: viewChanges) {
                        std::cout << std::setw(10) << v.viewNum << "|" << std::setw(10) << v.initiator << "|" << std::setw(10) << v.stableCheckpoint;
                        std::cout << std::endl;
                    }

                    std::cout << std::endl;
                    break;

                case types::PRINT_PERFORMANCE:
                    performance = client->GetPerformance();
                    std::cout << performance << " transactions/second" << std::endl;
                    break;

                case types::EXIT:
                    std::cout << "Exiting..." << std::endl;
                    Utils::killAllServers();
                    Utils::killAllClients();
                    exit = true;
                    break;

                default:
                    std::runtime_error("Unknown command type: " + std::to_string(c.command));
                    break;
            }
        
            if (firstSet) firstSet = false;

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
    try {
        Utils::initializeClients();
        AppClient* client = new AppClient(); 
        CSVReader* reader = new CSVReader(filename);
        mainloop(reader, client);
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        exit(1);
    } 
    
    return 0;
}