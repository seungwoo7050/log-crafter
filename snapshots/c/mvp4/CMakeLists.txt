cmake_minimum_required(VERSION 3.20)

find_package(Threads REQUIRED)

add_library(logcrafter_c_core STATIC
    src/log_buffer.c
    src/lc_server.c
    src/persistence.c
    src/query_parser.c
    src/thread_pool.c
)

target_include_directories(logcrafter_c_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(logcrafter_c_core PUBLIC c_std_11)
target_link_libraries(logcrafter_c_core PUBLIC Threads::Threads)

add_executable(logcrafter_c_mvp4
    src/main.c
)

target_link_libraries(logcrafter_c_mvp4
    PRIVATE
        logcrafter_c_core
        Threads::Threads
)

target_compile_features(logcrafter_c_mvp4 PUBLIC c_std_11)

add_custom_target(logcrafter_c_mvp3
    DEPENDS logcrafter_c_mvp4
    COMMENT "MVP3 binary preserved via MVP4 build output"
)

add_custom_target(logcrafter_c_mvp2
    DEPENDS logcrafter_c_mvp3
    COMMENT "MVP2 binary preserved via MVP4 build output"
)

add_custom_target(logcrafter_c_mvp1
    DEPENDS logcrafter_c_mvp2
    COMMENT "MVP1 binary preserved via MVP4 build output"
)

install(TARGETS logcrafter_c_mvp4)
