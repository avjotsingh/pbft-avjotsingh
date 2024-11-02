#include "commands_parser.h"
#include <iostream>
#include <regex>
#include <sstream>


void CommandsParser::promptUserForCommand(std::string& command) {
    std::cout << "Type one of the following commands to proceed:" << std::endl;
    std::cout << "1. processNextSet" << std::endl;
    std::cout << "2. printBalance <client_name>" << std::endl;
    std::cout << "3. printLog <server_name>" << std::endl;
    std::cout << "4. printDB <server_name>" << std::endl;
    std::cout << "5. printPerformance" << std::endl;
    std::cout << "6. exit" << std::endl;
    std::cout << "=========================================================================" << std::endl;


    std::getline(std::cin, command);
}

bool CommandsParser::isValidCommand(std::string& command) {
    // Define valid command patterns using regex
    std::regex processNextSetPattern(R"(processNextSet)");
    std::regex printBalancePattern(R"(printBalance S[1-9])");
    std::regex printLogPattern(R"(printLog S[1-9])");
    std::regex printDBPattern(R"(printDB S[1-9])");
    std::regex printPerformancePattern(R"(printPerformance)");
    std::regex exitPattern(R"(exit)");

    // Check if the command matches any of the valid patterns
    if (std::regex_match(command, processNextSetPattern) ||
        std::regex_match(command, printBalancePattern) ||
        std::regex_match(command, printLogPattern) ||
        std::regex_match(command, printDBPattern) ||
        std::regex_match(command, printPerformancePattern) ||
        std::regex_match(command, exitPattern)) {
        return true;  // Command is valid
    }

    return false;  // Command is invalid
}

std::vector<std::string> CommandsParser::tokenizeCommand(std::string& command) {
    std::istringstream stream(command);
    std::string token;
    std::vector<std::string> command_tokens;

    while (stream >> token) {
        command_tokens.push_back(token);
    }

    return command_tokens;
}

types::AppCommand CommandsParser::parseCommand(std::string& command) {
    if (!this->isValidCommand(command)) {
        throw std::invalid_argument("Invalid command: " + command);
    } else {
        std::vector<std::string> command_tokens = this->tokenizeCommand(command);
        types::AppCommand c = { types::PROCESS_NEXT_SET, "" };
        if (command_tokens.at(0) == "processNextSet") {
            c.command = types::PROCESS_NEXT_SET;
        } else if (command_tokens.at(0) == "printBalance") {
            c.command = types::PRINT_BALANCE;
            c.serverName = command_tokens.at(1);
        } else if (command_tokens.at(0) == "printLog") {
            c.command = types::PRINT_LOG;
            c.serverName = command_tokens.at(1);
        } else if (command_tokens.at(0) == "printDB") {
            c.command = types::PRINT_DB;
            c.serverName = command_tokens.at(1);
        } else if (command_tokens.at(0) == "printPerformance") {
            c.command = types::PRINT_PERFORMANCE;
        } else {
            c.command = types::EXIT;
        }

        return c;
    }
}
