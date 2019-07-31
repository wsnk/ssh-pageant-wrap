cmake_minimum_required(VERSION 3.2)
project(ssh-pageant-wrap)

set(CMAKE_CXX_STANDARD 11)

# path to real Git's ssh executable
set(GIT_SSH_PATH "C:/Program Files/Git/usr/bin/ssh.exe")

set(SOURCES
    common.cpp
    common.h
    main.cpp
    network.cpp
    network.h
    pageant.cpp
    pageant.h
)

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE Ws2_32)
target_compile_definitions(${PROJECT_NAME} PRIVATE "GIT_SSH_PATH=\"${GIT_SSH_PATH}\"")

if(MINGW)
    message(STATUS "Link with GCC's libraries statically")
    target_link_libraries(${PROJECT_NAME} PRIVATE -static)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug output is ON")
else()
    message(STATUS "Debug output is OFF")
    target_compile_definitions(${PROJECT_NAME} PRIVATE "NDEBUG")
endif()
