cmake_minimum_required(VERSION 3.21)

# The include tree's root holds the headers a user names in an include line (store.hpp, index.hpp, view.hpp, flat.hpp)
# and the resolution pipeline (concepts, elements, interface, properties, registry, traits). Containers live in
# urd/repr (declared by the registry, defined by the family headers in urd/use, which downstream code includes),
# compositions of Stores in urd/compose. A new root header must be added here on purpose.
set(allowed_files concepts.hpp elements.hpp flat.hpp index.hpp interface.hpp properties.hpp registry.hpp store.hpp traits.hpp view.hpp)
set(allowed_directories compose repr semantics store use)

file(GLOB entries RELATIVE "${ROOT}/include/urd" "${ROOT}/include/urd/*")
foreach(entry IN LISTS entries)
    if(IS_DIRECTORY "${ROOT}/include/urd/${entry}")
        if(NOT entry IN_LIST allowed_directories)
            message(FATAL_ERROR "include/urd/${entry}/: not an allowed directory of the root (tests/layout.cmake)")
        endif()
    elseif(NOT entry IN_LIST allowed_files)
        message(FATAL_ERROR "include/urd/${entry}: containers go in urd/repr, compositions in urd/compose (tests/layout.cmake)")
    endif()
endforeach()
