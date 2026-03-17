cmake_minimum_required(VERSION 3.15)
project(hello C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(hello src/main.c)
target_compile_options(hello PRIVATE -Wall -Wextra -g)
