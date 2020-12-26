cmake_minimum_required(VERSION 3.5)

add_compile_options(-Wall)

add_library(shortrecv SHARED lib.c)
target_link_libraries(shortrecv dl)
