cmake_minimum_required(VERSION 3.14)

project(umsg VERSION 0.1.0 LANGUAGES CXX)

# --- Main Library Target ---
# Header-only library interface
add_library(umsg INTERFACE)

# Expose 'src' as the include root.
# This allows users to #include <umsg/umsg.h>
target_include_directories(umsg INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

# Enforce C++11 requirement for consumers
target_compile_features(umsg INTERFACE cxx_std_11)

# --- Testing ---
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()
