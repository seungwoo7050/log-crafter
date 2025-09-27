find_package(Threads REQUIRED)

add_library(logcrafter_cpp_core_mvp0
    src/lc_server.cpp
)

target_include_directories(logcrafter_cpp_core_mvp0
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(logcrafter_cpp_core_mvp0 PUBLIC cxx_std_17)

target_link_libraries(logcrafter_cpp_core_mvp0
    PRIVATE
        Threads::Threads
)

add_executable(logcrafter_cpp_mvp0
    src/main.cpp
)

target_link_libraries(logcrafter_cpp_mvp0
    PRIVATE
        logcrafter_cpp_core_mvp0
)

target_compile_features(logcrafter_cpp_mvp0 PRIVATE cxx_std_17)
