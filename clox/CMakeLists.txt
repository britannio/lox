cmake_minimum_required(VERSION 3.21)
project(clox C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(clox
        main.c
        chunk.c chunk.h
        common.h
        memory.h memory.c
        debug.c debug.h
        value.c value.h
        vm.h vm.c
        compiler.c compiler.h
        scanner.c scanner.h)
