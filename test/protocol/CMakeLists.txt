cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(protocol)

zephyr_include_directories($ENV{APPLICATION_DIR}/src)
include_directories($ENV{APPLICATION_DIR}/src)

set(SOURCES
    test_protocol.c
    $ENV{APPLICATION_DIR}/src/protocol.c
    $ENV{APPLICATION_DIR}/src/protocol.h
    $ENV{APPLICATION_DIR}/src/serialise.c
    $ENV{APPLICATION_DIR}/src/serialise.h
    $ENV{APPLICATION_DIR}/src/commands.c
    $ENV{APPLICATION_DIR}/src/commands.h
)

target_sources(app PRIVATE ${SOURCES})