cmake_minimum_required(VERSION 3.0)

project(nrfserdfu LANGUAGES C)

find_package(ZLIB)

add_executable(nrfserdfu main.c log.c serialtty.c dfu.c slip.c)
target_link_libraries(nrfserdfu ${ZLIB_LIBRARIES})

install(TARGETS nrfserdfu RUNTIME DESTINATION bin)
