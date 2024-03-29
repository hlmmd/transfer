# 检查CMake版本
cmake_minimum_required(VERSION 3.3)

# project名称
project (transfer)

# 将include目录添加到include的搜索路径中,PROJECT_SOURCE_DIR为当前工程根目录
include_directories ("${PROJECT_SOURCE_DIR}/include")

# 生成debug/release版本
SET(CMAKE_BUILD_TYPE "release")
#SET(CMAKE_BUILD_TYPE "debug")
if (CMAKE_BUILD_TYPE STREQUAL debug)
    add_definitions(-D_DEBUG)
endif ()
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb ")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall ")
#SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -W ")

# 设置版本号
#set (LIBQEVENT_VERSION_MAJOR 1)
#set (LIBQEVENT_VERSION_MINOR 0)

# 配置一个头文件，从而将CMake的一些设置传递到源文件中。
# LibqeventConfig.h.LibqeventConfig.h
#configure_file (
#  "${PROJECT_SOURCE_DIR}/LibqeventConfig.h.in"
#  "${PROJECT_SOURCE_DIR}/include/LibqeventConfig.hpp"
#)

# 添加 src 目录
#add_subdirectory (src) 

aux_source_directory(common COMMON_LIST) 

aux_source_directory(server SRC_LIST) 

# 设置可执行文件
add_executable(transfer_server ${SRC_LIST} ${COMMON_LIST})

#add_executable(qstring qstring.c)

aux_source_directory(client SRC_LIST_C  ) 

# 设置可执行文件
add_executable(transfer_client ${SRC_LIST_C}  ${COMMON_LIST})

add_executable(client client_epoll.cpp)

#测试hashmap的程序，不能直接用cmake编译，（如何同时编译C和C++ 程序？？）
#add_executable(hashmap testhashmap.cpp src/hashmap.c)

# 为可执行文件添加链接库文件
target_link_libraries (transfer_server event)

#安装二进制文件
#install(TARGETS transfer DESTINATION bin)

