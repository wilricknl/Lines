cmake_minimum_required(VERSION 3.21)
project(lines
        VERSION 1.0.0
        DESCRIPTION "6 different implementations of doing wide line rendering in OpenGL"
        LANGUAGES C)

find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/extern)

add_executable(lines main.c extern/glad.c)
target_link_libraries(lines glfw OpenGL::GL)
