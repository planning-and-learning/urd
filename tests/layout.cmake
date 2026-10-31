cmake_minimum_required(VERSION 3.21)

# The include tree's root holds the headers a user names in an include line (store.hpp, index.hpp, view.hpp, flat.hpp)
# and the resolution pipeline (concepts, elements, interface, properties, registry, traits). Containers live in
# urd/repr (declared by the registry, defined by the family headers in urd/use, which downstream code includes),
# compositions of Stores in urd/compose. A new root header must be added here on purpose.
set(allowed_files concepts.hpp container_synopsis.hpp costs.hpp elements.hpp flat.hpp format.hpp index.hpp interface.hpp properties.hpp property_set.hpp registry.hpp request.hpp store.hpp tag_synopsis.hpp tags.hpp traits.hpp view.hpp)
set(allowed_directories compose repr semantics store use)

# The resolution and vocabulary tables are built from declarations alone: their sources include no family header and no
# repr header.
# Were selection to complete a representation, the table would stop compiling; this keeps the proof honest.
# The request groups read every candidate's costs (urd::container_costs) the same way, so costs complete nothing either.
foreach(source tests/resolution_table.cpp tests/vocabulary_table.cpp tests/generated.hpp tests/grid.hpp tests/names.hpp tests/requests_group.cpp
               tests/requests.hpp)
    file(READ "${ROOT}/${source}" text)
    if(text MATCHES "#include [\"<]urd/(use|repr)/")
        message(FATAL_ERROR "${source}: the generated tables build from declarations only; it includes no urd/use or urd/repr header (tests/layout.cmake)")
    endif()
endforeach()

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
