cmake_minimum_required(VERSION 2.8.9)
project(navit_binfile_extractor)

#include_directories(include)

file(GLOB SOURCES "src/*.c")

add_executable(navit_binfile_extractor ${SOURCES})
target_link_libraries(navit_binfile_extractor -lm)
