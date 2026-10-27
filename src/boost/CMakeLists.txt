cmake_minimum_required(VERSION 3.21)
project(InstallBoost)

include(ExternalProject)

urd_source(URD_BOOST_VERSION boost version)
urd_source(URD_BOOST_URLS boost urls)
urd_source(URD_BOOST_SHA256 boost sha256)

message(STATUS "Preparing external project \"boost\":")

# Only Boost.Container (flat sets and maps) and Boost.Unordered (open-addressing hash tables).
ExternalProject_Add(
    boost
    URL ${URD_BOOST_URLS}
    URL_HASH SHA256=${URD_BOOST_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PREFIX ${CMAKE_BINARY_DIR}/boost
    STAMP_DIR ${CMAKE_BINARY_DIR}/boost/stamp/${URD_BOOST_VERSION}
    STEP_TARGETS download
    LIST_SEPARATOR |
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DBUILD_SHARED_LIBS=ON
        -DBUILD_TESTING=OFF
        -DBOOST_INCLUDE_LIBRARIES=container|unordered
        -DCMAKE_INSTALL_RPATH=${URD_DEPENDENCY_INSTALL_RPATH}
    INSTALL_DIR ${CMAKE_INSTALL_PREFIX}
)
