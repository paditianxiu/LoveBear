project(dobby)

add_library(${PROJECT_NAME} STATIC
        dobby.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE
    android
    log
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/${ANDROID_ABI}/libdobby.a
)

#set_target_properties(${PROJECT_NAME} PROPERTIES
#        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../../jniLibs/arm64-v8a"
#        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../../jniLibs/arm64-v8a"
#        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../../jniLibs/arm64-v8a"
#)