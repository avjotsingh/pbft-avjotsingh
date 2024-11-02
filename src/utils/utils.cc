#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include <sys/wait.h>
#include <csignal>
#include "utils.h"
#include "../constants.h"


std::map<std::string, int> Utils::serverPIDs = {};

void Utils::initializeServers() {
    for (std::string& s: Constants::serverNames) {
        Utils::startServer(s);
    }
}

void Utils::killAllServers() {
    for (std::string& s:  Constants::serverNames) {
        Utils::killServer(s);
    }
}

void Utils::startServer(std::string serverName) {
    auto it = Constants::serverAddresses.find(serverName);
    if (it == Constants::serverAddresses.end()) {
        throw std::invalid_argument("Invalid server name: " + serverName);
    } 
    
    if (Utils::serverPIDs.find(serverName) == Utils::serverPIDs.end()) {
        std::string targetAddress = it->second;
        pid_t pid = fork();
        if (pid < 0) {
            throw std::runtime_error("Failed to start server: " + serverName);
        } else if (pid > 0) {
            Utils::serverPIDs.insert({ serverName, pid });
        } else {
            // server process
            execl("./paxosserver", "paxosserver", serverName.c_str(), targetAddress.c_str(), nullptr);
            // if execl fails
            throw std::runtime_error("Failed to start server: " + serverName);
        }
    }
}

void Utils::killServer(std::string serverName) {
    auto it = Constants::serverAddresses.find(serverName);
    if (it == Constants::serverAddresses.end()) {
        throw std::invalid_argument("Invalid server name: " + serverName);
    }

    auto it2 = Utils::serverPIDs.find(serverName);
    if (it2 != Utils::serverPIDs.end()) {
        pid_t pid = it2->second;
        if (kill(pid, SIGKILL) == 0) {
            std::cout << "Killing server " << serverName << std::endl;
            Utils::serverPIDs.erase(serverName);
        } else {
            throw std::runtime_error("Failed to kill server: " + serverName);
        }
    }
}