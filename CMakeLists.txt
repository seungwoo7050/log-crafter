cmake_minimum_required(VERSION 3.20)
project(LogCrafter LANGUAGES C CXX)

include(CTest)

set(LOGCRAFTER_MVP_TARGETS
    logcrafter_c_mvp0
    logcrafter_c_mvp1
    logcrafter_c_mvp2
    logcrafter_c_mvp3
    logcrafter_c_mvp4
    logcrafter_c_mvp5
    logcrafter_cpp_mvp0
    logcrafter_cpp_mvp1
    logcrafter_cpp_mvp2
    logcrafter_cpp_mvp3
    logcrafter_cpp_mvp4
    logcrafter_cpp_mvp5
    logcrafter_cpp_mvp6
)

add_subdirectory(work/c)
add_subdirectory(work/cpp)

foreach(target IN LISTS LOGCRAFTER_MVP_TARGETS)
    if(NOT TARGET ${target})
        add_custom_target(${target}
            COMMENT "Placeholder for ${target} implementation (to be defined in later Step B phases)."
        )
    endif()
endforeach()

if(BUILD_TESTING)
    add_subdirectory(tests)

    add_custom_target(logcrafter_tests
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Run the complete LogCrafter test suite via ctest."
    )
endif()

add_custom_target(logcrafter_all
    DEPENDS ${LOGCRAFTER_MVP_TARGETS}
    COMMENT "Top-level aggregator for all LogCrafter MVP targets."
)
