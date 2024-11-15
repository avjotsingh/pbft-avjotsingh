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
    // Load private key from PEM file
    FILE* fp = fopen(pemPath.c_str(), "r");
    if (!fp) {
        std::cerr << "Error opening PEM file\n";
        return "";
    }

    EVP_PKEY* pkey = PEM_read_PrivateKey(fp, NULL, NULL, NULL);
    fclose(fp);

    if (!pkey) {
        std::cerr << "Error reading private key\n";
        return "";
    }

    // Create ECDSA signing context
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        std::cerr << "Error creating signing context\n";
        EVP_PKEY_free(pkey);
        return "";
    }

    if (EVP_DigestSignInit(ctx, NULL, EVP_sha256(), NULL, pkey) != 1) {
        std::cerr << "Error initializing signing context\n";
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return "";
    }

    // Sign the data
    if (EVP_DigestSignUpdate(ctx, data.c_str(), data.size()) != 1) {
        std::cerr << "Error updating signing context\n";
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return "";
    }

    size_t siglen;
    if (EVP_DigestSignFinal(ctx, NULL, &siglen) != 1) {
        std::cerr << "Error getting signature length\n";
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return "";
    }

    unsigned char* signature = new unsigned char[siglen];
    if (EVP_DigestSignFinal(ctx, signature, &siglen) != 1) {
        std::cerr << "Error signing data\n";
        delete[] signature;
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return "";
    }

    // Convert the signature to a hex string
    std::stringstream hexStream;
    for (unsigned int i = 0; i < siglen; i++) {
        hexStream << std::setw(2) << std::setfill('0') << std::hex << (int)signature[i];
    }

    // Cleanup
    delete[] signature;
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);

    return hexStream.str();
}

bool crypto::verifyECDSA(const std::string& data, std::string signatureHex, const std::string& pemPath) {
    // Load public key from PEM file
    FILE* fp = fopen(pemPath.c_str(), "r");
    if (!fp) {
        std::cerr << "Error opening PEM file\n";
        return false;
    }

    EVP_PKEY* pkey = PEM_read_PUBKEY(fp, NULL, NULL, NULL);
    fclose(fp);

    if (!pkey) {
        std::cerr << "Error reading public key\n";
        return false;
    }

    // Create ECDSA verification context
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        std::cerr << "Error creating verification context\n";
        EVP_PKEY_free(pkey);
        return false;
    }

    if (EVP_DigestVerifyInit(ctx, NULL, EVP_sha256(), NULL, pkey) != 1) {
        std::cerr << "Error initializing verification context\n";
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return false;
    }

    // Update the verification context with data
    if (EVP_DigestVerifyUpdate(ctx, data.c_str(), data.size()) != 1) {
        std::cerr << "Error updating verification context\n";
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return false;
    }

    // Convert hex string signature to raw bytes
    std::vector<unsigned char> signatureBytes;
    for (size_t i = 0; i < signatureHex.size(); i += 2) {
        std::string byteString = signatureHex.substr(i, 2);
        unsigned char byte = (unsigned char)strtol(byteString.c_str(), NULL, 16);
        signatureBytes.push_back(byte);
    }

    // Verify the signature
    bool isValid = EVP_DigestVerifyFinal(ctx, signatureBytes.data(), signatureBytes.size()) == 1;

    // Cleanup
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);

    return isValid;
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
    std::string mac = signMAC(data, binPath);
    return mac == signatureHex;
}

