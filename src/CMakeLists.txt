# Builds and installs the dependencies listed in docs/SOURCES.json into CMAKE_INSTALL_PREFIX: what pyurd ships, and
# what urd, yggdrasil and the libraries above it build against (URD_NATIVE_PREFIX).

cmake_minimum_required(VERSION 3.21)

project(urd_native_sources)

include(GNUInstallDirs)
include(ProcessorCount)
include("${CMAKE_CURRENT_LIST_DIR}/source_manifest.cmake")

# Build configuration

if(NOT DEFINED URD_JOBS)
    ProcessorCount(URD_JOBS)
endif()
if(NOT URD_JOBS)
    set(URD_JOBS 1)
endif()

set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE)
    set(
        CMAKE_BUILD_TYPE
        ${DEFAULT_BUILD_TYPE}
        CACHE STRING
        "Choose the type of build, options are: Debug Release"
        FORCE
    )
endif()

set(BUILD_TYPE ${CMAKE_BUILD_TYPE})

message(STATUS "Build shared dependencies: ON")

if(APPLE)
    set(URD_DEPENDENCY_INSTALL_RPATH "@loader_path")
else()
    set(URD_DEPENDENCY_INSTALL_RPATH "$ORIGIN")
endif()
set(
    CMAKE_INSTALL_RPATH
    "${URD_DEPENDENCY_INSTALL_RPATH}"
    CACHE STRING
    "Install runtime path for bundled shared dependencies."
)
set(
    CMAKE_BUILD_WITH_INSTALL_RPATH
    OFF
    CACHE BOOL
    "Use the build-tree runtime path while building bundled dependencies."
)
set(
    CMAKE_INSTALL_RPATH_USE_LINK_PATH
    OFF
    CACHE BOOL
    "Do not append absolute linked library directories to bundled dependency install runtime paths."
)
message(STATUS "Dependency install rpath: ${URD_DEPENDENCY_INSTALL_RPATH}")

urd_sources(URD_DEPENDENCIES)
# Boost's MPI and Locale components require MPICH and ICU to exist first.
list(REMOVE_ITEM URD_DEPENDENCIES boost)
list(APPEND URD_DEPENDENCIES boost)
foreach(dependency IN LISTS URD_DEPENDENCIES)
    add_subdirectory("${dependency}")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${dependency}/licenses/"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/${dependency}"
        OPTIONAL
    )
endforeach()

list(TRANSFORM URD_DEPENDENCIES APPEND "-download" OUTPUT_VARIABLE URD_DOWNLOAD_TARGETS)
add_custom_target(corresponding-source-downloads DEPENDS ${URD_DOWNLOAD_TARGETS})
