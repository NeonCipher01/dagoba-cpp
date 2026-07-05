cmake_minimum_required(VERSION 3.16)
project(dagoba_cpp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra)

add_library(dagoba
    src/graph.cpp
    src/query.cpp
)
target_include_directories(dagoba PUBLIC include)

add_executable(dagoba_demo src/main.cpp)
target_link_libraries(dagoba_demo PRIVATE dagoba)

enable_testing()
add_subdirectory(tests)
