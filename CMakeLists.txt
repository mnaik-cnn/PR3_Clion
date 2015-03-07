cmake_minimum_required(VERSION 2.8.4)
project(PR3_Clion)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES webproxy.c handle_with_curl.c simplecached.c handle_with_cache.c simplecache.c  shm_channel.h simplecache.h)
add_executable(PR3_Clion ${SOURCE_FILES})