cmake_minimum_required(VERSION 3.12)
project(WebRTC_Signaling_Server C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags in the spirit of Linux kernel
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall -Wextra -Wpedantic")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=return-type -Werror=implicit-function-declaration")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_FORTIFY_SOURCE=2")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-strong")

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBWEBSOCKETS REQUIRED libwebsockets)
pkg_check_modules(JANSSON REQUIRED jansson)

# Include directories
include_directories(include)
include_directories(${LIBWEBSOCKETS_INCLUDE_DIRS})
include_directories(${JANSSON_INCLUDE_DIRS})

# Source files
set(SRC_FILES
    redrtc.c
    src/server.c
    src/client.c
    src/room.c
    src/messages.c
    src/utils.c
)

# Build executable
add_executable(webrtc_server ${SRC_FILES})

# Link libraries
target_link_libraries(webrtc_server 
    ${LIBWEBSOCKETS_LIBRARIES}
    ${JANSSON_LIBRARIES}
    m
)

# Installation
install(TARGETS webrtc_server DESTINATION bin)
install(FILES README.md DESTINATION share/doc/webrtc_server)