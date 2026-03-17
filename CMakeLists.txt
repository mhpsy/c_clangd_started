cmake_minimum_required(VERSION 3.15)
project(ch347_run_st7789 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

add_executable(ch347_run_st7789
    src/main.c
    src/ch347.c
    src/st7789.c
)

target_include_directories(ch347_run_st7789 PRIVATE
    src
    ${LIBUSB_INCLUDE_DIRS}
)

target_compile_options(ch347_run_st7789 PRIVATE -Wall -Wextra -g -D_GNU_SOURCE)
target_link_libraries(ch347_run_st7789 PRIVATE ${LIBUSB_LIBRARIES})
