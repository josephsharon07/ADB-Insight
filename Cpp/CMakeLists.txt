cmake_minimum_required(VERSION 3.10)
project(adb_insight_cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CMake policy for reproducible builds
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# Find or download required libraries
include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

# cpp-httplib (header-only from master branch)
FetchContent_Declare(cpp_httplib
    URL https://codeload.github.com/yhirose/cpp-httplib/tar.gz/master
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# nlohmann_json
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(cpp_httplib nlohmann_json)

# Main executable
add_executable(adb_insight
    src/main.cpp
    src/adb_utils.cpp
    src/parsers.cpp
)

target_include_directories(adb_insight PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${cpp_httplib_SOURCE_DIR}
)
target_link_libraries(adb_insight PRIVATE nlohmann_json::nlohmann_json)

# Enable all warnings
if(MSVC)
    target_compile_options(adb_insight PRIVATE /W4)
else()
    target_compile_options(adb_insight PRIVATE -Wall -Wextra -Wpedantic)
endif()
