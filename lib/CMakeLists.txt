cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(tftpv_server)

add_library(tftpv_server STATIC tftpv_server.c)
set_target_properties(tftpv_server PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tftpv_server PRIVATE -std=c99 -pedantic -Wall -Wextra -Werror)
elseif(MSVC)
    target_compile_options(tftpv_server PRIVATE /W4 /WX)
endif()

target_include_directories(tftpv_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
