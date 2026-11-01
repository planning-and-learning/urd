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

option(URD_ENABLE_FMT_FORMATTERS "urd/format.hpp: how urd's types are written, with fmt." ON)
# The superbuild's prefix comes after any prefix the user set, so a consumer may point at its own Boost.
list(APPEND CMAKE_PREFIX_PATH "${URD_NATIVE_PREFIX}")
# fmt writes urd's types (urd/format.hpp, with URD_ENABLE_FMT_FORMATTERS) and what the tests, generators and benchmarks
# print, which need it either way.
if(URD_ENABLE_FMT_FORMATTERS OR URD_BUILD_TESTS OR URD_BUILD_BENCHMARKS)
    find_package(fmt CONFIG REQUIRED)
endif()
# Boost.Container (flat sets and maps), Boost.Unordered (open-addressing hash tables) and Boost.Hash2 (xxHash for
# urd::Hash, in Boost since 1.89); cista (serialization, for records stored flat); gtl (sharded hash tables, for
# concurrent sets and maps by hashed keys). All header-only but for Boost.Container's small runtime library.
find_package(Boost 1.89 REQUIRED CONFIG COMPONENTS container unordered hash2)
find_package(cista REQUIRED CONFIG)
find_package(gtl REQUIRED CONFIG)

add_library(urd INTERFACE)
add_library(urd::urd ALIAS urd)
target_include_directories(urd INTERFACE
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_compile_features(urd INTERFACE cxx_std_20)
# Imported targets' headers are system headers, so the build's warnings cover only urd.
target_link_libraries(urd INTERFACE Boost::container Boost::unordered Boost::hash2 cista::cista gtl::gtl)
if(URD_ENABLE_FMT_FORMATTERS)
    target_link_libraries(urd INTERFACE fmt::fmt)
    target_compile_definitions(urd INTERFACE URD_ENABLE_FMT_FORMATTERS=1)
endif()

# The package: find_package(urd) after an install, or against this build tree with urd_DIR=<build>.
install(DIRECTORY include/urd DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
# The tags in plain words, for tools that write requests (share/urd/vocabulary.json, generated and checked by a test).
install(FILES share/urd/vocabulary.json share/urd/complexity.json DESTINATION "${CMAKE_INSTALL_DATADIR}/urd")
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
# Graphviz's dot draws the category diagrams. Published from main by .github/workflows/docs.yml.
find_program(URD_SPHINX_BUILD NAMES sphinx-build HINTS "${CMAKE_BINARY_DIR}/docs-venv/bin")
if(URD_SPHINX_BUILD)
    add_custom_target(docs
        COMMAND "${URD_SPHINX_BUILD}" -q -b html "${PROJECT_SOURCE_DIR}/docs" "${CMAKE_BINARY_DIR}/docs/html"
        COMMENT "Building the documentation into ${CMAKE_BINARY_DIR}/docs/html"
        VERBATIM
    )
endif()

# The parts beside the library, each behind its option: the examples, the benchmarks and the tests. The tests are enabled
# first, so that the examples register theirs, and the benchmarks come before the tests that check them.
if(URD_BUILD_TESTS OR URD_BUILD_BENCHMARKS)
    include(cmake/urd_objects.cmake)
endif()
if(URD_BUILD_TESTS)
    enable_testing()
endif()
if(URD_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
if(URD_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
if(URD_BUILD_TESTS)
    add_subdirectory(tests)
endif()
