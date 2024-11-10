#pragma once
#include <string>
#include <unistd.h>
#include <map>

class Utils {
public:
    static void initializeClients();
    static void killAllServers();
    static void killAllClients();
    static void startServer(std::string serverName, bool isByzantine);
    static void startClient(std::string clientName);
    static void killServer(std::string serverName);
    static void killClient(std::string clientName);

    static std::string clientPrECDSAKeyPath(int clientId);
    static std::string clientPbECDSAKeyPath(int clientId);
    static std::string serverPrECDSAKeyPath(int serverId);
    static std::string serverPbECDSAKeyPath(int serverId);
    static std::string macKeyPath(int serverId1, int serverId2);

private:
    static std::map<std::string, pid_t> serverPIDs;
    static std::map<std::string, pid_t> clientPIDs;
};