add_executable(UnitTests main.cpp)

# Google Test
include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.18.0
)

# See: https://github.com/google/googletest/blob/main/docs/quickstart-cmake.md
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

message(STATUS "Googletest: Version 1.18.0")

enable_testing()

# Binary properties
set_target_properties(UnitTests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_link_libraries(UnitTests PRIVATE Pumpkin_AE)
target_link_libraries(UnitTests PRIVATE GTest::gtest_main)
target_include_directories(UnitTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE TEST_SOURCES "*.cpp")
target_sources(UnitTests PRIVATE ${TEST_SOURCES})
