cmake_minimum_required(VERSION 3.14)

# Create the test executable
add_executable(umsg_tests
    test_main.cpp
    test_cobs.cpp
    test_crc32.cpp
    test_framer.cpp
    test_marshal.cpp
    test_node.cpp
    test_router.cpp
)

# Link against the main library
target_link_libraries(umsg_tests PRIVATE umsg)

# Compiler warnings for tests
if(MSVC)
    target_compile_options(umsg_tests PRIVATE /W4 /WX)
else()
    target_compile_options(umsg_tests PRIVATE -Wall -Wextra -Werror -pedantic)
endif()

# Register with CTest
add_test(NAME AllTests COMMAND umsg_tests)
