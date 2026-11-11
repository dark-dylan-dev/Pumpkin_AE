add_executable(Runtime main.cpp Item.cpp)

# Binary properties
set_target_properties(Runtime PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_link_libraries(Runtime PRIVATE Pumpkin_AE)
