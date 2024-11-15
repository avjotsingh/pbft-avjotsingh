#include <iostream>
#include "crypto/crypto.h"
#include "utils/utils.h"

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



std::string signECDSA(const std::string& data, const std::string& pemPath) {
    
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

    // // Print the signature in hex format
    // std::cout << "Signature: ";
    // for (size_t i = 0; i < siglen; ++i) {
    //     printf("%02x", signature[i]);
    // }
    // std::cout << std::endl;

    // Cleanup
    delete[] signature;
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);

    return hexStream.str();
}

bool verifyECDSA(const std::string& data, const std::string& signatureHex, const std::string& pemPath) {
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

    if (!isValid) {
        std::cerr << "Signature verification failed\n";
    }

    // Cleanup
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);

    return isValid;
}



int main(int argc, char** argv) {
    std::string hello = "hello world";
    std::string digest = signECDSA(hello, std::string(argv[1]));
    std::cout << digest << std::endl;

    bool valid = verifyECDSA(hello, digest, std::string(argv[2]));
    std::cout << (valid ? "valid signature" : "invalid signature") << std::endl;
    return 0;
}