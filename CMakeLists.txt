cmake_minimum_required(VERSION 3.21)

project(urd VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(URD_BUILD_TESTS "Build urd's tests." ${PROJECT_IS_TOP_LEVEL})
option(URD_BUILD_EXAMPLES "Build urd's examples." ${PROJECT_IS_TOP_LEVEL})
option(URD_STDLIB_ASSERTIONS "Check bounds and preconditions of the standard library in Debug builds." ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Debug checks the properties urd's types claim." FORCE)
endif()

add_library(urd INTERFACE)
add_library(urd::urd ALIAS urd)
target_include_directories(urd INTERFACE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")
target_compile_features(urd INTERFACE cxx_std_20)

if(PROJECT_IS_TOP_LEVEL)
    # Debug builds check the standard library's preconditions too, such as the bounds of
    # operator[] on std::vector (libstdc++; libc++ in its hardened mode).
    if(URD_STDLIB_ASSERTIONS)
        add_compile_definitions(
            $<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>
            "$<$<CONFIG:Debug>:_LIBCPP_HARDENING_MODE=(_LIBCPP_HARDENING_MODE_EXTENSIVE)>"
        )
    endif()
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
endif()

if(URD_BUILD_EXAMPLES)
    foreach(example basics set_algebra lookup)
        add_executable(example_${example} examples/${example}.cpp)
        target_link_libraries(example_${example} PRIVATE urd::urd)
    endforeach()
endif()

if(URD_BUILD_TESTS)
    enable_testing()

    foreach(test refined algorithms)
        add_executable(test_${test} tests/${test}.cpp)
        target_link_libraries(test_${test} PRIVATE urd::urd)
        add_test(NAME ${test} COMMAND test_${test})
    endforeach()

    if(URD_BUILD_EXAMPLES)
        foreach(example basics set_algebra lookup)
            add_test(NAME example_${example} COMMAND example_${example})
        endforeach()
    endif()

    # assume() checks its claim only in Debug builds.
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_executable(test_assume_fails tests/assume_fails.cpp)
        target_link_libraries(test_assume_fails PRIVATE urd::urd)
        add_test(NAME assume_fails COMMAND test_assume_fails)
        set_tests_properties(assume_fails PROPERTIES PASS_REGULAR_EXPRESSION "lack a claimed property")
    endif()

    # The surrounding code must compile, so each failure below is the case's own; the cases are
    # skipped when it does not.
    add_executable(test_compile_fail_control EXCLUDE_FROM_ALL tests/compile_fail.cpp)
    target_link_libraries(test_compile_fail_control PRIVATE urd::urd)
    add_test(
        NAME compile_fail_control
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target test_compile_fail_control --config $<CONFIG>
    )
    set_tests_properties(compile_fail_control PROPERTIES FIXTURES_SETUP compile_fail)

    foreach(case RANGE 1 8)
        add_executable(test_compile_fail_${case} EXCLUDE_FROM_ALL tests/compile_fail.cpp)
        target_link_libraries(test_compile_fail_${case} PRIVATE urd::urd)
        target_compile_definitions(test_compile_fail_${case} PRIVATE URD_CASE=${case})
        add_test(
            NAME compile_fail_${case}
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target test_compile_fail_${case} --config $<CONFIG>
        )
        set_tests_properties(compile_fail_${case} PROPERTIES WILL_FAIL TRUE FIXTURES_REQUIRED compile_fail)
    endforeach()
endif()
