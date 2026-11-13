cmake_minimum_required(VERSION 3.30)
set(AE_VERSION 1)
project(Pumpkin_AE VERSION ${AE_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory(Library)
add_subdirectory(Runtime)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(Pumpkin_AE PRIVATE stdc++exp)
    target_link_libraries(Runtime PRIVATE stdc++exp)
endif()
