# Builds and installs the dependencies listed in docs/SOURCES.json into CMAKE_INSTALL_PREFIX, which the
# main build then reads as URD_NATIVE_PREFIX. The same setup as yggdrasil's.

cmake_minimum_required(VERSION 3.21)

project(urd_native_sources)

include(GNUInstallDirs)
include(ProcessorCount)
include("${CMAKE_CURRENT_LIST_DIR}/source_manifest.cmake")

if(NOT DEFINED URD_JOBS)
    ProcessorCount(URD_JOBS)
endif()
if(NOT URD_JOBS)
    set(URD_JOBS 1)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release" FORCE)
endif()
set(BUILD_TYPE ${CMAKE_BUILD_TYPE})

if(APPLE)
    set(URD_DEPENDENCY_INSTALL_RPATH "@loader_path")
else()
    set(URD_DEPENDENCY_INSTALL_RPATH "$ORIGIN")
endif()

urd_sources(URD_DEPENDENCIES)
foreach(dependency IN LISTS URD_DEPENDENCIES)
    add_subdirectory("${dependency}")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${dependency}/licenses/"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/${dependency}"
        OPTIONAL
    )
endforeach()
