cmake_minimum_required(VERSION 3.21)

# The include tree's root holds the vocabulary and the entry point only; containers live in urd/repr (which downstream
# code does not include) and compositions of Stores in urd/compose. A new root header must be added here on purpose.
set(allowed_files concepts.hpp elements.hpp flat.hpp index.hpp interface.hpp properties.hpp registry.hpp store.hpp traits.hpp view.hpp)
set(allowed_directories compose repr semantics store)

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
