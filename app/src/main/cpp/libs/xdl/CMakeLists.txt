project(xdl)

set(CMAKE_CXX_VISIBILITY_PRESET "hidden")
set(CMAKE_C_VISIBILITY_PRESET "hidden")

add_library(${PROJECT_NAME} STATIC
        xdl.c
        xdl_util.c
        xdl_lzma.c
        xdl_linker.c
        xdl_iterate.c)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE android log)
