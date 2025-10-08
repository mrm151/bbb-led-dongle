cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(timer)

zephyr_include_directories($ENV{APPLICATION_DIR}/src)
include_directories($ENV{APPLICATION_DIR}/src)

set(SOURCES
    test_timer.c
    $ENV{APPLICATION_DIR}/src/timer.c
    $ENV{APPLICATION_DIR}/src/timer.h
)

target_sources(app PRIVATE ${SOURCES})