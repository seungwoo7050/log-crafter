cmake_minimum_required(VERSION 3.10)
project(LogCrafterC VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -pedantic")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -DNDEBUG")

# Include directories
include_directories(include)

# Source files
set(SOURCES
    src/main.c
    src/server.c
    src/log_buffer.c
    src/thread_pool.c
    src/query_handler.c
    src/query_parser.c
    src/persistence.c
)

# Header files
set(HEADERS
    include/server.h
    include/log_buffer.h
    include/thread_pool.h
    include/query_handler.h
    include/query_parser.h
    include/persistence.h
)

# Create executable
add_executable(logcrafter-c ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(logcrafter-c pthread)

# Platform-specific libraries
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    target_link_libraries(logcrafter-c rt)
endif()

# Install rules
install(TARGETS logcrafter-c DESTINATION bin)

# Enable testing
enable_testing()

# Print build type
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C flags: ${CMAKE_C_FLAGS}")