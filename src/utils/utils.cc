#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include <sys/wait.h>
#include <csignal>
#include "utils.h"
#include "../constants.h"


std::map<std::string, int> Utils::serverPIDs = {};
std::map<std::string, int> Utils::clientPIDs = {};

void Utils::killAllServers() {
    for (std::string& s:  Constants::serverNames) {
        Utils::killServer(s);
    }
}

void Utils::initializeClients() {
    for (auto& pair: Constants::clientAddresses) {
        Utils::startClient(pair.first);
    }
}

void Utils::killAllClients() {
    for (auto& pair: Constants::clientAddresses) {
        Utils::killClient(pair.first);
    }
}

void Utils::startServer(std::string serverName, bool isByzantine) {
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
            FILE *new_stdout = freopen(std::string(serverName + ".txt").c_str(), "w", stdout);
            if (!new_stdout) {
                perror("Failed to redirect stdout");    
            }


            int serverId = serverName[1] - '1';
            execl("./pbftserver", "pbftserver", std::to_string(serverId).c_str(), serverName.c_str(), targetAddress.c_str(), isByzantine ? "true" : "false", nullptr);
            // if execl fails
            throw std::runtime_error("Failed to start server: " + serverName);
        }
    }
}

void Utils::startClient(std::string clientName) {
    auto it = Constants::clientAddresses.find(clientName);
    if (it == Constants::clientAddresses.end()) {
        throw std::invalid_argument("Invalid client name: " + clientName);
    } 
    
    std::string targetAddress = it->second;
    pid_t pid = fork();
    if (pid < 0) {
        throw std::runtime_error("Failed to start client: " + clientName);
    } else if (pid > 0) {
        Utils::clientPIDs.insert({ clientName, pid });
    } else {
        // client process
        FILE *new_stdout = freopen(std::string(clientName + ".txt").c_str(), "w", stdout);
        if (!new_stdout) {
            perror("Failed to redirect stdout");    
        }

        int clientId = clientName[0] - 'A';
        execl("./pbftclient", "pbftclient", std::to_string(clientId).c_str(), clientName.c_str(), targetAddress.c_str(), nullptr);
        // if execl fails
        throw std::runtime_error("Failed to start server: " + clientName);
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

void Utils::killClient(std::string clientName) {
    auto it = Utils::clientPIDs.find(clientName);
    pid_t pid = it->second;
    if (kill(pid, SIGKILL) == 0) {
        std::cout << "Killing client " << clientName << std::endl;
    } else {
        throw std::runtime_error("Failed to kill server: " + clientName);
    }
}

std::string Utils::clientPrECDSAKeyPath(int clientId) {
    return "keys/client/ECDSA/private/c" + std::to_string(clientId) + "_private.pem";
}

std::string Utils::clientPbECDSAKeyPath(int clientId) {
    return "keys/client/ECDSA/public/c" + std::to_string(clientId) + "_public.pem";
}

std::string Utils::serverPrECDSAKeyPath(int serverId) {
    return "keys/server/ECDSA/private/s" + std::to_string(serverId) + "_private.pem";
}

std::string Utils::serverPbECDSAKeyPath(int serverId) {
    return "keys/server/ECDSA/public/s" + std::to_string(serverId) + "_public.pem";
}

std::string Utils::macKeyPath(int serverId1, int serverId2) {
    if (serverId1 > serverId2) {
        int temp = serverId1;
        serverId1 = serverId2;
        serverId2 = temp;
    }

  return "keys/servers/MAC/s" + std::to_string(serverId1) + "_s" + std::to_string(serverId2) + ".bin";
}