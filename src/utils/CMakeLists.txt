# Gather all .cpp files in the utils directory
file(GLOB_RECURSE UTIL_SOURCES "*.cc")

# Create a library from the source files
add_library(utils ${UTIL_SOURCES})

# Specify the include directories for this library
target_include_directories(utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/../types}")
