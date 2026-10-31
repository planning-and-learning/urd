# The examples (URD_BUILD_EXAMPLES): each a program of its own, and a test where the tests are built.

find_package(Threads REQUIRED)
foreach(example sets sequences maps rows dense interning queues scopes structures states)
    add_executable(example_${example} ${example}.cpp)
    target_link_libraries(example_${example} PRIVATE urd::urd Threads::Threads)
    target_include_directories(example_${example} PRIVATE "${PROJECT_SOURCE_DIR}/tests")
endforeach()

# Each example runs as a test where the tests are built.
if(URD_BUILD_TESTS)
    foreach(example sets sequences maps rows dense interning queues scopes structures states)
        add_test(NAME example_${example} COMMAND example_${example})
    endforeach()
endif()
