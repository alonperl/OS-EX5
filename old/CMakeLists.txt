cmake_minimum_required(VERSION 3.2)
project(OS_ex5)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES main.cpp)
add_executable(OS_ex5 ${SOURCE_FILES} NewCppClass.cpp NewCppClass.h srftp.h srftp.cpp)