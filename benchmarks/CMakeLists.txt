# The benchmarks (URD_BUILD_BENCHMARKS; run them in a Release build): named scenarios, and every container measured
# on its own (benchmarks/record.py records and judges them).

find_package(benchmark REQUIRED PATHS "${URD_NATIVE_PREFIX}" NO_DEFAULT_PATH)
add_executable(benchmark_containers containers.cpp)
target_link_libraries(benchmark_containers PRIVATE urd::urd benchmark::benchmark)
add_executable(benchmark_pools pools.cpp)
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
configure_file(experiments_main.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/experiments_main.cpp @ONLY)
add_executable(benchmark_comparisons ${CMAKE_CURRENT_BINARY_DIR}/experiments_main.cpp)
target_link_libraries(benchmark_comparisons PRIVATE ${experiment_objects} benchmark::benchmark fmt::fmt)
