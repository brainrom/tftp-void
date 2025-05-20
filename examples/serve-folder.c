#include "tftpv_server.h"
#include <string.h>

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>

#define TFTP_PORT 6969

void write_osfile(const uint8_t *block, uint16_t block_number, size_t block_length, const struct tftpv_file *writing_file, tftpv_error_t *err)
{
    FILE **fp = writing_file->userdata;
    size_t offset = (block_number-1)*0x200;

    if (!(*fp))
    {
        *fp = fopen(writing_file->filename, "wb");
    }

    if (!(*fp))
    {
        err->code = TFTPV_ERR_ACCESS_VIOLATION;
        err->message = "Can't open file";
        return;
    }

    fseek(*fp, offset, SEEK_SET);

    size_t written = fwrite(block, 1, block_length, *fp);

    if (written!=block_length)
    {
        err->code = TFTPV_ERR_DISK_FULL;
        err->message = "Can't write";
    }

    if (block_length<512)
    {
        fclose(*fp);
        *fp = NULL;
    }
}

size_t read_osfile(uint8_t *block, uint16_t block_number, const struct tftpv_file *reading_file, tftpv_error_t *err)
{
    void **fp = reading_file->userdata;
    size_t offset = (block_number-1)*0x200;

    if (!(*fp))
    {
        *fp = fopen(reading_file->filename, "rb");
    }

    if (!(*fp))
    {
        err->code = TFTPV_ERR_ACCESS_VIOLATION;
        err->message = "Can't open file";
        return 0;
    }

    fseek(*fp, offset, SEEK_SET);

    size_t read = fread(block, 1, 512, *fp);

    if (read==0) /* it should be read<512, but almost all tftp client will try to read until data block isn't empty */
    {
        fclose(*fp);
        *fp = NULL;
    }

    return read;
}

const tftpv_file_t *search_local_file(const char* filename, void *userdata)
{
    static char filename_buf[128];
    static FILE *f = NULL;
    static tftpv_file_t os_file;

    os_file.read_block = NULL;
    os_file.write_block = NULL;
    os_file.userdata = &f;
    os_file.filename = filename_buf;

    if (f)
    {
        fclose(f);
        f = NULL;
    }

    if (strchr(filename, '/') || strchr(filename, '\\')) /* Allow only current directory for security reasons */
    {
        return NULL;
    }

    strncpy(filename_buf, filename, sizeof(filename_buf)-1);


    if (access(filename, R_OK)==0)
        os_file.read_block = read_osfile;

    if (access(filename, W_OK)==0 || access(filename, F_OK)<0)
        os_file.write_block = write_osfile;

    return &os_file;
}

typedef struct bsd_socket_ctx {
    int sockfd;
    struct sockaddr_in client_address;
} bsd_socket_ctx_t;

void bsd_send(uint8_t *buf, size_t len, void *userdata)
{
    bsd_socket_ctx_t *ctx = (bsd_socket_ctx_t *)userdata;
    sendto(ctx->sockfd, buf, len, 0,
           (struct sockaddr *)&ctx->client_address, sizeof(ctx->client_address));
}

int main()
{
    printf("tftp-void serve-folder example.\nOnly for test purposes!\nDON'T USE IN PRODUCTION!\n");
    /* BSD socket backend */
    bsd_socket_ctx_t socket_ctx = {};
    uint8_t buffer[516]; /* Enough for TFTP datagram */
    struct sockaddr_in sender_addr, local_addr;
    socklen_t addrlen = sizeof(sender_addr);

    socket_ctx.sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_ctx.sockfd < 0) {
        perror("socket");
        return -1;
    }

    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(TFTP_PORT);

    if (bind(socket_ctx.sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("bind");
        close(socket_ctx.sockfd);
        return -1;
    }

    /* tftp_void initialization */
    tftpv_serverctx_t ctx;
    ctx.send_datagram = bsd_send; /* Will be called when tftpv needs to send datagram */
    ctx.send_userdata = &socket_ctx; /* Will be passed to send_userdata function */
    ctx.search_file = search_local_file; /* Method to search file */

    /* Main loop */

    while (1) {
        ssize_t received = recvfrom(socket_ctx.sockfd, buffer, sizeof(buffer), 0,
                                    (struct sockaddr *)&sender_addr, &addrlen);
        if (received < 0) {
            perror("recvfrom");
            continue;
        }

        /* It's important to save sender address to answer to it */
        socket_ctx.client_address = sender_addr;

        /* tftpv_server_parse() does all magic, including invoking  */
        tftpv_server_parse(&ctx, buffer, received);
    }

    close(socket_ctx.sockfd);
    return 0;

}
