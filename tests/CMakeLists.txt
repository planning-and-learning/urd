# The tests (URD_BUILD_TESTS): the Store's behavior, each container's properties, the generated files and their
# checks, the cost of compiling, code that must not compile, and a consumer of the package.

find_package(Threads REQUIRED)
find_package(GTest 1.11.0 REQUIRED COMPONENTS GTest PATHS "${URD_NATIVE_PREFIX}" NO_DEFAULT_PATH)

# The Store, and typed tests: one per property for every container that declares it, one per behavior of
# nested Stores. Those below count allocations (they link urd::count_allocations, which replaces operator new).
foreach(test properties rows semantics queues pools views children concurrency shortfall)
    add_executable(test_${test} ${test}.cpp repr/main.cpp)
    target_link_libraries(test_${test} PRIVATE urd::urd GTest::gtest Threads::Threads fmt::fmt)
    add_test(NAME ${test} COMMAND test_${test})
    set_tests_properties(${test} PROPERTIES FAIL_REGULAR_EXPRESSION "Running 0 tests")
endforeach()
target_link_libraries(test_properties PRIVATE urd::count_allocations)
# Each container's property tests (tests/container.cpp, the suite of tests/properties.hpp), linked into one program;
# the gtest main collects the instantiations. benchmarks/requests.json's `tested` lists what each is tested for.
urd_per_container(KEY tested FIELDS 2 PER_OBJECT 3 PREFIX properties SOURCE ${PROJECT_SOURCE_DIR}/tests/container.cpp
                  DEFINE URD_TESTED LINK GTest::gtest Threads::Threads fmt::fmt
                  TAIL "INSTANTIATE_TYPED_TEST_SUITE_P(@object@, Properties, container_types, urd::test::Names);\n"
                  OUT_OBJECTS container_test_objects)
