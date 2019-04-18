cmake_minimum_required(VERSION 3.0)

project(nrfdfu LANGUAGES C)

find_package(PkgConfig REQUIRED)
pkg_search_module(LIBZIP REQUIRED libzip)
pkg_search_module(JSONC REQUIRED json-c)
find_package(ZLIB)

add_executable(nrfdfu main.c log.c serialtty.c dfu.c slip.c)
target_link_libraries(nrfdfu ${ZLIB_LIBRARIES} ${LIBZIP_LIBRARIES} ${JSONC_LIBRARIES})

install(TARGETS nrfdfu RUNTIME DESTINATION bin)
