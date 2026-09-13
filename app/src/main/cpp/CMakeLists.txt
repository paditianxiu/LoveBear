cmake_minimum_required(VERSION 3.22.1)

project("LoveBear")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DEPENDENCIES
        dobby
        KittyMemory
        xdl
)

set(DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs")

add_library(
        padi
        SHARED
        padi.cpp)

find_library(
        log-lib
        log)

target_link_libraries(
        padi
        ${log-lib}
        ${CMAKE_DL_LIBS})

foreach (DEP ${DEPENDENCIES})
    add_subdirectory("${DEPS_DIR}/${DEP}" EXCLUDE_FROM_ALL)
endforeach ()

# 链接 KittyMemory 和 dobby 的 target
target_link_libraries(padi KittyMemory dobby xdl)

set(DEP_INCLUDE_DIRS)
foreach (DEP ${DEPENDENCIES})
    list(APPEND DEP_INCLUDE_DIRS "${DEPS_DIR}/${DEP}")
endforeach ()

target_include_directories(padi PRIVATE ${DEP_INCLUDE_DIRS})
