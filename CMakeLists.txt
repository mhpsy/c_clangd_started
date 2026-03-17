cmake_minimum_required(VERSION 3.15)
project(ch347_run_st7789 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(ch347_run_st7789 src/main.c)
target_compile_options(ch347_run_st7789 PRIVATE -Wall -Wextra -g)
