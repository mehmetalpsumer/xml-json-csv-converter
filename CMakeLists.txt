cmake_minimum_required(VERSION 3.6)
project(untitled)

set(CMAKE_C_STANDARD 99)
find_package(LibXml2)
include_directories(${LIBXML2_INCLUDE_DIR})
set(SOURCE_FILES main.c)
add_executable(untitled ${SOURCE_FILES})