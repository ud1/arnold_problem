cmake_minimum_required(VERSION 3.26)
project(arn_util)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
FetchContent_Declare(
        sqlite3
        URL https://www.sqlite.org/2026/sqlite-amalgamation-3510200.zip
        URL_HASH SHA3_256=9a9dd4eef7a97809bfacd84a7db5080a5c0eff7aaf1fc1aca20a6dc9a0c26f96
)
FetchContent_MakeAvailable(sqlite3)

add_executable(arn_util main.cpp
        db.cpp
        omatrix.cpp
        omatrix.hpp
        bits.hpp
        db.hpp
        conf.hpp
        graceful_stop.hpp
        graceful_stop.cpp)
#target_include_directories(arn_util PUBLIC "${sqlite_SOURCE_DIR}")
target_link_libraries(arn_util PRIVATE sqlite3)
