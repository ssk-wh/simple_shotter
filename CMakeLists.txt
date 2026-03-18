cmake_minimum_required(VERSION 3.16)
project(SimpleShotter VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Svg)
if(UNIX AND NOT APPLE)
    find_package(Qt5 REQUIRED COMPONENTS X11Extras)
endif()

if(MSVC)
    add_compile_options(/W4 /utf-8)
    add_compile_definitions(UNICODE _UNICODE)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(src)
add_subdirectory(tests)
