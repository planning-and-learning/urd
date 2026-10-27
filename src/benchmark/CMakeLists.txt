cmake_minimum_required(VERSION 3.21)
project(InstallBenchmark)

include(ExternalProject)

urd_source(URD_BENCHMARK_GIT_REPOSITORY benchmark repository)
urd_source(URD_BENCHMARK_GIT_TAG benchmark revision)

message(STATUS "Dependency \"benchmark\"...")

# Google Benchmark, for benchmarks/ (built with -DURD_BUILD_BENCHMARKS=ON).
find_package(benchmark QUIET PATHS ${CMAKE_INSTALL_PREFIX} NO_DEFAULT_PATH)

if (NOT benchmark_FOUND)
    ExternalProject_Add(
            benchmark
            GIT_REPOSITORY ${URD_BENCHMARK_GIT_REPOSITORY}
            GIT_TAG ${URD_BENCHMARK_GIT_TAG}
            PREFIX ${CMAKE_BINARY_DIR}/benchmark
            STAMP_DIR ${CMAKE_BINARY_DIR}/benchmark/stamp/${URD_BENCHMARK_GIT_TAG}
            STEP_TARGETS download
            UPDATE_COMMAND ""
            CMAKE_ARGS
                -DCMAKE_BUILD_TYPE=Release
                -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
                -DCMAKE_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}
                -DCMAKE_INSTALL_RPATH=${URD_DEPENDENCY_INSTALL_RPATH}
                -DBENCHMARK_ENABLE_TESTING=OFF
                -DBENCHMARK_ENABLE_GTEST_TESTS=OFF
                -DBENCHMARK_ENABLE_WERROR=OFF
    )
else ()
    message(STATUS "benchmark is already installed.")
endif ()
