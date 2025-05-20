cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

set (TFTP_VOID_STANDALONE_PROJECT ON)
if(DEFINED PROJECT_NAME)
  set(TFTP_VOID_STANDALONE_PROJECT OFF)
endif()

project(tftp-void)

option(TFTPV_BUILD_EXAMPLES "Build examples" ${TFTP_VOID_STANDALONE_PROJECT})

add_subdirectory(lib)

if (TFTPV_BUILD_EXAMPLES)
add_subdirectory(examples)
endif()
