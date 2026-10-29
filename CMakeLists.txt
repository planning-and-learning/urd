cmake_minimum_required(VERSION 3.21)

project(urd VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(URD_BUILD_TESTS "Build urd's tests." ${PROJECT_IS_TOP_LEVEL})
option(URD_BUILD_EXAMPLES "Build urd's examples." ${PROJECT_IS_TOP_LEVEL})
option(URD_BUILD_BENCHMARKS "Build urd's benchmarks (Google Benchmark; run them in a Release build)." OFF)
option(URD_STDLIB_ASSERTIONS "Check bounds and preconditions of the standard library in Debug builds." ON)
# The dependencies of docs/SOURCES.json, installed by the build of src/.
set(URD_NATIVE_PREFIX "${PROJECT_SOURCE_DIR}/dependencies-install" CACHE PATH "Where src/ installed the dependencies.")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Debug checks the properties urd's types claim." FORCE)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(URD_WITH_GTL "Concurrent sets and maps by hashed keys: gtl's parallel hash tables." ON)
# The superbuild's prefix comes after any prefix the user set, so a consumer may point at its own Boost.
list(APPEND CMAKE_PREFIX_PATH "${URD_NATIVE_PREFIX}")
# Boost.Container (flat sets and maps), Boost.Unordered (open-addressing hash tables) and Boost.Hash2 (xxHash for
# urd::Hash, in Boost since 1.89); cista (serialization, for records stored flat). All header-only but for
# Boost.Container's small runtime library.
find_package(Boost 1.89 REQUIRED CONFIG COMPONENTS container unordered hash2)
find_package(cista REQUIRED CONFIG)

add_library(urd INTERFACE)
add_library(urd::urd ALIAS urd)
target_include_directories(urd INTERFACE
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_compile_features(urd INTERFACE cxx_std_20)
# Imported targets' headers are system headers, so the build's warnings cover only urd.
target_link_libraries(urd INTERFACE Boost::container Boost::unordered Boost::hash2 cista::cista)
if(URD_WITH_GTL)
    find_package(gtl REQUIRED CONFIG)
    target_link_libraries(urd INTERFACE gtl::gtl)
    target_compile_definitions(urd INTERFACE URD_HAS_GTL=1)
endif()

# The package: find_package(urd) after an install, or against this build tree with urd_DIR=<build>.
install(DIRECTORY include/urd DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(TARGETS urd EXPORT urdTargets)
install(EXPORT urdTargets NAMESPACE urd:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/urd")
export(EXPORT urdTargets NAMESPACE urd:: FILE "${CMAKE_BINARY_DIR}/urdTargets.cmake")
configure_package_config_file(cmake/urdConfig.cmake.in "${CMAKE_BINARY_DIR}/urdConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/urd"
)
write_basic_package_version_file("${CMAKE_BINARY_DIR}/urdConfigVersion.cmake" COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_BINARY_DIR}/urdConfig.cmake" "${CMAKE_BINARY_DIR}/urdConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/urd"
)

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

# The documentation (docs/, Sphinx): `cmake --build build --target docs` where sphinx-build is found, in
# build/docs-venv (python3 -m venv build/docs-venv && build/docs-venv/bin/pip install -r docs/requirements.txt) or on the path.
find_program(URD_SPHINX_BUILD NAMES sphinx-build HINTS "${CMAKE_BINARY_DIR}/docs-venv/bin")
if(URD_SPHINX_BUILD)
    add_custom_target(docs
        COMMAND "${URD_SPHINX_BUILD}" -q -b html "${PROJECT_SOURCE_DIR}/docs" "${CMAKE_BINARY_DIR}/docs/html"
        COMMENT "Building the documentation into ${CMAKE_BINARY_DIR}/docs/html"
        VERBATIM
    )
endif()

if(URD_BUILD_EXAMPLES)
    find_package(Threads REQUIRED)
    foreach(example sets sequences maps rows dense interning queues scopes structures states)
        add_executable(example_${example} examples/${example}.cpp)
        target_link_libraries(example_${example} PRIVATE urd::urd Threads::Threads)
        target_include_directories(example_${example} PRIVATE "${PROJECT_SOURCE_DIR}/tests")
    endforeach()
endif()

if(URD_BUILD_BENCHMARKS)
    find_package(benchmark REQUIRED PATHS "${URD_NATIVE_PREFIX}" NO_DEFAULT_PATH)
    add_executable(benchmark_containers benchmarks/containers.cpp)
    target_link_libraries(benchmark_containers PRIVATE urd::urd benchmark::benchmark)
    add_executable(benchmark_pools benchmarks/pools.cpp)
    target_link_libraries(benchmark_pools PRIVATE urd::urd benchmark::benchmark)
endif()

if(URD_BUILD_TESTS)
    enable_testing()
    find_package(Threads REQUIRED)
    find_package(GTest 1.11.0 REQUIRED COMPONENTS GTest PATHS "${URD_NATIVE_PREFIX}" NO_DEFAULT_PATH)

    # The Store, and typed tests: one per property for every container that declares it, one per behavior of
    # nested Stores. Those below count allocations (tests/allocations.cpp replaces operator new).
    foreach(test store properties rows semantics queues pools registry denials views)
        add_executable(test_${test} tests/${test}.cpp tests/repr/main.cpp)
        target_link_libraries(test_${test} PRIVATE urd::urd GTest::gtest Threads::Threads)
        add_test(NAME ${test} COMMAND test_${test})
        set_tests_properties(${test} PROPERTIES FAIL_REGULAR_EXPRESSION "Running 0 tests")
    endforeach()
    target_sources(test_properties PRIVATE tests/allocations.cpp)
    target_sources(test_rows PRIVATE tests/allocations.cpp)
    target_sources(test_queues PRIVATE tests/allocations.cpp)
    target_sources(test_store PRIVATE tests/allocations.cpp)

    # The resolution table: docs/RESOLUTION.md is generated from the request grid and checked against the registry.
    add_executable(resolution_table tests/resolution_table.cpp)
    target_link_libraries(resolution_table PRIVATE urd::urd)
    if(URD_WITH_GTL)  # the table documents the full build: without gtl its concurrent rows read none
        add_test(NAME resolution COMMAND resolution_table --check ${PROJECT_SOURCE_DIR}/docs/RESOLUTION.md)
    endif()

    # The include root holds the vocabulary and the entry point only.
    add_test(NAME layout COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} -P ${PROJECT_SOURCE_DIR}/tests/layout.cmake)

    # Declarations from the four libraries as Store requests: compiling them is the test.
    add_library(survey OBJECT tests/survey.cpp)
    target_link_libraries(survey PRIVATE urd::urd)

    # The tests that came with the representations copied from yggdrasil, unchanged but for names.
    foreach(test segmented_vector tree_vector_set tree_bitset_set)
        add_executable(test_repr_${test} tests/repr/containers/${test}.cpp tests/repr/main.cpp)
        target_link_libraries(test_repr_${test} PRIVATE urd::urd GTest::gtest)
        add_test(NAME repr_${test} COMMAND test_repr_${test})
        set_tests_properties(repr_${test} PROPERTIES FAIL_REGULAR_EXPRESSION "Running 0 tests")
    endforeach()

    if(URD_BUILD_EXAMPLES)
        foreach(example sets sequences maps rows dense interning queues scopes structures states)
            add_test(NAME example_${example} COMMAND example_${example})
        endforeach()
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

    # A case that must fail with a given message: the error's text, not only the failure, is the test.
    function(urd_compile_fail_expects name case regex)
        set_tests_properties(${name}_${case} PROPERTIES WILL_FAIL FALSE PASS_REGULAR_EXPRESSION "${regex}")
    endfunction()

    urd_compile_fail_cases(compile_fail_store tests/compile_fail_store.cpp 44)
    urd_compile_fail_expects(compile_fail_store 44 "a queue empties by pop")
    urd_compile_fail_expects(compile_fail_store 8 "culprits<urd::tag::dense>")
    urd_compile_fail_expects(compile_fail_store 13 "culprits<urd::tag::stable_refs>")
    urd_compile_fail_expects(compile_fail_store 32 "culprits<>, false")
    urd_compile_fail_expects(compile_fail_store 33 "culprits<>, false")
    # A missing operation names the property that rules it out (store/denials.hpp).
    urd_compile_fail_expects(compile_fail_store 1 "a set has no positions")
    urd_compile_fail_expects(compile_fail_store 2 "insert: the Store is frozen")
    urd_compile_fail_expects(compile_fail_store 3 "pop_back: the Store is monotone")
    urd_compile_fail_expects(compile_fail_store 22 "begin/end: a queue")
    urd_compile_fail_expects(compile_fail_store 26 "a pool is reached by handle")
    if(NOT URD_WITH_GTL)
        urd_compile_fail_cases(compile_fail_gtl tests/compile_fail_gtl.cpp 2)
        urd_compile_fail_expects(compile_fail_gtl 1 "URD_WITH_GTL")
        urd_compile_fail_expects(compile_fail_gtl 2 "URD_WITH_GTL")
    endif()

    # A consumer: find_package(urd) against this build tree's export, then its build. What a user of the package sees.
    add_test(
        NAME consumer_configure
        COMMAND ${CMAKE_COMMAND} -S ${PROJECT_SOURCE_DIR}/tests/consumer -B ${CMAKE_BINARY_DIR}/consumer
                -Durd_DIR=${CMAKE_BINARY_DIR} -DCMAKE_PREFIX_PATH=${URD_NATIVE_PREFIX}
                -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} -DCMAKE_BUILD_TYPE=$<CONFIG>
    )
    set_tests_properties(consumer_configure PROPERTIES FIXTURES_SETUP consumer)
    add_test(NAME consumer_build COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}/consumer)
    set_tests_properties(consumer_build PROPERTIES FIXTURES_REQUIRED consumer)
endif()
