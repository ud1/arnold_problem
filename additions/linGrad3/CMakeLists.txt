cmake_minimum_required(VERSION 3.26)
project(linGrad3)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(linGrad3 linGrad3.cpp)

target_compile_options(linGrad3 PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
)
