cmake_minimum_required(VERSION 3.28)
project(Objects-Tests)

include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

add_executable(objectTests
        ObjectTests.cpp
        StringTests.cpp
        NumberTests.cpp
        KVOTests.cpp)

set(RESOURCES
        manifest.json)

rayne_copy_resources(${RESOURCES} objectTests)

target_link_libraries(objectTests ${gtest_LIBRARIES})
target_link_libraries(objectTests Rayne)
