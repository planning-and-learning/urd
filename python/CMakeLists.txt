# pyurd: the wheel that ships urd's headers and package, and the native prefix src/ built (URD_NATIVE_PREFIX), the
# dependencies of urd, yggdrasil and the libraries above them. It has no compiled module; everything installs under
# pyurd/ in the pyurd component, which is what scikit-build-core packages.

set(URD_INSTALL_PACKAGEDIR "pyurd")

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
foreach(python_file IN ITEMS pyurd/__init__.py pyurd/__main__.py pyurd/py.typed pyurd/repair.py)
    get_filename_component(python_file_directory "${python_file}" DIRECTORY)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/${python_file}"
        DESTINATION "${python_file_directory}"
        COMPONENT pyurd
    )
endforeach()

# urd itself: headers, the target, its configuration and the vocabulary.
install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/urd"
    DESTINATION "${URD_INSTALL_PACKAGEDIR}/include"
    COMPONENT pyurd
)
install(FILES "${PROJECT_SOURCE_DIR}/share/urd/vocabulary.json" "${PROJECT_SOURCE_DIR}/share/urd/complexity.json"
    DESTINATION "${URD_INSTALL_PACKAGEDIR}/share/urd"
    COMPONENT pyurd
)
install(TARGETS urd urd_count_allocations EXPORT urdPackageTargets COMPONENT pyurd)
install(EXPORT urdPackageTargets
    NAMESPACE urd::
    FILE "urdTargets.cmake"
    DESTINATION "${URD_INSTALL_PACKAGEDIR}/${CMAKE_INSTALL_LIBDIR}/cmake/urd"
    COMPONENT pyurd
)
install(FILES "${CMAKE_BINARY_DIR}/urdConfig.cmake" "${CMAKE_BINARY_DIR}/urdConfigVersion.cmake"
    DESTINATION "${URD_INSTALL_PACKAGEDIR}/${CMAKE_INSTALL_LIBDIR}/cmake/urd"
    COMPONENT pyurd
)

# The native prefix, pruned to what consumers need at build and run time. googletest and benchmark are bundled:
# downstream test and benchmark suites find them here, pinned with the rest. Pruned: static archives other than fmt's
# installed C target, libtool files, command line tools other than the minimal MPI toolchain, and all but one copy of
# each Boost shared library. urd's own files come from this build, not from the prefix.
foreach(native_dir IN ITEMS include lib lib64)
    if(EXISTS "${URD_NATIVE_PREFIX}/${native_dir}")
        install(DIRECTORY "${URD_NATIVE_PREFIX}/${native_dir}"
            DESTINATION "${URD_INSTALL_PACKAGEDIR}"
            COMPONENT pyurd
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
            "${URD_NATIVE_PREFIX}/${native_dir}/libboost_*${CMAKE_SHARED_LIBRARY_SUFFIX}*"
        )
        foreach(boost_runtime_library IN LISTS boost_runtime_libraries)
            if(NOT IS_SYMLINK "${boost_runtime_library}")
                install(FILES "${boost_runtime_library}"
                    DESTINATION "${URD_INSTALL_PACKAGEDIR}/${native_dir}"
                    COMPONENT pyurd
                )
            endif()
        endforeach()
    endif()
endforeach()

if(EXISTS "${URD_NATIVE_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libfmt-c.a")
    install(FILES "${URD_NATIVE_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libfmt-c.a"
        DESTINATION "${URD_INSTALL_PACKAGEDIR}/${CMAKE_INSTALL_LIBDIR}"
        COMPONENT pyurd
    )
endif()

if(EXISTS "${URD_NATIVE_PREFIX}/nanobind")
    install(DIRECTORY "${URD_NATIVE_PREFIX}/nanobind"
        DESTINATION "${URD_INSTALL_PACKAGEDIR}"
        COMPONENT pyurd
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
        DESTINATION "${URD_INSTALL_PACKAGEDIR}/${CMAKE_INSTALL_LIBDIR}"
        RENAME "${mpi_runtime_name}"
        COMPONENT pyurd
    )
    foreach(mpi_tool IN ITEMS mpicc mpicxx hydra_pmi_proxy)
        install(PROGRAMS "${URD_NATIVE_PREFIX}/bin/${mpi_tool}"
            DESTINATION "${URD_INSTALL_PACKAGEDIR}/bin"
            COMPONENT pyurd
        )
    endforeach()
    file(REAL_PATH "${URD_NATIVE_PREFIX}/bin/mpiexec" mpi_launcher)
    install(PROGRAMS "${mpi_launcher}"
        DESTINATION "${URD_INSTALL_PACKAGEDIR}/bin"
        RENAME mpiexec
        COMPONENT pyurd
    )
endif()

install(DIRECTORY "${URD_NATIVE_PREFIX}/share/licenses/"
    DESTINATION "${URD_INSTALL_PACKAGEDIR}/share/licenses"
    COMPONENT pyurd
)
install(FILES "${PROJECT_SOURCE_DIR}/docs/CORRESPONDING_SOURCE.md" "${PROJECT_SOURCE_DIR}/docs/SOURCES.json"
    DESTINATION "${URD_INSTALL_PACKAGEDIR}/share"
    COMPONENT pyurd
)

if(WIN32 AND EXISTS "${URD_NATIVE_PREFIX}/bin")
    # Runtime DLLs live in bin/ on Windows.
    install(DIRECTORY "${URD_NATIVE_PREFIX}/bin"
        DESTINATION "${URD_INSTALL_PACKAGEDIR}"
        COMPONENT pyurd
        FILES_MATCHING PATTERN "*.dll"
    )
endif()

install(CODE
    "include(\"${brokkr_DIR}/brokkrFixRuntimePaths.cmake\")
         brokkr_fix_runtime_paths(LIB_DIR_GLOB \"pyurd/lib*\")"
    COMPONENT pyurd
)

add_custom_target(pyurd_wheel ALL)
