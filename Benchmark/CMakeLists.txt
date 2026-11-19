add_executable(Benchmark main.cpp)

# Binary properties
set_target_properties(Benchmark PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_link_libraries(Benchmark PRIVATE Pumpkin_AE)
target_include_directories(Benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
