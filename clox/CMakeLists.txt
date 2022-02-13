cmake_minimum_required(VERSION 3.21)
project(clox C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(clox
        chunk.c
        chunk.h
        common.h
        main.c memory.h cmake-build-debug/memory.c debug.c debug.h value.c value.h)
