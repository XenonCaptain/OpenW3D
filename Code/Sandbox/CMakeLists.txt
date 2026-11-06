cmake_minimum_required(VERSION 3.25)
project(OpenW3DSandboxRules LANGUAGES CXX)
include(CTest)

# Standalone sandbox rules, suitable for both client and dedicated server.
add_library(openw3d_sandbox STATIC sandbox.cpp sandbox.h)
target_compile_features(openw3d_sandbox PUBLIC cxx_std_20)
target_include_directories(openw3d_sandbox PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(BUILD_TESTING)
    add_executable(openw3d_sandbox_tests sandbox_tests.cpp)
    target_link_libraries(openw3d_sandbox_tests PRIVATE openw3d_sandbox)
    add_test(NAME openw3d_sandbox_rules COMMAND openw3d_sandbox_tests)
endif()
