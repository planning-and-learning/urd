cmake_minimum_required(VERSION 3.21)

project(urd VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(URD_BUILD_TESTS "Build urd's tests." ${PROJECT_IS_TOP_LEVEL})
option(URD_BUILD_EXAMPLES "Build urd's examples." ${PROJECT_IS_TOP_LEVEL})
option(URD_STDLIB_ASSERTIONS "Check bounds and preconditions of the standard library in Debug builds." ON)
# The dependencies of docs/SOURCES.json, installed by the build of src/ (as in yggdrasil).
set(URD_NATIVE_PREFIX "${PROJECT_SOURCE_DIR}/dependencies-install" CACHE PATH "Where src/ installed the dependencies.")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Debug checks the properties urd's types claim." FORCE)
endif()

add_library(urd INTERFACE)
add_library(urd::urd ALIAS urd)
target_include_directories(urd INTERFACE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")
target_compile_features(urd INTERFACE cxx_std_20)
# The dependencies' headers are system headers, so the build's warnings cover only urd.
target_include_directories(urd SYSTEM INTERFACE "$<BUILD_INTERFACE:${URD_NATIVE_PREFIX}/include>")

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
    foreach(example store)
        add_executable(example_${example} examples/${example}.cpp)
        target_link_libraries(example_${example} PRIVATE urd::urd)
    endforeach()
endif()

if(URD_BUILD_TESTS)
    enable_testing()
    find_package(Threads REQUIRED)

    foreach(test store properties)
        add_executable(test_${test} tests/${test}.cpp)
        target_link_libraries(test_${test} PRIVATE urd::urd Threads::Threads)
        add_test(NAME ${test} COMMAND test_${test})
    endforeach()

    # The tests that came with the representations copied from yggdrasil, unchanged but for names.
    find_package(GTest 1.11.0 REQUIRED COMPONENTS GTest PATHS "${URD_NATIVE_PREFIX}" NO_DEFAULT_PATH)
    foreach(test segmented_vector unique_object_pool shared_object_pool)
        add_executable(test_repr_${test} tests/repr/containers/${test}.cpp tests/repr/main.cpp)
        target_link_libraries(test_repr_${test} PRIVATE urd::urd GTest::gtest)
        add_test(NAME repr_${test} COMMAND test_repr_${test})
        set_tests_properties(repr_${test} PROPERTIES FAIL_REGULAR_EXPRESSION "Running 0 tests")
    endforeach()

    if(URD_BUILD_EXAMPLES)
        foreach(example store)
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

    # Code that must not compile. The control build of each file must compile, so each case fails for its
    # own reason; the cases are skipped when the control does not build.
    function(urd_compile_fail_cases name source count)
        add_executable(test_${name}_control EXCLUDE_FROM_ALL ${source})
        target_link_libraries(test_${name}_control PRIVATE urd::urd)
        add_test(
            NAME ${name}_control
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target test_${name}_control --config $<CONFIG>
        )
        set_tests_properties(${name}_control PROPERTIES FIXTURES_SETUP ${name})
        foreach(case RANGE 1 ${count})
            add_executable(test_${name}_${case} EXCLUDE_FROM_ALL ${source})
            target_link_libraries(test_${name}_${case} PRIVATE urd::urd)
            target_compile_definitions(test_${name}_${case} PRIVATE URD_CASE=${case})
            add_test(
                NAME ${name}_${case}
                COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target test_${name}_${case} --config $<CONFIG>
            )
            set_tests_properties(${name}_${case} PROPERTIES WILL_FAIL TRUE FIXTURES_REQUIRED ${name})
        endforeach()
    endfunction()

    urd_compile_fail_cases(compile_fail_store tests/compile_fail_store.cpp 13)
endif()
