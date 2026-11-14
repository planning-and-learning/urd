# A consumer of the urd package, built by the consumer_* tests against the build tree's export (urd_DIR) with the
# dependencies' prefix on CMAKE_PREFIX_PATH.
cmake_minimum_required(VERSION 3.21)
project(urd_consumer LANGUAGES CXX)

find_package(urd REQUIRED CONFIG)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE urd::urd)
