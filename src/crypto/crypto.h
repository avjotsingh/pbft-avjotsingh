#pragma once
#include <string>
#include <vector>

class crypto {
public:
    static std::string sha256Digest(std::string& data);
    static std::string signECDSA(const std::string& data, const std::string& pemPath);
    static bool verifyECDSA(const std::string& data, std::string signatureHex, const std::string& pemPath);
    static std::string signMAC(const std::string& data, const std::string& binPath);
    static bool verifyMAC(std::string &data, std::string signatureHex, const std::string& binPath);
};