#include <openssl/pem.h>
#include <openssl/ecdsa.h>
#include <openssl/sha.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <cstring>
#include <vector>
#include <fstream>

#include "crypto.h"

std::string crypto::sha256Digest(std::string& data) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLength;

    // Create and initialize the context
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        return "";
    }

    // Initialize the SHA-256 hash context
    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        return "";
    }

    // Update the context with the data
    if (EVP_DigestUpdate(mdctx, data.c_str(), data.size()) != 1) {
        EVP_MD_CTX_free(mdctx);
        return "";
    }

    // Finalize the hash computation
    if (EVP_DigestFinal_ex(mdctx, hash, &hashLength) != 1) {
        EVP_MD_CTX_free(mdctx);
        return "";
    }

    // Clean up
    EVP_MD_CTX_free(mdctx);

    // Convert the hash to a hex string
    std::stringstream ss;
    for (unsigned int i = 0; i < hashLength; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

std::string crypto::signECDSA(const std::string& data, const std::string& pemPath) {

    printf("adding empty ecdsa signature\n");
    return "somerandomsignature";
    
    // Load the private key from the PEM file
    FILE* pemFile = fopen(pemPath.c_str(), "r");
    if (!pemFile) {
        return "";
    }

    EVP_PKEY* privateKey = PEM_read_PrivateKey(pemFile, nullptr, nullptr, nullptr);
    fclose(pemFile);
    if (!privateKey) {
        return "";
    }

    // Create a context for signing
    EVP_MD_CTX* mdCtx = EVP_MD_CTX_new();
    if (!mdCtx) {
        EVP_PKEY_free(privateKey);
        return "";
    }

    // Initialize signing
    if (EVP_SignInit(mdCtx, EVP_sha256()) <= 0) {
        EVP_MD_CTX_free(mdCtx);
        EVP_PKEY_free(privateKey);
        return "";
    }

    // Update the context with data
    if (EVP_SignUpdate(mdCtx, data.c_str(), data.size()) <= 0) {
        EVP_MD_CTX_free(mdCtx);
        EVP_PKEY_free(privateKey);
        return "";
    }

    // Finalize the signing
    std::vector<unsigned char> signature(EVP_PKEY_size(privateKey));
    unsigned int signatureLen;
    signature.resize(EVP_PKEY_size(privateKey));
    if (EVP_SignFinal(mdCtx, signature.data(), &signatureLen, privateKey) <= 0) {
        EVP_MD_CTX_free(mdCtx);
        EVP_PKEY_free(privateKey);
        return "";
    }

    // Clean up
    EVP_MD_CTX_free(mdCtx);
    EVP_PKEY_free(privateKey);

    // Convert the signature to a hex string
    std::stringstream hexStream;
    for (unsigned int i = 0; i < signatureLen; ++i) {
        hexStream << std::setw(2) << std::setfill('0') << std::hex << (int)signature[i];
    }

    // Return the hex string representation of the signature
    return hexStream.str();
}

bool crypto::verifyECDSA(const std::string& data, std::string signatureHex, const std::string& pemPath) {
    return true;
    
    // Convert the signature from hex string to bytes

    std::vector<unsigned char> signature;
    for (size_t i = 0; i < signatureHex.length(); i += 2) {
        unsigned char byte = static_cast<unsigned char>(std::stoi(signatureHex.substr(i, 2), nullptr, 16));
        signature.push_back(byte);
    }
    size_t signatureLen = signature.size();
    
    // Load the public key from the PEM file
    FILE* pemFile = fopen(pemPath.c_str(), "r");
    if (!pemFile) {
        return false;
    }

    EVP_PKEY* publicKey = PEM_read_PUBKEY(pemFile, nullptr, nullptr, nullptr);
    fclose(pemFile);
    if (!publicKey) {
        return false;
    }

    // Create a context for verification
    EVP_MD_CTX* mdCtx = EVP_MD_CTX_new();
    if (!mdCtx) {
        EVP_PKEY_free(publicKey);
        return false;
    }

    // Initialize verification
    if (EVP_VerifyInit(mdCtx, EVP_sha256()) <= 0) {
        EVP_MD_CTX_free(mdCtx);
        EVP_PKEY_free(publicKey);
        return false;
    }

    // Update the context with data
    if (EVP_VerifyUpdate(mdCtx, data.c_str(), data.size()) <= 0) {
        EVP_MD_CTX_free(mdCtx);
        EVP_PKEY_free(publicKey);
        return false;
    }

    // Finalize the verification
    int result = EVP_VerifyFinal(mdCtx, signature.data(), signatureLen, publicKey);

    // Clean up
    EVP_MD_CTX_free(mdCtx);
    EVP_PKEY_free(publicKey);

    return result == 1; // Returns 1 if the signature is valid
}

std::string crypto::signMAC(const std::string& data, const std::string& binPath) {
    std::ifstream file(binPath, std::ios::binary);
    if (!file) {
        return "";
    }

    std::string key((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    // Create a context for HMAC
    EVP_MAC* macAlgorithm = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
    if (!macAlgorithm) {
        return "";
    }

    EVP_MAC_CTX* macContext = EVP_MAC_CTX_new(macAlgorithm);
    if (!macContext) {
        EVP_MAC_free(macAlgorithm);
        return "";
    }

    // Set up the key and algorithm (HMAC with SHA-256)
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string("digest", const_cast<char*>("SHA256"), 0),
        OSSL_PARAM_END
    };
    if (EVP_MAC_init(macContext, reinterpret_cast<const unsigned char*>(key.c_str()), key.size(), params) != 1) {
        EVP_MAC_CTX_free(macContext);
        EVP_MAC_free(macAlgorithm);
        return "";
    }

    // Update MAC context with the data
    if (EVP_MAC_update(macContext, reinterpret_cast<const unsigned char*>(data.c_str()), data.size()) != 1) {
        EVP_MAC_CTX_free(macContext);
        EVP_MAC_free(macAlgorithm);
        return "";
    }

    // Finalize MAC and retrieve the signature
    size_t signatureLen;
    if (EVP_MAC_final(macContext, nullptr, &signatureLen, 0) != 1) {
        EVP_MAC_CTX_free(macContext);
        EVP_MAC_free(macAlgorithm);
        return "";
    }

    std::vector<unsigned char> signature(signatureLen);
    if (EVP_MAC_final(macContext, signature.data(), &signatureLen, signature.size()) != 1) {
        EVP_MAC_CTX_free(macContext);
        EVP_MAC_free(macAlgorithm);
        return "";
    }

    // Clean up
    EVP_MAC_CTX_free(macContext);
    EVP_MAC_free(macAlgorithm);


    // Convert the MAC signature to a hex string
    std::stringstream hexStream;
    for (unsigned char byte : signature) {
        hexStream << std::setw(2) << std::setfill('0') << std::hex << (int)byte;
    }

    // Return the hex string representation of the MAC signature
    return hexStream.str();
}


bool crypto::verifyMAC(std::string &data, std::string signatureHex, const std::string& binPath) {
    return true;
    std::string mac = signMAC(data, binPath);
    return mac == signatureHex;
}