add_executable(test_containers repr/main.cpp)
target_link_libraries(test_containers PRIVATE ${container_test_objects} urd::urd urd::count_allocations GTest::gtest Threads::Threads fmt::fmt)
add_test(NAME containers COMMAND test_containers)
set_tests_properties(containers PROPERTIES FAIL_REGULAR_EXPRESSION "Running 0 tests")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
# GCC's -Wfree-nonheap-object misfires on a row vector built and compared inside a thread's lambda (a known false
# positive of its optimizer, not a defect of the code); the other warnings stay on.
target_compile_options(test_concurrency PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-free-nonheap-object>)
target_link_libraries(test_rows PRIVATE urd::count_allocations)
target_link_libraries(test_queues PRIVATE urd::count_allocations)
# A Store's behavior, by topic (every tests/store/*.cpp): each file includes the headers of the containers its Stores
# resolve to.
file(GLOB store_topics CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/tests/store/*.cpp)
list(TRANSFORM store_topics REPLACE ".*/([^/]+)\\.cpp$" "\\1")
foreach(topic ${store_topics})
    add_executable(test_store_${topic} store/${topic}.cpp repr/main.cpp)
    target_link_libraries(test_store_${topic} PRIVATE urd::urd urd::count_allocations GTest::gtest Threads::Threads fmt::fmt)
    add_test(NAME store_${topic} COMMAND test_store_${topic})
endforeach()
# The rules are compile-time only.
set_tests_properties(store_rules PROPERTIES PASS_REGULAR_EXPRESSION "Running 0 tests|PASSED")

# The nanobind casters (urd/python/): round trips through a module, where nanobind is found (-Dnanobind_DIR=...).
find_package(Python 3.11 COMPONENTS Interpreter Development.Module)
find_package(nanobind CONFIG QUIET)
if(nanobind_FOUND AND Python_FOUND)
    nanobind_add_module(urd_casters_test NB_SUPPRESS_WARNINGS python/casters_module.cpp python/caster_headers.cpp)
    target_link_libraries(urd_casters_test PRIVATE urd::urd)
    add_test(NAME python_casters COMMAND ${Python_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/python/casters.py $<TARGET_FILE:urd_casters_test>)
    nanobind_add_module(urd_views_test NB_SUPPRESS_WARNINGS python/views_module.cpp)
    target_link_libraries(urd_views_test PRIVATE urd::urd)
    add_test(NAME python_views COMMAND ${Python_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/python/views.py $<TARGET_FILE:urd_views_test>)
    set_tests_properties(python_views PROPERTIES PASS_REGULAR_EXPRESSION "PASSED")
    nanobind_add_module(urd_graphs_test NB_SUPPRESS_WARNINGS python/graphs_module.cpp)
    target_link_libraries(urd_graphs_test PRIVATE urd::urd)
    add_test(NAME python_graphs COMMAND ${Python_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/python/graphs.py $<TARGET_FILE:urd_graphs_test>)
    set_tests_properties(python_graphs PROPERTIES PASS_REGULAR_EXPRESSION "PASSED")
    # nanobind's own sources are compiled into this build: they are not held to urd's warnings.
    set_target_properties(nanobind-static PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
else()
    message(STATUS "urd: nanobind not found; the caster test (tests/python/casters.py) is not built")
endif()

# The resolution table: docs/RESOLUTION.md is generated from the request grid and checked against the registry.
add_executable(resolution_table resolution_table.cpp)
target_link_libraries(resolution_table PRIVATE urd::urd fmt::fmt)
add_test(NAME resolution COMMAND resolution_table --check ${PROJECT_SOURCE_DIR}/docs/RESOLUTION.md)
# What serves each request and what each container is measured for (benchmarks/requests.json): one object per
# group of the grid (requests_group.cpp), so no translation unit walks the whole grid; requests_table.cpp
# merges them.
# The groups of tests/grid.hpp, counted from it (one `group<"` each): requests_table checks the number.
file(STRINGS ${PROJECT_SOURCE_DIR}/tests/grid.hpp grid_groups REGEX "group<\"")
list(LENGTH grid_groups URD_REQUEST_GROUPS)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/tests/grid.hpp)
math(EXPR last_group "${URD_REQUEST_GROUPS} - 1")
set(request_group_objects "")
foreach(group RANGE ${last_group})
    add_library(requests_group_${group} OBJECT requests_group.cpp)
    target_link_libraries(requests_group_${group} PRIVATE urd::urd fmt::fmt)
    target_include_directories(requests_group_${group} PRIVATE ${PROJECT_SOURCE_DIR}/tests ${PROJECT_SOURCE_DIR}/benchmarks)
    target_compile_definitions(requests_group_${group} PRIVATE URD_GROUP=${group})
    urd_small_object(requests_group_${group})
    list(APPEND request_group_objects requests_group_${group})
endforeach()
add_executable(requests_table requests_table.cpp)
target_link_libraries(requests_table PRIVATE urd::urd ${request_group_objects} fmt::fmt)
target_include_directories(requests_table PRIVATE ${PROJECT_SOURCE_DIR}/tests ${PROJECT_SOURCE_DIR}/benchmarks)
target_compile_definitions(requests_table PRIVATE URD_GROUPS=${URD_REQUEST_GROUPS})
add_test(NAME requests COMMAND requests_table --check ${PROJECT_SOURCE_DIR}/benchmarks/requests.json)
# The vocabulary: share/urd/vocabulary.json is generated from the tags and their entries, and checked against them.
add_executable(vocabulary_table vocabulary_table.cpp)
target_link_libraries(vocabulary_table PRIVATE urd::urd fmt::fmt)
add_test(NAME vocabulary COMMAND vocabulary_table --check ${PROJECT_SOURCE_DIR}/share/urd/vocabulary.json)

# The costs: share/urd/complexity.json is generated from the cost table (tests/complexity.hpp) and checked against it.
add_executable(complexity_table complexity_table.cpp)
target_link_libraries(complexity_table PRIVATE urd::urd fmt::fmt)
add_test(NAME complexity_table COMMAND complexity_table --check ${PROJECT_SOURCE_DIR}/share/urd/complexity.json)

# What urd exposes publicly (tests/exposure.py, AGENTS.md rules F7 and F8), read with clang's parser: where the Python
# that runs the tests has libclang (pip install -e '.[test]'), from the flags this build compiles with.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND ${Python3_EXECUTABLE} -c "import clang.cindex" RESULT_VARIABLE urd_libclang OUTPUT_QUIET ERROR_QUIET)
if(urd_libclang EQUAL 0)
    add_test(NAME exposure COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tests/exposure.py --compile-commands
                                   ${PROJECT_BINARY_DIR}/compile_commands.json)
else()
    message(STATUS "urd: ${Python3_EXECUTABLE} has no libclang; the exposure check (tests/exposure.py) is not run")
endif()


# The measurements behind the claims (benchmarks/record.py): every cell measured and current, every choice
# holding, the generated part of docs/BENCHMARKS.md current; with the benchmarks built, every cell registered.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME benchmark_results COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/benchmarks/record.py check)
add_test(NAME benchmarks_doc COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/benchmarks/record.py doc --check)
if(URD_BUILD_BENCHMARKS)
    add_test(NAME benchmark_coverage
             COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/benchmarks/record.py coverage --binary $<TARGET_FILE:benchmark_comparisons>)
endif()

# The glue names no specific tag: what a tag means is declared beside it, in the rules or by a container (tests/generic.py).
add_test(NAME generic COMMAND Python3::Interpreter -I ${PROJECT_SOURCE_DIR}/tests/generic.py ${PROJECT_SOURCE_DIR})

# Tests of the containers in repr/containers.
foreach(test segmented_vector tree_vector_set tree_bitset_set bitset_span bit)
    add_executable(test_repr_${test} repr/containers/${test}.cpp repr/main.cpp)
    target_link_libraries(test_repr_${test} PRIVATE urd::urd GTest::gtest)
    add_test(NAME repr_${test} COMMAND test_repr_${test})
    set_tests_properties(repr_${test} PROPERTIES FAIL_REGULAR_EXPRESSION "Running 0 tests")
endforeach()

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

# The bare include (urd/store.hpp and nothing else).
add_executable(test_budget_bare_control EXCLUDE_FROM_ALL budget/bare.cpp)
target_link_libraries(test_budget_bare_control PRIVATE urd::urd)
urd_budget(bare test_budget_bare_control budget/bare.cpp 210 210)

# A file holding a Store pays for its own container and for no other registered one: tests/scaling.py
# compiles it with Clang's -ftime-trace and fails on an instantiation of a container its Store did not choose. Store<set<int>>
# resolves to gtl's flat hash set.
find_program(URD_CLANG_FOR_TRACE NAMES clang++ clang++-18)
if(URD_CLANG_FOR_TRACE)
    add_executable(test_scaling_one_control EXCLUDE_FROM_ALL scaling/one.cpp)
    target_link_libraries(test_scaling_one_control PRIVATE urd::urd)
    urd_syntax_only(compile test_scaling_one_control scaling/one.cpp)
    add_test(NAME scaling
             COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tests/scaling.py --clang ${URD_CLANG_FOR_TRACE}
                     --allow "gtl::flat_hash_set<" -- ${compile}
             COMMAND_EXPAND_LISTS)
endif()

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

urd_compile_fail_cases(compile_fail_store compile_fail_store.cpp 64)
# A rejected request names its rule (to_spec in urd/spec.hpp); docs/language/index.rst lists them.
urd_compile_fail_expects(compile_fail_store 4 "the request contradicts itself")
urd_compile_fail_expects(compile_fail_store 5 "two properties answer the same question")
urd_compile_fail_expects(compile_fail_store 6 "no container serves this request")
urd_compile_fail_expects(compile_fail_store 8 "the first argument is the kind")
urd_compile_fail_expects(compile_fail_store 9 "every argument must be an urd tag")
urd_compile_fail_expects(compile_fail_store 13 "two properties answer the same question")
urd_compile_fail_expects(compile_fail_store 24 "means nothing for this kind of Store")
urd_compile_fail_expects(compile_fail_store 25 "the request contradicts itself")
urd_compile_fail_expects(compile_fail_store 62 "the request contradicts itself")
urd_compile_fail_expects(compile_fail_store 26 "the request contradicts itself")
urd_compile_fail_expects(compile_fail_store 27 "C compares two elements")
urd_compile_fail_expects(compile_fail_store 32 "no container serves this request")
urd_compile_fail_expects(compile_fail_store 33 "means nothing for this kind of Store")
urd_compile_fail_expects(compile_fail_store 39 "means nothing for this kind of Store")
urd_compile_fail_expects(compile_fail_store 40 "no container serves this request")
urd_compile_fail_expects(compile_fail_store 12 "changes_in_the_middle")
urd_compile_fail_expects(compile_fail_store 60 "means nothing for this kind of Store")
urd_compile_fail_expects(compile_fail_store 43 "no container serves this request")
urd_compile_fail_expects(compile_fail_store 41 "no container serves this request")
urd_compile_fail_expects(compile_fail_store 42 "no container serves this request")
urd_compile_fail_expects(compile_fail_store 44 "no container serves this request")
urd_compile_fail_expects(compile_fail_store 45 "no container serves this request")
urd_compile_fail_expects(compile_fail_store 46 "no container serves this request")
urd_compile_fail_expects(compile_fail_store 47 "no container serves this request")
urd_compile_fail_expects(compile_fail_store 48 "no container serves this request")
# A request no container serves (urd/registry/select.hpp), the first with the whole sentence an agent reads. The
# cases without a message fail by the language (a member the Store does not have), not by an urd assertion.
urd_compile_fail_expects(compile_fail_store 7 "no container serves this request: drop or change a tag [(]urd-search's why_not")
urd_compile_fail_expects(compile_fail_store 19 "no container serves this request")
urd_compile_fail_expects(compile_fail_store 34 "no container serves this request")
urd_compile_fail_expects(compile_fail_store 28 "no container serves this request")
urd_compile_fail_expects(compile_fail_store 29 "no container serves this request")
urd_compile_fail_expects(compile_fail_store 36 "no container serves this request")
urd_compile_fail_expects(compile_fail_store 37 "no container serves this request")
urd_compile_fail_expects(compile_fail_store 38 "a tree pool hands out its own handles")
# A Store whose container's header is missing names it, with its part's for a wrapper, the part first (urd/registry/select.hpp,
# chosen_type, missing_headers_for).
urd_compile_fail_cases(compile_fail_include compile_fail_include.cpp 13)
# Clang prints at most 32 characters of a string in a diagnostic, so the longer header names are matched by their
# first 32.
urd_compile_fail_expects(compile_fail_include 5 "include<.*\"urd/repr/values/parallel_hash_s")
urd_compile_fail_expects(compile_fail_include 1 "include<.*\"urd/repr/records/arena[.]hpp\"")
urd_compile_fail_expects(compile_fail_include 2 "include<.*\"urd/repr/values/dense_set[.]hpp\"")
urd_compile_fail_expects(compile_fail_include 3 "include<.*\"urd/repr/values/unordered_flat_s")
urd_compile_fail_expects(compile_fail_include 4 "include<.*\"urd/repr/values/flat_set[.]hpp\"")
urd_compile_fail_expects(compile_fail_include 6 "include<.*\"urd/repr/queues/queue[.]hpp\"")
urd_compile_fail_expects(compile_fail_include 7 "include<.*\"urd/repr/pools/pools[.]hpp\"")
urd_compile_fail_expects(compile_fail_include 8 "include<.*\"urd/repr/maps/index_map[.]hpp\".*include<.*\"urd/repr/wrappers/layer[.]hpp\"")
urd_compile_fail_expects(compile_fail_include 9 "include<.*\"urd/repr/wrappers/layer[.]hpp\"")
urd_compile_fail_expects(compile_fail_include 10 "include<.*\"urd/repr/wrappers/scoped[.]hpp\"")
urd_compile_fail_expects(compile_fail_include 11 "include<.*\"urd/repr/values/unordered_flat_s")
urd_compile_fail_expects(compile_fail_include 12 "include<.*\"urd/repr/values/unordered_flat_s")
urd_compile_fail_expects(compile_fail_include 13 "include<.*\"urd/repr/values/unordered_flat_s")

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
