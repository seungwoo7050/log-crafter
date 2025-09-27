cmake_minimum_required(VERSION 3.20)

find_package(Threads REQUIRED)

add_library(logcrafter_cpp_core STATIC
    src/lc_server.cpp
    src/log_buffer.cpp
    src/irc_channel.cpp
    src/irc_channel_manager.cpp
    src/irc_command_handler.cpp
    src/irc_command_parser.cpp
    src/irc_server.cpp
    src/persistence.cpp
    src/query_parser.cpp
    src/thread_pool.cpp
)

target_include_directories(logcrafter_cpp_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(logcrafter_cpp_core PUBLIC cxx_std_17)
target_link_libraries(logcrafter_cpp_core PUBLIC Threads::Threads)

add_executable(logcrafter_cpp_mvp6
    src/main.cpp
)

target_link_libraries(logcrafter_cpp_mvp6
    PRIVATE
        logcrafter_cpp_core
)

target_compile_features(logcrafter_cpp_mvp6 PRIVATE cxx_std_17)

add_custom_target(logcrafter_cpp_mvp5
    DEPENDS logcrafter_cpp_mvp6
    COMMENT "MVP5 binary preserved via MVP6 build output"
)

add_custom_target(logcrafter_cpp_mvp4
    DEPENDS logcrafter_cpp_mvp5
    COMMENT "MVP4 binary preserved via MVP6 build output"
)

add_custom_target(logcrafter_cpp_mvp3
    DEPENDS logcrafter_cpp_mvp4
    COMMENT "MVP3 binary preserved via MVP6 build output"
)

add_custom_target(logcrafter_cpp_mvp2
    DEPENDS logcrafter_cpp_mvp3
    COMMENT "MVP2 binary preserved via MVP6 build output"
)

add_custom_target(logcrafter_cpp_mvp1
    DEPENDS logcrafter_cpp_mvp2
    COMMENT "MVP1 binary preserved via MVP6 build output"
)

add_custom_target(logcrafter_cpp_mvp0
    DEPENDS logcrafter_cpp_mvp1
    COMMENT "MVP0 binary preserved via MVP6 build output"
)

install(TARGETS logcrafter_cpp_mvp6)
