cmake_minimum_required(VERSION 3.21)
project(InstallICU)

include(ExternalProject)

urd_source(URD_ICU_VERSION icu version)
urd_source(URD_ICU_URLS icu urls)
urd_source(URD_ICU_SHA256 icu sha256)

ExternalProject_Add(
    icu
    URL ${URD_ICU_URLS}
    URL_HASH SHA256=${URD_ICU_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PREFIX ${CMAKE_BINARY_DIR}/icu
    STAMP_DIR ${CMAKE_BINARY_DIR}/icu/stamp/${URD_ICU_VERSION}
    STEP_TARGETS download
    CONFIGURE_COMMAND
        ${CMAKE_COMMAND} -E env
        CC=${CMAKE_C_COMPILER}
        CXX=${CMAKE_CXX_COMPILER}
        <SOURCE_DIR>/source/configure
        --prefix=<INSTALL_DIR>
        --libdir=<INSTALL_DIR>/${CMAKE_INSTALL_LIBDIR}
        --enable-shared
        --disable-static
        --enable-rpath
        --disable-extras
        --disable-icuio
        --disable-tools
        --disable-tests
        --disable-samples
        --with-data-packaging=library
    BUILD_COMMAND make -j${URD_JOBS}
    INSTALL_COMMAND
        make -j${URD_JOBS} install
        COMMAND ${CMAKE_COMMAND} -E make_directory <INSTALL_DIR>/${CMAKE_INSTALL_DATADIR}/licenses/icu
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/LICENSE <INSTALL_DIR>/${CMAKE_INSTALL_DATADIR}/licenses/icu/LICENSE
    INSTALL_DIR ${CMAKE_INSTALL_PREFIX}
)
