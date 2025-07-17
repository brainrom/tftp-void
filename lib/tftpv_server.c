/*
 * tftp-void - Tiny TFTP server library
 * Copyright (c) 2025 Ilya Chelyadin <brainrom@users.noreply.github.com>
 * https://github.com/brainrom/tftp-void
 *
 * This file is part of the tftpv project and is licensed under the MIT License.
 * See the LICENSE file in the project root for full license text.
 */

#include "tftpv_server.h"
#include "string.h"

void fill_header(uint8_t *buffer, uint16_t opcode, uint16_t arg)
{
    buffer[0] = (opcode >> 8) & 0xFF;
    buffer[1] = opcode & 0xFF;
    buffer[2] = (arg >> 8) & 0xFF;
    buffer[3] = arg & 0xFF;
}

void send_ack(tftpv_serverctx_t *c, uint16_t block_number) {
    uint8_t ack_buffer[4];
    fill_header(ack_buffer, TFTPV_OP_ACK, block_number);
    c->send_datagram(ack_buffer, sizeof(ack_buffer), c->send_userdata);
}

/* Helper function to send ERROR */
void send_error(tftpv_serverctx_t *c, tftpv_error_code_t error_code, const char *error_msg) {
    uint8_t error_buffer[512]; /* Maximum possible size for an ERROR packet */
    fill_header(error_buffer, TFTPV_OP_ERROR, error_code);

    size_t msg_len = strlen(error_msg);
    memcpy(error_buffer + 4, error_msg, msg_len); /* Error message */
    error_buffer[4 + msg_len] = '\0'; /* Null terminator */

    c->send_datagram(error_buffer, 4 + msg_len + 1, c->send_userdata); /* Include null terminator in length */
}

void send_data_from_handler(tftpv_serverctx_t *c, uint16_t block_number, const tftpv_file_t *reading_file)
{
    uint8_t data_buffer[512]; /* Maximum possible size for a DATA packet */
    tftpv_error_t err = {0};
    fill_header(data_buffer, TFTPV_OP_DATA, block_number);

    size_t data_length = reading_file->read_block(data_buffer + 4, block_number, reading_file, &err);
    if (err.code!=TFTPV_ERR_UNDEFINED)
    {
        send_error(c, err.code, err.message);
        return;
    }

    c->send_datagram(data_buffer, 4 + data_length, c->send_userdata);
}

int32_t check_block_num(tftpv_serverctx_t *c, const uint8_t *buffer)
{
    uint16_t block_number = (buffer[2] << 8) | buffer[3];

    if (block_number != c->expected_block_number && block_number != c->expected_block_number - 1) {
        send_error(c, TFTPV_ERR_ILLEGAL_OPERATION, "Unexpected block number");
        c->current_file = NULL; /* Reset current file */
        return -1;
    }

    if (block_number == c->expected_block_number) {
        c->expected_block_number++;
    }
    return block_number;
}

const tftpv_file_t *tftpv_server_search_file_in_list(const char* filename, void *userdata)
{
    const tftpv_file_t *files = userdata;
    size_t i;
    for (i = 0; files[i].filename != NULL; i++) {
        if (strcmp(files[i].filename, filename) == 0) {
            return &files[i];
        }
    }
    return NULL;
}

int tftpv_server_parse(tftpv_serverctx_t *c, const uint8_t *buffer, size_t length) {
    if (length < 4) { /* Minimum length for any valid TFTP packet */
        send_error(c, TFTPV_ERR_ILLEGAL_OPERATION, "Packet too short");
        return -1;
    }

    /* Extract opcode from the buffer */
    uint16_t opcode = (buffer[0] << 8) | buffer[1];
    switch (opcode) {
    case TFTPV_OP_ACK:
    {
        if (c->current_file==NULL || c->current_operation!=TFTPV_OP_RRQ)
        {
            send_error(c, TFTPV_ERR_ILLEGAL_OPERATION, "No active read operation");
            return -1;
        }

        if (check_block_num(c, buffer)<0)
            return -1;

        send_data_from_handler(c, c->expected_block_number, c->current_file);
        break;
    }
    case TFTPV_OP_RRQ: /* Read request */
    case TFTPV_OP_WRQ: { /* Write request */
        /* Parse filename and mode */
        const char *filename = (const char *)(buffer + 2);
        const char *mode = strchr(filename, '\0') + 1; /* First \0 in filename is the end of filename (and beginning of mode) */

        if (!filename || !mode) {
            send_error(c, TFTPV_ERR_ILLEGAL_OPERATION, "Invalid packet");
            return -1;
        }

        if (strcmp(mode, "octet") != 0)
        {
            send_error(c, TFTPV_ERR_ILLEGAL_OPERATION, "Only octet mode is supported");
            return -1;
        }

        /* Search for the file */
        const tftpv_file_t *found_file = c->search_file(filename, c->search_userdata);
        if (!found_file) /* File not found */
        {
            send_error(c, TFTPV_ERR_FILE_NOT_FOUND, "File not found");
            return -1;
        }

        c->current_file = found_file;
        c->expected_block_number = 1; /* Reset block number expectation */
        if (opcode == TFTPV_OP_WRQ && c->current_file->write_block)
        {
            send_ack(c, 0); /* Send ACK for block 0 */
            c->current_operation = TFTPV_OP_WRQ;
        }
        else if (opcode == TFTPV_OP_RRQ && c->current_file->read_block)
        {
            c->current_operation = TFTPV_OP_RRQ;
            send_data_from_handler(c, 1, c->current_file);
        }
        else
        {
            send_error(c, TFTPV_ERR_ILLEGAL_OPERATION, "Current operation is unavailable for this file");
            return -1;
        }
        return 0;
    }

    case TFTPV_OP_DATA: { /* Data packet */
        if (c->current_file == NULL || c->current_operation!=TFTPV_OP_WRQ) {
            send_error(c, TFTPV_ERR_ILLEGAL_OPERATION, "No active write operation");
            return -1;
        }

        int32_t block_number = check_block_num(c, buffer);

        if (block_number<0)
            return -1;

        /* Call the write function for the current file */
        size_t data_length = length - 4; /* Subtract opcode and block number bytes */
        tftpv_error_t err = {0};
        c->current_file->write_block(buffer + 4, block_number, data_length, c->current_file, &err);
        if (err.code!=TFTPV_ERR_UNDEFINED)
        {
            send_error(c, err.code, err.message);
            return -1;
        }

        /* Send ACK for the received block */
        send_ack(c, block_number);
        return -1;
    }

    default: /* Unsupported opcode */
        send_error(c, TFTPV_ERR_ILLEGAL_OPERATION, "Unsupported operation");
        return -1;
    }
    return -1;
}
