cmake_minimum_required(VERSION 3.5.1)

project(test)
message("Building ${PROJECT_NAME}...") 

#set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(netlib)
add_subdirectory(client)
add_subdirectory(server)
