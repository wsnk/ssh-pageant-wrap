cmake_minimum_required(VERSION 3.0)
project(ssh-pageant-wrap)

# path to real Git's ssh executable
set(GIT_SSH_PATH "C:/Program Files/Git/usr/bin/ssh.exe")

set(SOURCES
    network.cpp
    main.cpp
    winpgntc.cpp
    common.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE Ws2_32)
target_compile_definitions(${PROJECT_NAME} PRIVATE "GIT_SSH_PATH=\"${GIT_SSH_PATH}\"")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug output is ON")
else()
    message(STATUS "Debug output is OFF")
    target_compile_definitions(${PROJECT_NAME} PRIVATE "NDEBUG")
endif()
