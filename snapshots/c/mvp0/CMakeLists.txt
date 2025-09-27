cmake_minimum_required(VERSION 3.20)

add_library(logcrafter_c_core STATIC
    src/lc_server.c
)

target_include_directories(logcrafter_c_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(logcrafter_c_core PUBLIC c_std_11)

add_executable(logcrafter_c_mvp0
    src/main.c
)

target_link_libraries(logcrafter_c_mvp0
    PRIVATE
        logcrafter_c_core
)

target_compile_features(logcrafter_c_mvp0 PUBLIC c_std_11)

install(TARGETS logcrafter_c_mvp0)
