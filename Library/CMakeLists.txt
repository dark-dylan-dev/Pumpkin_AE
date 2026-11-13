add_library(Pumpkin_AE SHARED)

# Library properties
set_target_properties(Pumpkin_AE PROPERTIES OUTPUT_NAME Pumpkin_AE)
set_target_properties(Pumpkin_AE PROPERTIES PREFIX "")
set_target_properties(Pumpkin_AE PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# DLL Properties
target_compile_definitions(Pumpkin_AE PRIVATE PUMPKIN_AE_BUILD)
target_compile_definitions(Pumpkin_AE PRIVATE PUMPKIN_AE_VERSION=${AE_VERSION})
set_target_properties(Pumpkin_AE PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
)

# User include folder
target_include_directories(Pumpkin_AE PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Include)

# Sources
file(GLOB_RECURSE AE_LIBRARY_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
target_sources(Pumpkin_AE PRIVATE ${AE_LIBRARY_SOURCES})
