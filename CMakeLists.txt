cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(owctl VERSION 0.0.1 LANGUAGES C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_FILE_OFFSET_BITS=64 -DOW_DEBUG")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall --pedantic -g")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake" ${CMAKE_MODULE_PATH})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

find_package(FUSE REQUIRED)
find_package(I2C REQUIRED)

include_directories(${FUSE_INCLUDE_DIR})
include_directories(${I2C_INCLUDE_DIRS})
include_directories(include)
add_executable(owctl
	owctl_main.c
	src/ow_cache.c
	src/owfs_callback.c
	src/ow_crc.c
	src/ow_dirblob.c src/ow_dir.c
	src/ow_ds2482.c
	src/error.c
	src/ow_filelength.c
	src/ow_fs_items.c
	src/ow_fstat.c
	src/ow_getbit.c
	src/ow_parseinput.c
	src/ow_parseobject.c
	src/ow_parsename.c
	src/ow_parse_sn.c
	src/ow_parseoutput.c
	src/ow_presence.c
	src/ow_memblob.c
	src/ow_name.c
	src/ow_none.c
	src/ow_read.c
	src/ow_reconnect.c
	src/ow_regex.c
	src/ow_search.c
	src/ow_tree.c
	src/ow_util.c
	src/ow_visibility.c
	src/ow_write.c	)

target_link_libraries(owctl ${FUSE_LIBRARIES} pthread)
