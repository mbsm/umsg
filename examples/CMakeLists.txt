cmake_minimum_required(VERSION 3.14)
project(umsg_examples)

# If built standalone, define umsg target
if(NOT TARGET umsg)
    add_library(umsg INTERFACE)
    target_include_directories(umsg INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
    target_compile_features(umsg INTERFACE cxx_std_11)
endif()

# Helper to add examples
function(add_umsg_example name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE umsg)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Common)
endfunction()

add_umsg_example(PosixSerialLedController PosixSerialLedController/main.cpp)
add_umsg_example(PosixUdpLedController PosixUdpLedController/main.cpp)
add_umsg_example(PosixTcpSensor PosixTcpSensor/main.cpp)
