cmake_minimum_required(VERSION 3.21)
project(InstallNanobind)

include(ExternalProject)

urd_source(URD_NANOBIND_VERSION nanobind version)
urd_source(URD_NANOBIND_GIT_REPOSITORY nanobind repository)
urd_source(URD_NANOBIND_GIT_TAG nanobind revision)
urd_source(URD_NANOBIND_SUBMODULES nanobind submodules)

message(STATUS "Dependency \"nanobind\"...")

find_package(Python 3.11 COMPONENTS Interpreter Development.Module REQUIRED)
find_package(nanobind ${URD_NANOBIND_VERSION} CONFIG QUIET PATHS ${CMAKE_INSTALL_PREFIX} NO_DEFAULT_PATH)

if(NOT nanobind_FOUND)
    list(APPEND CMAKE_ARGS
            -DCMAKE_BUILD_TYPE=${BUILD_TYPE}
            -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
            -DCMAKE_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}
            -DNB_TEST=OFF
            -DNB_CREATE_INSTALL_RULES=ON
    )

    message(STATUS "Preparing external project \"nanobind\" with args:")
    foreach(CMAKE_ARG ${CMAKE_ARGS})
        message(STATUS "-- ${CMAKE_ARG}")
    endforeach()

    ExternalProject_Add(
            nanobind
            GIT_REPOSITORY ${URD_NANOBIND_GIT_REPOSITORY}
            GIT_TAG ${URD_NANOBIND_GIT_TAG}
            PREFIX ${CMAKE_BINARY_DIR}/nanobind
            STAMP_DIR ${CMAKE_BINARY_DIR}/nanobind/stamp/${URD_NANOBIND_GIT_TAG}
            STEP_TARGETS download
            UPDATE_COMMAND ""
            GIT_SUBMODULES_RECURSE ${URD_NANOBIND_SUBMODULES}
            CMAKE_ARGS ${CMAKE_ARGS}
    )
else()
    message(STATUS "nanobind is already installed.")
endif()
