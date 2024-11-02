# Copyright 2018 gRPC authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# cmake build file for C++ helloworld example.
# Assumes protobuf and gRPC have been installed using cmake.
# See cmake_externalproject/CMakeLists.txt for all-in-one cmake build
# that automatically builds all the dependencies before building helloworld.

cmake_minimum_required(VERSION 3.20)

project(paxos C CXX)

include(common.cmake)


# Proto file
get_filename_component(paxos_proto "./src/proto/paxos.proto" ABSOLUTE)
get_filename_component(paxos_proto_path "${paxos_proto}" PATH)

# Generated sources
set(paxos_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/paxos.pb.cc")
set(paxos_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/paxos.pb.h")
set(paxos_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/paxos.grpc.pb.cc")
set(paxos_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/paxos.grpc.pb.h")
add_custom_command(
      OUTPUT "${paxos_proto_srcs}" "${paxos_proto_hdrs}" "${paxos_grpc_srcs}" "${paxos_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${paxos_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${paxos_proto}"
      DEPENDS "${paxos_proto}")


# Include generated *.pb.h files
include_directories(${CMAKE_CURRENT_BINARY_DIR} 
  ${CMAKE_CURRENT_SOURCE_DIR} 
  "${CMAKE_CURRENT_SOURCE_DIR}/types"
  "${CMAKE_CURRENT_SOURCE_DIR}/utils"
  "${CMAKE_CURRENT_SOURCE_DIR}/client"
  "${CMAKE_CURRENT_SOURCE_DIR}/server")

# paxos_grpc_proto
add_library(paxos_grpc_proto
  ${paxos_grpc_srcs}
  ${paxos_grpc_hdrs}
  ${paxos_proto_srcs}
  ${paxos_proto_hdrs})
target_link_libraries(paxos_grpc_proto
  absl::check
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})


# Add utils subdirectory
add_subdirectory(src/utils)
target_link_libraries(utils)


# Add client subdirectory
add_subdirectory(src/client)
target_link_libraries(client
  paxos_grpc_proto
  absl::check
  absl::flags
  absl::flags_parse
  absl::log
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})


add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/src/thirdparty/SQLiteCpp)

# Gather all .cc files in the server directory
file(GLOB_RECURSE SERVER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/server/*.cc" "src/constants.cc")

message(STATUS "SERVER_SOURCES: ${SERVER_SOURCES}")

add_executable(paxosserver ${SERVER_SOURCES})
target_link_libraries(paxosserver
  paxos_grpc_proto
  client
  SQLiteCpp
  sqlite3
  pthread
  dl
  absl::check
  absl::flags
  absl::flags_parse
  absl::log
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

add_executable(driver "src/driver.cc" "src/constants.cc")
target_link_libraries(driver utils client)
  