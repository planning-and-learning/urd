# pyurd-native: the wheel that ships the native prefix the build of native/ installed (URD_NATIVE_PREFIX), the
# dependencies of urd, yggdrasil and the libraries above them. It compiles nothing; everything installs under
# pyurd_native/ in the pyurd_native component, which is what scikit-build-core packages. urd's headers and package are
# pyurd's (python/CMakeLists.txt).

cmake_minimum_required(VERSION 3.21)

project(pyurd_native LANGUAGES NONE)

# Libraries go to lib/, as the build of native/ puts them (no language is enabled, so GNUInstallDirs has no default).
if(NOT CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR "lib")
endif()

# The project directory, two up (python/ of the repository): its src/pyurd_native and native/.
get_filename_component(URD_NATIVE_PROJECT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(URD_NATIVE_INSTALL_PACKAGEDIR "pyurd_native")

if(NOT URD_NATIVE_PREFIX OR NOT EXISTS "${URD_NATIVE_PREFIX}/include")
    message(FATAL_ERROR "URD_NATIVE_PREFIX must name the prefix the build of native/ installed the dependencies into: '${URD_NATIVE_PREFIX}'")
endif()

# The platform's shared library suffix, spelled out for the same reason.
if(APPLE)
    set(shared_library_suffix ".dylib")
else()
    set(shared_library_suffix ".so")
endif()

# brokkr (pybrokkr): found through scikit-build-core in wheel builds, else asked of the Python that has it.
find_package(brokkr QUIET)
if(NOT brokkr_FOUND)
    find_package(Python 3.11 REQUIRED COMPONENTS Interpreter)
    execute_process(COMMAND "${Python_EXECUTABLE}" -m pybrokkr --cmake-dir
                    OUTPUT_VARIABLE _brokkr_dir OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    set(brokkr_DIR "${_brokkr_dir}" CACHE PATH "The directory holding brokkrConfig.cmake" FORCE)
    find_package(brokkr REQUIRED)
endif()

# The Python package.
foreach(python_file IN ITEMS pyurd_native/__init__.py pyurd_native/__main__.py pyurd_native/py.typed)
    get_filename_component(python_file_directory "${python_file}" DIRECTORY)
    install(FILES "${URD_NATIVE_PROJECT_DIR}/src/${python_file}"
        DESTINATION "${python_file_directory}"
        COMPONENT pyurd_native
    )
endforeach()

# The native prefix, pruned to what consumers need at build and run time. googletest and benchmark are bundled:
# downstream test and benchmark suites find them here, pinned with the rest. Pruned: static archives other than fmt's
# installed C target, libtool files, command line tools other than the minimal MPI toolchain, and all but one copy of
# each Boost shared library. urd's own files are pyurd's, not this wheel's.
foreach(native_dir IN ITEMS include lib lib64)
    if(EXISTS "${URD_NATIVE_PREFIX}/${native_dir}")
        install(DIRECTORY "${URD_NATIVE_PREFIX}/${native_dir}"
            DESTINATION "${URD_NATIVE_INSTALL_PACKAGEDIR}"
            COMPONENT pyurd_native
            REGEX "/include/urd$" EXCLUDE
            REGEX "/cmake/urd$" EXCLUDE
            REGEX "/include/yggdrasil$" EXCLUDE
            REGEX "/cmake/yggdrasil$" EXCLUDE
            REGEX "/pkgconfig$" EXCLUDE
            REGEX "\\.(a|la)$" EXCLUDE
            REGEX "/libboost_[^/]*\\.(so|dylib)[^/]*$" EXCLUDE
            REGEX "/lib(mpi|mpl|opa)[^/]*\\.(so|dylib)[^/]*$" EXCLUDE
        )
        file(GLOB boost_runtime_libraries
            LIST_DIRECTORIES false
            "${URD_NATIVE_PREFIX}/${native_dir}/libboost_*${shared_library_suffix}*"
        )
        foreach(boost_runtime_library IN LISTS boost_runtime_libraries)
            if(NOT IS_SYMLINK "${boost_runtime_library}")
                install(FILES "${boost_runtime_library}"
                    DESTINATION "${URD_NATIVE_INSTALL_PACKAGEDIR}/${native_dir}"
                    COMPONENT pyurd_native
                )
            endif()
        endforeach()
    endif()
endforeach()

if(EXISTS "${URD_NATIVE_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libfmt-c.a")
    install(FILES "${URD_NATIVE_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libfmt-c.a"
        DESTINATION "${URD_NATIVE_INSTALL_PACKAGEDIR}/${CMAKE_INSTALL_LIBDIR}"
        COMPONENT pyurd_native
    )
endif()

if(EXISTS "${URD_NATIVE_PREFIX}/nanobind")
    install(DIRECTORY "${URD_NATIVE_PREFIX}/nanobind"
        DESTINATION "${URD_NATIVE_INSTALL_PACKAGEDIR}"
        COMPONENT pyurd_native
    )
endif()

if(UNIX)
    if(APPLE)
        set(mpi_runtime_name "libmpi.dylib")
    else()
        set(mpi_runtime_name "libmpi.so")
    endif()
    file(REAL_PATH "${URD_NATIVE_PREFIX}/${CMAKE_INSTALL_LIBDIR}/${mpi_runtime_name}" mpi_runtime_library)
    install(FILES "${mpi_runtime_library}"
        DESTINATION "${URD_NATIVE_INSTALL_PACKAGEDIR}/${CMAKE_INSTALL_LIBDIR}"
        RENAME "${mpi_runtime_name}"
        COMPONENT pyurd_native
    )
    foreach(mpi_tool IN ITEMS mpicc mpicxx hydra_pmi_proxy)
        install(PROGRAMS "${URD_NATIVE_PREFIX}/bin/${mpi_tool}"
            DESTINATION "${URD_NATIVE_INSTALL_PACKAGEDIR}/bin"
            COMPONENT pyurd_native
        )
    endforeach()
    file(REAL_PATH "${URD_NATIVE_PREFIX}/bin/mpiexec" mpi_launcher)
    install(PROGRAMS "${mpi_launcher}"
        DESTINATION "${URD_NATIVE_INSTALL_PACKAGEDIR}/bin"
        RENAME mpiexec
        COMPONENT pyurd_native
    )
endif()

install(DIRECTORY "${URD_NATIVE_PREFIX}/share/licenses/"
    DESTINATION "${URD_NATIVE_INSTALL_PACKAGEDIR}/share/licenses"
    COMPONENT pyurd_native
)
install(FILES "${URD_NATIVE_PROJECT_DIR}/native/CORRESPONDING_SOURCE.md" "${URD_NATIVE_PROJECT_DIR}/native/SOURCES.json"
    DESTINATION "${URD_NATIVE_INSTALL_PACKAGEDIR}/share"
    COMPONENT pyurd_native
)

if(WIN32 AND EXISTS "${URD_NATIVE_PREFIX}/bin")
    # Runtime DLLs live in bin/ on Windows.
    install(DIRECTORY "${URD_NATIVE_PREFIX}/bin"
        DESTINATION "${URD_NATIVE_INSTALL_PACKAGEDIR}"
        COMPONENT pyurd_native
        FILES_MATCHING PATTERN "*.dll"
    )
endif()

install(CODE
    "include(\"${brokkr_DIR}/brokkrFixRuntimePaths.cmake\")
         brokkr_fix_runtime_paths(LIB_DIR_GLOB \"pyurd_native/lib*\")"
    COMPONENT pyurd_native
)

add_custom_target(pyurd_native_wheel ALL)
