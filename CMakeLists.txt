cmake_minimum_required(VERSION 3.21)

project(urd VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(URD_BUILD_TESTS "Build urd's tests." ${PROJECT_IS_TOP_LEVEL})
option(URD_BUILD_EXAMPLES "Build urd's examples." ${PROJECT_IS_TOP_LEVEL})
option(URD_BUILD_BENCHMARKS "Build urd's benchmarks (Google Benchmark; run them in a Release build)." OFF)
option(URD_STDLIB_ASSERTIONS "Check bounds and preconditions of the standard library in Debug builds." ON)
# The dependencies of docs/SOURCES.json, installed by the build of src/.
set(URD_NATIVE_PREFIX "${PROJECT_SOURCE_DIR}/dependencies-install" CACHE PATH "Where src/ installed the dependencies.")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Debug checks the properties urd's types claim." FORCE)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(URD_WITH_GTL "Concurrent sets and maps by hashed keys: gtl's parallel hash tables." ON)
option(URD_ENABLE_FMT_FORMATTERS "urd/format.hpp: how urd's types are written, with fmt." ON)
# The superbuild's prefix comes after any prefix the user set, so a consumer may point at its own Boost.
list(APPEND CMAKE_PREFIX_PATH "${URD_NATIVE_PREFIX}")
# fmt writes urd's types (urd/format.hpp, with URD_ENABLE_FMT_FORMATTERS) and what the tests, generators and benchmarks
# print, which need it either way.
if(URD_ENABLE_FMT_FORMATTERS OR URD_BUILD_TESTS OR URD_BUILD_BENCHMARKS)
    find_package(fmt CONFIG REQUIRED)
endif()
# Boost.Container (flat sets and maps), Boost.Unordered (open-addressing hash tables) and Boost.Hash2 (xxHash for
# urd::Hash, in Boost since 1.89); cista (serialization, for records stored flat). All header-only but for
# Boost.Container's small runtime library.
find_package(Boost 1.89 REQUIRED CONFIG COMPONENTS container unordered hash2)
find_package(cista REQUIRED CONFIG)

add_library(urd INTERFACE)
add_library(urd::urd ALIAS urd)
target_include_directories(urd INTERFACE
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_compile_features(urd INTERFACE cxx_std_20)
# Imported targets' headers are system headers, so the build's warnings cover only urd.
target_link_libraries(urd INTERFACE Boost::container Boost::unordered Boost::hash2 cista::cista)
if(URD_WITH_GTL)
    find_package(gtl REQUIRED CONFIG)
    target_link_libraries(urd INTERFACE gtl::gtl)
    target_compile_definitions(urd INTERFACE URD_HAS_GTL=1)
endif()
if(URD_ENABLE_FMT_FORMATTERS)
    target_link_libraries(urd INTERFACE fmt::fmt)
    target_compile_definitions(urd INTERFACE URD_ENABLE_FMT_FORMATTERS=1)
endif()

# The package: find_package(urd) after an install, or against this build tree with urd_DIR=<build>.
install(DIRECTORY include/urd DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
# The tags in plain words, for tools that write requests (share/urd/vocabulary.json, generated and checked by a test).
install(FILES share/urd/vocabulary.json share/urd/complexity.json DESTINATION "${CMAKE_INSTALL_DATADIR}/urd")
install(TARGETS urd EXPORT urdTargets)
install(EXPORT urdTargets NAMESPACE urd:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/urd")
export(EXPORT urdTargets NAMESPACE urd:: FILE "${CMAKE_BINARY_DIR}/urdTargets.cmake")
configure_package_config_file(cmake/urdConfig.cmake.in "${CMAKE_BINARY_DIR}/urdConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/urd"
)
write_basic_package_version_file("${CMAKE_BINARY_DIR}/urdConfigVersion.cmake" COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_BINARY_DIR}/urdConfig.cmake" "${CMAKE_BINARY_DIR}/urdConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/urd"
)

if(PROJECT_IS_TOP_LEVEL)
    # Debug builds check the standard library's preconditions too, such as the bounds of
    # operator[] on std::vector (libstdc++; libc++ in its hardened mode).
    if(URD_STDLIB_ASSERTIONS)
        add_compile_definitions(
            $<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>
            "$<$<CONFIG:Debug>:_LIBCPP_HARDENING_MODE=(_LIBCPP_HARDENING_MODE_EXTENSIVE)>"
        )
    endif()
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
endif()

# The documentation (docs/, Sphinx): `cmake --build build --target docs` where sphinx-build is found, in
# build/docs-venv (python3 -m venv build/docs-venv && build/docs-venv/bin/pip install -r docs/requirements.txt) or on the path.
# Graphviz's dot draws the category diagrams. Published from main by .github/workflows/docs.yml.
find_program(URD_SPHINX_BUILD NAMES sphinx-build HINTS "${CMAKE_BINARY_DIR}/docs-venv/bin")
if(URD_SPHINX_BUILD)
    add_custom_target(docs
        COMMAND "${URD_SPHINX_BUILD}" -q -b html "${PROJECT_SOURCE_DIR}/docs" "${CMAKE_BINARY_DIR}/docs/html"
        COMMENT "Building the documentation into ${CMAKE_BINARY_DIR}/docs/html"
        VERBATIM
    )
endif()

if(URD_BUILD_EXAMPLES)
    find_package(Threads REQUIRED)
    foreach(example sets sequences maps rows dense interning queues scopes structures states)
        add_executable(example_${example} examples/${example}.cpp)
        target_link_libraries(example_${example} PRIVATE urd::urd Threads::Threads)
        target_include_directories(example_${example} PRIVATE "${PROJECT_SOURCE_DIR}/tests")
    endforeach()
endif()

# The objects compiled once per container or per grid group stay small: each compiles under a 1 GiB address space
# where prlimit exists, so one that outgrows it fails the build by name instead of slowing every build down.
find_program(URD_PRLIMIT prlimit)
function(urd_small_object target)
    if(URD_PRLIMIT)
        set_property(TARGET ${target} PROPERTY RULE_LAUNCH_COMPILE "${URD_PRLIMIT} --as=1073741824 --")
    endif()
endfunction()

# One object per container of benchmarks/requests.json, each compiling SOURCE with only that container's family headers
# included first, so adding a container adds its own objects and changing one rebuilds its own. KEY names the map of
# containers to grid positions (FIELDS numbers each); an object takes at most PER_OBJECT of them, so no object grows
# with the grid. Each object defines URD_CONTAINER, DEFINE (its positions) and URD_CHUNK (its index among the
# container's objects), and its source ends with TAIL, where @object@ is the object's name. Without gtl, gtl's
# containers are left out. The objects are named PREFIX_<container>_<chunk> and returned in OUT_OBJECTS.
function(urd_per_container)
    cmake_parse_arguments(PARSE_ARGV 0 arg "" "KEY;FIELDS;PER_OBJECT;PREFIX;SOURCE;DEFINE;TAIL;OUT_OBJECTS" "LINK")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/benchmarks/requests.json)
    set(requests "{}")
    if(EXISTS ${PROJECT_SOURCE_DIR}/benchmarks/requests.json)
        file(READ ${PROJECT_SOURCE_DIR}/benchmarks/requests.json requests)
    endif()
    string(JSON container_count ERROR_VARIABLE missing LENGTH "${requests}" ${arg_KEY})
    if(missing)
        message(WARNING "benchmarks/requests.json lists no ${arg_KEY}: requests_table --write it, then reconfigure")
        set(container_count 0)
    endif()
    set(objects "")
    foreach(index RANGE ${container_count})
        if(index EQUAL container_count)
            break()
        endif()
        string(JSON container MEMBER "${requests}" ${arg_KEY} ${index})
        if(NOT URD_WITH_GTL AND container MATCHES "^parallel_")
            continue()
        endif()
        string(JSON families_count ERROR_VARIABLE no_families LENGTH "${requests}" families ${container})
        if(no_families)
            set(families_count 0)
        endif()
        set(headers "")
        if(families_count GREATER 0)
            math(EXPR last_family "${families_count} - 1")
            foreach(at RANGE ${last_family})
                string(JSON family GET "${requests}" families ${container} ${at})
                string(APPEND headers "#include \"urd/use/${family}.hpp\"\n")
            endforeach()
        endif()
        string(JSON values LENGTH "${requests}" ${arg_KEY} ${container})
        math(EXPR entries "${values} / ${arg_FIELDS}")
        math(EXPR last_entry "${entries} - 1")
        math(EXPR last_field "${arg_FIELDS} - 1")
        set(chunk 0)
        set(positions "")
        foreach(entry RANGE ${last_entry})
            foreach(field RANGE ${last_field})
                math(EXPR at "${entry} * ${arg_FIELDS} + ${field}")
                string(JSON position GET "${requests}" ${arg_KEY} ${container} ${at})
                list(APPEND positions ${position})
            endforeach()
            list(LENGTH positions held)
            math(EXPR held "${held} / ${arg_FIELDS}")
            if(held EQUAL arg_PER_OBJECT OR entry EQUAL last_entry)
                set(object ${arg_PREFIX}_${container}_${chunk})
                list(JOIN positions "," joined)
                string(REPLACE "@object@" "${object}" tail "${arg_TAIL}")
                file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/per_container/${object}.cpp
                     CONTENT "${headers}#include \"${arg_SOURCE}\"\n\n${tail}")
                add_library(${object} OBJECT ${CMAKE_CURRENT_BINARY_DIR}/per_container/${object}.cpp)
                target_link_libraries(${object} PRIVATE urd::urd ${arg_LINK})
                target_include_directories(${object} PRIVATE ${PROJECT_SOURCE_DIR}/tests ${PROJECT_SOURCE_DIR}/benchmarks)
                target_compile_definitions(${object} PRIVATE URD_CONTAINER="${container}" ${arg_DEFINE}=${joined} URD_CHUNK=${chunk})
                # GCC's -Wmaybe-uninitialized misfires on a slot vector held in a std::optional (a known false positive
                # of its optimizer); the other warnings stay on.
                target_compile_options(${object} PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-maybe-uninitialized>)
                urd_small_object(${object})
                list(APPEND objects ${object})
                math(EXPR chunk "${chunk} + 1")
                set(positions "")
            endif()
        endforeach()
    endforeach()
    set(${arg_OUT_OBJECTS} ${objects} PARENT_SCOPE)
endfunction()

if(URD_BUILD_BENCHMARKS)
    find_package(benchmark REQUIRED PATHS "${URD_NATIVE_PREFIX}" NO_DEFAULT_PATH)
    add_executable(benchmark_containers benchmarks/containers.cpp)
    target_link_libraries(benchmark_containers PRIVATE urd::urd benchmark::benchmark)
    add_executable(benchmark_pools benchmarks/pools.cpp)
    target_link_libraries(benchmark_pools PRIVATE urd::urd benchmark::benchmark)
    # Each container measured on its own (benchmarks/experiment.cpp), linked into one program with a generated main.
    # The registry's claims are judged from the results afterwards (benchmarks/record.py), never compiled or run.
    urd_per_container(KEY containers FIELDS 3 PER_OBJECT 6 PREFIX benchmark SOURCE ${PROJECT_SOURCE_DIR}/benchmarks/experiment.cpp
                      DEFINE URD_EXPERIMENTS LINK benchmark::benchmark fmt::fmt
                      TAIL "void register_@object@();\nvoid register_@object@() { register_experiments(); }\n"
                      OUT_OBJECTS experiment_objects)
    set(URD_EXPERIMENT_DECLARATIONS "")
    set(URD_EXPERIMENT_CALLS "")
    foreach(object IN LISTS experiment_objects)
        string(APPEND URD_EXPERIMENT_DECLARATIONS "void register_${object}();\n")
        string(APPEND URD_EXPERIMENT_CALLS "    register_${object}();\n")
    endforeach()
    configure_file(benchmarks/experiments_main.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/experiments_main.cpp @ONLY)
    add_executable(benchmark_comparisons ${CMAKE_CURRENT_BINARY_DIR}/experiments_main.cpp)
    target_link_libraries(benchmark_comparisons PRIVATE ${experiment_objects} benchmark::benchmark fmt::fmt)
endif()

if(URD_BUILD_TESTS)
    enable_testing()
    find_package(Threads REQUIRED)
    find_package(GTest 1.11.0 REQUIRED COMPONENTS GTest PATHS "${URD_NATIVE_PREFIX}" NO_DEFAULT_PATH)

    # The Store, and typed tests: one per property for every container that declares it, one per behavior of
    # nested Stores. Those below count allocations (tests/allocations.cpp replaces operator new).
    foreach(test properties rows semantics queues pools registry denials views concurrency complexity complexity_verify)
        add_executable(test_${test} tests/${test}.cpp tests/repr/main.cpp)
        target_link_libraries(test_${test} PRIVATE urd::urd GTest::gtest Threads::Threads fmt::fmt)
        add_test(NAME ${test} COMMAND test_${test})
        set_tests_properties(${test} PROPERTIES FAIL_REGULAR_EXPRESSION "Running 0 tests")
    endforeach()
    target_sources(test_properties PRIVATE tests/allocations.cpp)
    # Each container's property tests (tests/container.cpp, the suite of tests/properties.hpp), linked into one program;
    # the gtest main collects the instantiations. benchmarks/requests.json's `tested` lists what each is tested for.
    urd_per_container(KEY tested FIELDS 2 PER_OBJECT 3 PREFIX properties SOURCE ${PROJECT_SOURCE_DIR}/tests/container.cpp
                      DEFINE URD_TESTED LINK GTest::gtest Threads::Threads fmt::fmt
                      TAIL "INSTANTIATE_TYPED_TEST_SUITE_P(@object@, Properties, container_types, urd::test::Names);\n"
                      OUT_OBJECTS container_test_objects)
    add_executable(test_containers tests/repr/main.cpp tests/allocations.cpp)
    target_link_libraries(test_containers PRIVATE ${container_test_objects} urd::urd GTest::gtest Threads::Threads fmt::fmt)
    add_test(NAME containers COMMAND test_containers)
    set_tests_properties(containers PROPERTIES FAIL_REGULAR_EXPRESSION "Running 0 tests")
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME containers_coverage
             COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tests/containers_coverage.py $<TARGET_FILE:test_containers>
                     $<$<NOT:$<BOOL:${URD_WITH_GTL}>>:--without-gtl>)
    # GCC's -Wfree-nonheap-object misfires on a row vector built and compared inside a thread's lambda (a known false
    # positive of its optimizer, not a defect of the code); the other warnings stay on.
    target_compile_options(test_concurrency PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-free-nonheap-object>)
    target_sources(test_rows PRIVATE tests/allocations.cpp)
    target_sources(test_queues PRIVATE tests/allocations.cpp)
    # A Store's behavior, by topic: each file includes the family headers its Stores need and no other.
    foreach(topic rules sets maps multimaps scopes queues pools rows)
        add_executable(test_store_${topic} tests/store/${topic}.cpp tests/repr/main.cpp tests/allocations.cpp)
        target_link_libraries(test_store_${topic} PRIVATE urd::urd GTest::gtest Threads::Threads fmt::fmt)
        add_test(NAME store_${topic} COMMAND test_store_${topic})
    endforeach()
    # The rules are compile-time only.
    set_tests_properties(store_rules PROPERTIES PASS_REGULAR_EXPRESSION "Running 0 tests|PASSED")

    # The resolution table: docs/RESOLUTION.md is generated from the request grid and checked against the registry.
    add_executable(resolution_table tests/resolution_table.cpp)
    target_link_libraries(resolution_table PRIVATE urd::urd fmt::fmt)
    if(URD_WITH_GTL)  # the table documents the full build: without gtl its concurrent rows read none
        add_test(NAME resolution COMMAND resolution_table --check ${PROJECT_SOURCE_DIR}/docs/RESOLUTION.md)
    endif()
    # What serves each request and what each container is measured for (benchmarks/requests.json): one object per
    # group of the grid (tests/requests_group.cpp), so no translation unit walks the whole grid; tests/requests_table.cpp
    # merges them.
    set(URD_REQUEST_GROUPS 21)  # the groups of tests/grid.hpp: requests_table checks the number
    math(EXPR last_group "${URD_REQUEST_GROUPS} - 1")
    set(request_group_objects "")
    foreach(group RANGE ${last_group})
        add_library(requests_group_${group} OBJECT tests/requests_group.cpp)
        target_link_libraries(requests_group_${group} PRIVATE urd::urd fmt::fmt)
        target_include_directories(requests_group_${group} PRIVATE tests benchmarks)
        target_compile_definitions(requests_group_${group} PRIVATE URD_GROUP=${group})
        urd_small_object(requests_group_${group})
        list(APPEND request_group_objects requests_group_${group})
    endforeach()
    add_executable(requests_table tests/requests_table.cpp)
    target_link_libraries(requests_table PRIVATE urd::urd ${request_group_objects} fmt::fmt)
    target_include_directories(requests_table PRIVATE tests benchmarks)
    target_compile_definitions(requests_table PRIVATE URD_GROUPS=${URD_REQUEST_GROUPS})
    if(URD_WITH_GTL)
        add_test(NAME requests COMMAND requests_table --check ${PROJECT_SOURCE_DIR}/benchmarks/requests.json)
    endif()
    # The vocabulary: share/urd/vocabulary.json is generated from the tags and their entries, and checked against them.
    add_executable(vocabulary_table tests/vocabulary_table.cpp)
    target_link_libraries(vocabulary_table PRIVATE urd::urd fmt::fmt)
    add_test(NAME vocabulary COMMAND vocabulary_table --check ${PROJECT_SOURCE_DIR}/share/urd/vocabulary.json)

    # The costs: share/urd/complexity.json is generated from the cost table (tests/complexity.hpp) and checked against it.
    add_executable(complexity_table tests/complexity_table.cpp)
    target_link_libraries(complexity_table PRIVATE urd::urd fmt::fmt)
    if(URD_WITH_GTL)  # as the resolution table: the full build's representations
        add_test(NAME complexity_table COMMAND complexity_table --check ${PROJECT_SOURCE_DIR}/share/urd/complexity.json)
    endif()


    # The measurements behind the claims (benchmarks/record.py): every cell measured and current, every choice
    # holding, the generated part of docs/BENCHMARKS.md current; with the benchmarks built, every cell registered.
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    if(URD_WITH_GTL)
        add_test(NAME benchmark_results COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/benchmarks/record.py check)
        add_test(NAME benchmarks_doc COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/benchmarks/record.py doc --check)
    endif()
    if(URD_BUILD_BENCHMARKS AND URD_WITH_GTL)
        add_test(NAME benchmark_coverage
                 COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/benchmarks/record.py coverage --binary $<TARGET_FILE:benchmark_comparisons>)
    endif()

    # The include root holds the vocabulary and the entry point only.
    add_test(NAME layout COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} -P ${PROJECT_SOURCE_DIR}/tests/layout.cmake)

    # Declarations from the four libraries as Store requests: compiling them is the test.
    add_library(survey OBJECT tests/survey.cpp)
    target_link_libraries(survey PRIVATE urd::urd)

    # Resolving a request completes neither the candidates nor a Store named as the element: compiling it is the test.
    add_library(selection OBJECT tests/selection.cpp)
    target_link_libraries(selection PRIVATE urd::urd)

    # The tests that came with the representations copied from yggdrasil, unchanged but for names.
    foreach(test segmented_vector tree_vector_set tree_bitset_set)
        add_executable(test_repr_${test} tests/repr/containers/${test}.cpp tests/repr/main.cpp)
        target_link_libraries(test_repr_${test} PRIVATE urd::urd GTest::gtest)
        add_test(NAME repr_${test} COMMAND test_repr_${test})
        set_tests_properties(repr_${test} PROPERTIES FAIL_REGULAR_EXPRESSION "Running 0 tests")
    endforeach()

    if(URD_BUILD_EXAMPLES)
        foreach(example sets sequences maps rows dense interning queues scopes structures states)
            add_test(NAME example_${example} COMMAND example_${example})
        endforeach()
    endif()

    # One run of the compiler on a source, syntax only, with the include directories, definitions and options of a
    # target that links urd: the command behind the tests that are a compiler run (they build nothing and run in
    # parallel under ctest -j).
    function(urd_syntax_only out target source)
        set(includes "$<TARGET_PROPERTY:${target},INCLUDE_DIRECTORIES>")
        set(definitions "$<TARGET_PROPERTY:${target},COMPILE_DEFINITIONS>")
        set(${out}
            ${CMAKE_CXX_COMPILER}
            ${CMAKE_CXX20_STANDARD_COMPILE_OPTION}
            "$<$<BOOL:${includes}>:-I$<JOIN:${includes},$<SEMICOLON>-I>>"
            "$<$<BOOL:${definitions}>:-D$<JOIN:${definitions},$<SEMICOLON>-D>>"
            "$<TARGET_PROPERTY:${target},COMPILE_OPTIONS>"
            -fsyntax-only
            ${CMAKE_CURRENT_SOURCE_DIR}/${source}
            PARENT_SCOPE
        )
    endfunction()

    # What compiling a file costs: tests/budget.py runs the compiler on it and fails when its peak memory exceeds
    # the limit for this compiler (time is reported only: it depends on the machine and on what else ctest runs).
    # The limits are the measured numbers plus a margin; a change that exceeds one fails like a lint finding.
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    function(urd_budget name target source limit_gnu limit_clang)
        urd_syntax_only(compile ${target} ${source})
        if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            set(limit ${limit_clang})
        else()
            set(limit ${limit_gnu})
        endif()
        add_test(NAME budget_${name}
                 COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tests/budget.py --limit-mb ${limit} --label ${name} -- ${compile}
                 COMMAND_EXPAND_LISTS)
    endfunction()

    # The bare include (urd/store.hpp and nothing else) and the survey, a file holding many Stores.
    add_executable(test_budget_bare_control EXCLUDE_FROM_ALL tests/budget/bare.cpp)
    target_link_libraries(test_budget_bare_control PRIVATE urd::urd)
    urd_budget(bare test_budget_bare_control tests/budget/bare.cpp 210 210)
    urd_budget(survey survey tests/survey.cpp 720 650)

    # Code that must not compile. Each case is one run of the compiler on the source, syntax only, with the flags
    # of a target that links urd (so the cases build nothing and run in parallel under ctest -j). The control
    # case of each file must compile, so each case fails for its own reason; the cases are skipped when it does not.
    function(urd_compile_fail_cases name source count)
        add_executable(test_${name}_control EXCLUDE_FROM_ALL ${source})
        target_link_libraries(test_${name}_control PRIVATE urd::urd)
        urd_syntax_only(compile test_${name}_control ${source})
        add_test(NAME ${name}_control COMMAND ${compile} COMMAND_EXPAND_LISTS)
        set_tests_properties(${name}_control PROPERTIES FIXTURES_SETUP ${name})
        foreach(case RANGE 1 ${count})
            add_test(NAME ${name}_${case} COMMAND ${compile} -DURD_CASE=${case} COMMAND_EXPAND_LISTS)
            set_tests_properties(${name}_${case} PROPERTIES WILL_FAIL TRUE FIXTURES_REQUIRED ${name})
        endforeach()
    endfunction()

    # A case that must fail with a given message: the error's text, not only the failure, is the test.
    function(urd_compile_fail_expects name case regex)
        set_tests_properties(${name}_${case} PROPERTIES WILL_FAIL FALSE PASS_REGULAR_EXPRESSION "${regex}")
    endfunction()

    urd_compile_fail_cases(compile_fail_store tests/compile_fail_store.cpp 68)
    # A rejected request names its rule (to_spec in properties.hpp); docs/language/index.rst lists them.
    urd_compile_fail_expects(compile_fail_store 4 "two properties answer the same question")
    urd_compile_fail_expects(compile_fail_store 5 "two properties answer the same question")
    urd_compile_fail_expects(compile_fail_store 6 "stable_refs needs frozen too")
    urd_compile_fail_expects(compile_fail_store 7 "so it cannot have key<K>")
    urd_compile_fail_expects(compile_fail_store 9 "at most one key<K>")
    urd_compile_fail_expects(compile_fail_store 10 "every argument must be an urd property")
    urd_compile_fail_expects(compile_fail_store 14 "two properties answer the same question")
    urd_compile_fail_expects(compile_fail_store 24 "a queue changes by push and pop")
    urd_compile_fail_expects(compile_fail_store 27 "is reached by handle, not by key")
    urd_compile_fail_expects(compile_fail_store 28 "orders a sorted Store or a by_priority queue")
    urd_compile_fail_expects(compile_fail_store 29 "breaks ties by the keys' own order")
    urd_compile_fail_expects(compile_fail_store 31 "P is neither")
    urd_compile_fail_expects(compile_fail_store 36 "stores them flat: ask for flat")
    urd_compile_fail_expects(compile_fail_store 37 "ask for handles and unique too")
    urd_compile_fail_expects(compile_fail_store 44 "a queue empties by pop")
    urd_compile_fail_expects(compile_fail_store 45 "a partition groups key<K>s")
    urd_compile_fail_expects(compile_fail_store 46 "ask for by_priority too")
    urd_compile_fail_expects(compile_fail_store 47 "fixed_shape is a sequence of rows")
    urd_compile_fail_expects(compile_fail_store 13 "stable_refs on a sequence or a positional set needs monotone")
    urd_compile_fail_expects(compile_fail_store 50 "stable_refs on a sequence or a positional set needs monotone")
    urd_compile_fail_expects(compile_fail_store 48 "no concurrent sorted container")
    urd_compile_fail_expects(compile_fail_store 49 "no queue is concurrent")
    urd_compile_fail_expects(compile_fail_store 51 "any number of threads may read it")
    urd_compile_fail_expects(compile_fail_store 52 "reclaims needs sparse keys")
    urd_compile_fail_expects(compile_fail_store 53 "ask for dense too")
    urd_compile_fail_expects(compile_fail_store 54 "so no order_by<P> applies")
    urd_compile_fail_expects(compile_fail_store 55 "ask for small or frozen too")
    urd_compile_fail_expects(compile_fail_store 56 "no order holds over both")
    urd_compile_fail_expects(compile_fail_store 57 "single-threaded")
    # A request no representation serves names the culprits (registry.hpp).
    urd_compile_fail_expects(compile_fail_store 8 "culprits<urd::tag::dense>")
    urd_compile_fail_expects(compile_fail_store 32 "culprits<>, false")
    urd_compile_fail_expects(compile_fail_store 33 "culprits<>, false")
    urd_compile_fail_expects(compile_fail_store 40 "culprits<urd::tag::layered>")
    urd_compile_fail_expects(compile_fail_store 41 "culprits<urd::tag::layered, urd::tag::structural_sharing>")
    urd_compile_fail_expects(compile_fail_store 43 "culprits<urd::tag::concurrent, urd::tag::layered>")
    # A missing operation names the property that rules it out (store/denials.hpp).
    urd_compile_fail_expects(compile_fail_store 1 "a set has no positions")
    urd_compile_fail_expects(compile_fail_store 2 "insert: the Store is frozen")
    urd_compile_fail_expects(compile_fail_store 3 "pop_back: the Store is monotone")
    urd_compile_fail_expects(compile_fail_store 26 "a pool is reached by handle")
    urd_compile_fail_expects(compile_fail_store 58 "operator==: a layered Store.*s contents include its parent")
    urd_compile_fail_expects(compile_fail_store 59 "erase: the pool is monotone")
    urd_compile_fail_expects(compile_fail_store 60 "erase: a pool with structural_sharing shares nodes")
    urd_compile_fail_expects(compile_fail_store 61 "erase: a concurrent pool is read without a lock")
    urd_compile_fail_expects(compile_fail_store 62 "erase: a layered pool.*s handles are continued")
    urd_compile_fail_expects(compile_fail_store 63 "front/back: dense keys are found by scanning their range")
    urd_compile_fail_expects(compile_fail_store 64 "begin/end: a bucket queue")
    urd_compile_fail_expects(compile_fail_store 65 "back: only a fifo queue has a back")
    urd_compile_fail_expects(compile_fail_store 66 "operator.*: a queue is reached by its front or top; ask for handles")
    urd_compile_fail_expects(compile_fail_store 67 "count: a partition holds each element once")
    urd_compile_fail_expects(compile_fail_store 68 "begin/end: a layered pool reaches each record")
    # A Store whose family header is missing names it (store.hpp, complete_representation).
    urd_compile_fail_cases(compile_fail_include tests/compile_fail_include.cpp 13)
    if(URD_WITH_GTL)  # the parallel family exists only with gtl; without, its case has nothing to leave out
        urd_compile_fail_expects(compile_fail_include 6 "include<.*\"urd/use/parallel[.]hpp\"")
    else()
        set_tests_properties(compile_fail_include_6 PROPERTIES DISABLED TRUE)
    endif()
    urd_compile_fail_expects(compile_fail_include 1 "include<.*\"urd/use/records[.]hpp\"")
    urd_compile_fail_expects(compile_fail_include 2 "include<.*\"urd/use/dense[.]hpp\"")
    urd_compile_fail_expects(compile_fail_include 3 "include<.*\"urd/use/sequences[.]hpp\"")
    urd_compile_fail_expects(compile_fail_include 4 "include<.*\"urd/use/hash[.]hpp\"")
    urd_compile_fail_expects(compile_fail_include 5 "include<.*\"urd/use/contiguous[.]hpp\"")
    urd_compile_fail_expects(compile_fail_include 7 "include<.*\"urd/use/nodes[.]hpp\"")
    urd_compile_fail_expects(compile_fail_include 8 "include<.*\"urd/use/queues[.]hpp\"")
    urd_compile_fail_expects(compile_fail_include 9 "include<.*\"urd/use/pools[.]hpp\"")
    urd_compile_fail_expects(compile_fail_include 10 "include<.*\"urd/use/hash[.]hpp\".*include<.*\"urd/use/layers[.]hpp\"")
    urd_compile_fail_expects(compile_fail_include 11 "include<.*\"urd/use/scopes[.]hpp\"")
    urd_compile_fail_expects(compile_fail_include 12 "include<.*\"urd/use/hash[.]hpp\"")
    urd_compile_fail_expects(compile_fail_include 13 "include<.*\"urd/use/nodes[.]hpp\"")
    if(NOT URD_WITH_GTL)
        urd_compile_fail_cases(compile_fail_gtl tests/compile_fail_gtl.cpp 2)
        urd_compile_fail_expects(compile_fail_gtl 1 "URD_WITH_GTL")
        urd_compile_fail_expects(compile_fail_gtl 2 "URD_WITH_GTL")
    endif()

    # A consumer: find_package(urd) against this build tree's export, then its build. What a user of the package sees.
    add_test(
        NAME consumer_configure
        COMMAND ${CMAKE_COMMAND} -S ${PROJECT_SOURCE_DIR}/tests/consumer -B ${CMAKE_BINARY_DIR}/consumer
                -Durd_DIR=${CMAKE_BINARY_DIR} -DCMAKE_PREFIX_PATH=${URD_NATIVE_PREFIX}
                -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} -DCMAKE_BUILD_TYPE=$<CONFIG>
    )
    set_tests_properties(consumer_configure PROPERTIES FIXTURES_SETUP consumer)
    add_test(NAME consumer_build COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}/consumer)
    set_tests_properties(consumer_build PROPERTIES FIXTURES_REQUIRED consumer)
endif()
