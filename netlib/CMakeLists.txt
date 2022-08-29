cmake_minimum_required(VERSION 3.5.1)

project(netlib)

set(SOURCES netlib.cpp)

add_library(${PROJECT_NAME} ${SOURCES})

