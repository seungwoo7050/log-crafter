cmake_minimum_required(VERSION 3.20)

find_package(Threads REQUIRED)

add_library(logcrafter_cpp_core STATIC
    src/lc_server.cpp
    src/log_buffer.cpp
    src/thread_pool.cpp
)

target_include_directories(logcrafter_cpp_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(logcrafter_cpp_core PUBLIC cxx_std_17)
target_link_libraries(logcrafter_cpp_core PUBLIC Threads::Threads)

add_executable(logcrafter_cpp_mvp2
    src/main.cpp
)

target_link_libraries(logcrafter_cpp_mvp2
    PRIVATE
        logcrafter_cpp_core
)

target_compile_features(logcrafter_cpp_mvp2 PRIVATE cxx_std_17)

add_custom_target(logcrafter_cpp_mvp1
    DEPENDS logcrafter_cpp_mvp2
    COMMENT "MVP1 binary preserved via MVP2 build output"
)

add_custom_target(logcrafter_cpp_mvp0
    DEPENDS logcrafter_cpp_mvp1
    COMMENT "MVP0 binary preserved via MVP2 build output"
)

install(TARGETS logcrafter_cpp_mvp2)
