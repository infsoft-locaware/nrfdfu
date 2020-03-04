cmake_minimum_required(VERSION 3.0)

project(nrfdfu LANGUAGES C)

option(BLE_SUPPORT "DFU over BLE support" ON)

find_package(PkgConfig REQUIRED)
pkg_search_module(LIBZIP REQUIRED libzip)
pkg_search_module(JSONC REQUIRED json-c)
find_package(ZLIB)

if (BLE_SUPPORT)
pkg_search_module(BLZ REQUIRED blzlib)
add_definitions(-DBLE_SUPPORT)
endif (BLE_SUPPORT)

add_executable(nrfdfu main.c log.c util.c serialtty.c
    dfu.c dfu_serial.c slip.c dfu_ble.c)

target_include_directories(nrfdfu PRIVATE . ${BLZLIB_INCLUDE_DIRS})
target_link_libraries(nrfdfu ${ZLIB_LIBRARIES} ${LIBZIP_LIBRARIES}
    ${JSONC_LIBRARIES} ${BLZ_LIBRARIES})

install(TARGETS nrfdfu RUNTIME DESTINATION bin)
