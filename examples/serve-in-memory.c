#include "tftpv_server.h"
#include <string.h>

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define MAX_FILE_SIZE 1024
#define MAX_FILE_SIZE_STR STR(MAX_FILE_SIZE)
#define TFTP_PORT 6969

typedef struct filedata
{
    char buf[MAX_FILE_SIZE];
    size_t size;
} filedata_t;

void write_memfile(const uint8_t *block, uint16_t block_number, size_t block_length, const struct tftpv_file *writing_file, tftpv_error_t *err)
{
    filedata_t *f = writing_file->userdata;
    size_t offset = (block_number-1)*0x200;

    if (block_number==1)
        f->size = 0;

    if (offset+block_length>MAX_FILE_SIZE)
    {
        err->code = TFTPV_ERR_DISK_FULL;
        err->message = "File larger than" MAX_FILE_SIZE_STR "isn't allowed";
        return;
    }
    memcpy(f->buf+offset, block, block_length);
    f->size+=block_length;
}

size_t read_memfile(uint8_t *block, uint16_t block_number, const struct tftpv_file *reading_file, tftpv_error_t *err)
{
    filedata_t *f = reading_file->userdata;
    size_t offset = (block_number-1)*0x200;

    size_t size = f->size;
    size_t sendsize;

    if (offset>size)
        return 0;

    if (size<512)
        sendsize = size;
    else if (offset+512>size)
        sendsize = size-offset;
    else
        sendsize = 512;

    memcpy(block, f->buf+offset, sendsize);
    return sendsize;
}

filedata_t filebufs[2] = {};

tftpv_file_t files[] = {
    {"file1", write_memfile, read_memfile, &filebufs[0]},
    {"file2", write_memfile, read_memfile, &filebufs[1]},
    {} /* NULL termination is mandatory! */
};

const char file1_content[] = "This is the file1 test content\n";
const char file2_content[] = "This is the file2 test content\n";


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
    printf("tftp-void serve-in-memory example.\nOnly for test purposes!\nDON'T USE IN PRODUCTION!\n");
    /* Filling example data */
    memcpy(filebufs[0].buf, file1_content, sizeof(file1_content));
    filebufs[0].size = sizeof(file1_content)-1;

    memcpy(filebufs[1].buf, file2_content, sizeof(file2_content));
    filebufs[1].size = sizeof(file2_content)-1;

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
    ctx.search_file = tftpv_server_search_file_in_list; /* Method to search file */
    ctx.search_userdata = files; /* Will be passed to search_file function */

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
