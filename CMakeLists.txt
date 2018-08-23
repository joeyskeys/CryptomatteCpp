cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are:
        ${CMAKE_CONFIGURATION_TYPES}")
endif()

project(CryptomatteCpp)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(Nuke)
include_directories(${NUKE_INCLUDE_PATH})
find_package(RapidJSON)
include_directories((${RAPIDJSON_INCLUDE_PATH}))

add_library(CryptomatteCpp SHARED
    Cryptomatte.cpp
)

target_link_libraries(CryptomatteCpp
    general
        ${NUKE_LIBRARY_PATH}
)