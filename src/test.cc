#include <iostream>
#include "crypto/crypto.h"

int main() {
    std::string hello = "hello world";
    std::string digest = crypto::sha256Digest(hello);

    std::cout << digest << std::endl;
    return 0;
}