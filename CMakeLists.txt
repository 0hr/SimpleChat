cmake_minimum_required(VERSION 4.0)
project(SimpleChat)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

add_subdirectory(src/UI)
add_subdirectory(src/Controllers)
add_subdirectory(src/Core)

add_executable(SimpleChat
        src/main.cpp)

target_link_libraries(SimpleChat Core UI Controllers Qt6::Widgets Qt6::Network)