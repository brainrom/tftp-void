add_executable(serve-folder serve-folder.c)
target_link_libraries(serve-folder PRIVATE tftpv_server)

add_executable(serve-in-memory serve-in-memory.c)
target_link_libraries(serve-in-memory PRIVATE tftpv_server)
