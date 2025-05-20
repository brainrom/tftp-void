/*
 * tftp-void - Tiny TFTP server library
 * Copyright (c) 2025 Ilya Chelyadin <brainrom@users.noreply.github.com>
 * https://github.com/brainrom/tftp-void
 *
 * This file is part of the tftpv project and is licensed under the MIT License.
 * See the LICENSE file in the project root for full license text.
 */

#ifndef TFTPV_SERVER_H
#define TFTPV_SERVER_H

#include <stdint.h>
#include <stdio.h>

/* Enum for TFTP opcodes */
typedef enum tftpv_opcode {
    TFTPV_OP_RRQ = 1,
    TFTPV_OP_WRQ,
    TFTPV_OP_DATA,
    TFTPV_OP_ACK,
    TFTPV_OP_ERROR
} tftpv_opcode_t;

/* Enum for TFTP error codes */
typedef enum tftpv_error_code {
    TFTPV_ERR_UNDEFINED = 0,
    TFTPV_ERR_FILE_NOT_FOUND,
    TFTPV_ERR_ACCESS_VIOLATION,
    TFTPV_ERR_DISK_FULL,
    TFTPV_ERR_ILLEGAL_OPERATION,
    TFTPV_ERR_UNKNOWN_TID,
    TFTPV_ERR_FILE_EXISTS,
    TFTPV_ERR_NO_SUCH_USER
} tftpv_error_code_t;

typedef struct tftpv_error {
    tftpv_error_code_t code;
    const char *message;
} tftpv_error_t;

/* Structure to represent a file available over TFTP */
typedef struct tftpv_file {
    const char *filename;
    void (*write_block)(const uint8_t *block, uint16_t block_number, size_t block_length, const struct tftpv_file *writing_file, tftpv_error_t *err);
    size_t (*read_block)(uint8_t *block, uint16_t block_number, const struct tftpv_file *reading_file, tftpv_error_t *err);
    void *userdata;
} tftpv_file_t;

/* TFTP Server context */
typedef struct tftpv_serverctx {
    void (*send_datagram)(uint8_t *buf, size_t len, void *userdata);
    void *send_userdata;

    const tftpv_file_t *(*search_file)(const char* filename, void *userdata);
    void *search_userdata;

    const tftpv_file_t *current_file;
    uint16_t expected_block_number;
    tftpv_opcode_t current_operation;
} tftpv_serverctx_t;

const tftpv_file_t *tftpv_server_search_file_in_list(const char* filename, void *files);
int tftpv_server_parse(tftpv_serverctx_t *c, const uint8_t *buffer, size_t length);

#endif /* TFTPV_SERVER_H */
