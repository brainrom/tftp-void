# tftp-void — Minimal TFTP Server Library in C99

`tftp-void` is a lightweight and portable TFTP server library written in clean C99.
It implements [RFC 1350](https://datatracker.ietf.org/doc/html/rfc1350) (TFTP protocol)
and is suitable for bare-metal, embedded,
or constrained environments where sockets are externally managed.

- CMake-based build system
- C99-compliant code
- No dependency on any socket library — all I/O is delegated to user code
- Flexible file backend — reading and writing is fully handled via user-defined callbacks
(serve from memory, filesystem, or anything else)

---

## Try It Out

### Build and Run (with Examples)

```sh
git clone https://github.com/brainrom/tftp-void
cd tftp-void
cmake -B build
cmake --build build
````

This builds the library and test examples.

### Examples

Two sample servers using BSD sockets are provided:

* `serve-folder` — Serves files from the current directory using standard file I/O
* `serve-in-memory` — Serves fixed in-memory buffers (`file1`, `file2`)

### Testing

Use any standard TFTP client (e.g., `tftp`, `curl`) to test:

```sh
tftp localhost 6969
tftp> get file
tftp> put file
```

---

## Usage

Here’s a minimal example of integrating the library into your UDP receive loop:

```c
tftpv_file_t files[] = {
    { "file", write_file_cb, read_file_cb, file_userdata },
    { 0 } /* NULL-terminated! */
};

tftpv_serverctx_t ctx;
ctx.send_datagram   = send_func;
ctx.send_userdata   = send_func_userdata;
ctx.search_file     = tftpv_server_search_file_in_list;
ctx.search_userdata = files;

while (1) {
    /* Receive UDP datagram into buffer */
    tftpv_server_parse(&ctx, buffer, received);
}
```

## Integration: CMake

If you want to use the library with CMake, then use `add_subdirectory(tftp-void)`
and link your target with `tftpv_server` library.

Alternatively you can just add `tftpv_server.c` and `tftpv_server.h` to your project.


## Integration: Required Callbacks

### `send_datagram`

```c
void (*send_datagram)(uint8_t *buf, size_t len, void *userdata);
```

Called when the server needs to send a response.

* `buf`: Pointer to data to send
* `len`: Number of bytes to send
* `userdata`: User-supplied context pointer (`ctx.send_userdata`)

### `search_file`

```c
const tftpv_file_t *(*search_file)(const char *filename, void *userdata);
```

Called to locate a file on read/write request.

* `filename`: Requested file name
* `userdata`: User-supplied context pointer (`ctx.search_userdata`)
* Returns a pointer to a matching `tftpv_file_t`, or `NULL` if not found

#### Provided Helper: `tftpv_server_search_file_in_list`

```c
const tftpv_file_t *tftpv_server_search_file_in_list(const char *filename, void *userdata);
```

This helper searches through a `NULL`-terminated array of `tftpv_file_t` structures (like `files[]` above). Use this if you want a quick static file list without writing your own search logic.

## File Object Structure: `tftpv_file_t`

```c
typedef struct tftpv_file {
    const char *name;
    void (*write_block)(const uint8_t *block, uint16_t block_number, size_t block_length, const struct tftpv_file *writing_file, tftpv_error_t *err);
    size_t (*read_block)(uint8_t *block, uint16_t block_number, const struct tftpv_file *reading_file, tftpv_error_t *err);
    void *userdata;
} tftpv_file_t;
```

* `name`: File name string
* `write_block`: Called when a write operation arrives (can be `NULL`)
* `read_block`: Called when a read operation is requested (can be `NULL`)
* `userdata`: Pointer passed to callbacks for context

## Block Callbacks

### `write_block`

```c
void write_block(
    const uint8_t *block,
    uint16_t block_number,
    size_t block_length,
    const struct tftpv_file *writing_file,
    tftpv_error_t *err
);
```

* Called when a TFTP client sends a block to write
* `block`: Input data buffer
* `block_number`: Block number (starting from 1)
* `block_length`: Number of bytes in this block. `< 512` signals end of file
* `writing_file`: Pointer to the associated file definition
* `err`: Set err->code and err->message if an error occurs. Error message will be sent to client.

### `read_block`

```c
size_t read_block(
    uint8_t *block,
    uint16_t block_number,
    const struct tftpv_file *reading_file,
    tftpv_error_t *err
);
```

* Called when a TFTP client requests a block to read
* `block`: Output data buffer to fill (max 512 bytes)
* `block_number`: Block number (starting from 1)
* `reading_file`: Pointer to the associated file definition
* `err`: Set err->code and err->message if an error occurs. Error message will be sent to client.
* Returns the number of bytes written to `block`. Returning `< 512` signals end of file.

---

## License

MIT — see [LICENSE](LICENSE)

