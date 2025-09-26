cmake_minimum_required(VERSION 3.10)
project(LogCrafterCPP VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -pedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

# Include directories
include_directories(include)

# Source files
set(SOURCES
    src/main.cpp
    src/LogServer.cpp
    src/Logger.cpp
    src/ThreadPool.cpp
    src/LogBuffer.cpp
    src/QueryHandler.cpp
    src/QueryParser.cpp
)

# Header files
set(HEADERS
    include/LogServer.h
    include/Logger.h
    include/ThreadPool.h
    include/LogBuffer.h
    include/QueryHandler.h
    include/QueryParser.h
)

# Create executable
add_executable(logcrafter-cpp ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(logcrafter-cpp pthread)

# Install rules
install(TARGETS logcrafter-cpp DESTINATION bin)

# Enable testing
enable_testing()

# Add test subdirectory if exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()

# Print build type
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS}")