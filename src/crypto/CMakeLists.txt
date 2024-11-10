# Gather all .cpp files in the crypto directory
file(GLOB_RECURSE CRYPTO_SOURCES "*.cc")

# Find OpenSSL
find_package(OpenSSL 3 REQUIRED)

# Create a library from the source files
add_library(crypto ${CRYPTO_SOURCES})

# Link the OpenSSL libraries to the crypto library
target_link_libraries(crypto OpenSSL::SSL OpenSSL::Crypto)

# Specify the include directories for this library
target_include_directories(crypto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/../types")
