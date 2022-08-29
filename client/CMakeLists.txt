cmake_minimum_required(VERSION 3.5.1)

project(client)

set(SOURCES main.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})

find_package(Threads REQUIRED)

target_include_directories(${PROJECT_NAME} PRIVATE ../netlib)
target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_THREAD_LIBS_INIT} netlib crypto)
