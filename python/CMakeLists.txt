# pyurd: the wheel that ships urd's headers and package. It compiles nothing; everything installs under pyurd/ in the
# pyurd component, which is what scikit-build-core packages. The dependencies urd's headers include are pyurd-native's
# (python/native/wheel/CMakeLists.txt), which pyurd requires.

set(URD_INSTALL_PACKAGEDIR "pyurd")

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

add_custom_target(pyurd_wheel ALL)
