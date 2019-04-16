cmake_minimum_required(VERSION 3.0)

project(nrfserdfu LANGUAGES C)

add_executable(nrfserdfu main.c)

install(TARGETS nrfserdfu RUNTIME DESTINATION bin)
