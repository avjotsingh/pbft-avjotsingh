cmake_minimum_required(VERSION 3.20)

project(pbft C CXX)
set(CMAKE_CXX_STANDARD 23)
set(OPENSSL_ROOT_DIR "/opt/homebrew/Cellar/openssl@3/")

include(common.cmake)

# Proto file
get_filename_component(pbft_proto "./src/proto/pbft.proto" ABSOLUTE)
get_filename_component(pbft_proto_path "${pbft_proto}" PATH)

# Generated sources
set(pbft_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/pbft.pb.cc")
set(pbft_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/pbft.pb.h")
set(pbft_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/pbft.grpc.pb.cc")
set(pbft_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/pbft.grpc.pb.h")
add_custom_command(
      OUTPUT "${pbft_proto_srcs}" "${pbft_proto_hdrs}" "${pbft_grpc_srcs}" "${pbft_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${pbft_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${pbft_proto}"
      DEPENDS "${pbft_proto}")

# Include generated *.pb.h files
include_directories(${CMAKE_CURRENT_BINARY_DIR} 
  ${CMAKE_CURRENT_SOURCE_DIR} 
  "${CMAKE_CURRENT_SOURCE_DIR}/crypto"
  "${CMAKE_CURRENT_SOURCE_DIR}/types"
  "${CMAKE_CURRENT_SOURCE_DIR}/utils"
  "${CMAKE_CURRENT_SOURCE_DIR}/client"
  "${CMAKE_CURRENT_SOURCE_DIR}/server")

# pbft_grpc_proto
add_library(pbft_grpc_proto
  ${pbft_grpc_srcs}
  ${pbft_grpc_hdrs}
  ${pbft_proto_srcs}
  ${pbft_proto_hdrs})
target_link_libraries(pbft_grpc_proto
  absl::check
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})


# Find OpenSSL
find_package(OpenSSL 3 REQUIRED)

# Add crypto subdirectory
add_subdirectory(src/crypto)

# Add utils subdirectory
add_subdirectory(src/utils)

# Build the client executable
file(GLOB_RECURSE CLIENT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/client/*.cc" "src/constants.cc")

add_executable(pbftclient ${CLIENT_SOURCES})
target_link_libraries(pbftclient
  pbft_grpc_proto
  crypto
  utils
  absl::check
  absl::flags
  absl::flags_parse
  absl::log
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})


# Build the server executable
file(GLOB_RECURSE SERVER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/server/*.cc" "src/constants.cc")

add_executable(pbftserver ${SERVER_SOURCES})
target_link_libraries(pbftserver
  pbft_grpc_proto
  crypto
  utils
  absl::check
  absl::flags
  absl::flags_parse
  absl::log
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

add_executable(driver "src/driver.cc" "src/constants.cc" "src/app_client.cc")
target_link_libraries(driver pbft_grpc_proto utils)

file(COPY ${CMAKE_SOURCE_DIR}/keys DESTINATION ${CMAKE_BINARY_DIR})

