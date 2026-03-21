# Multi-Client File Transfer Server (B1)

## Build

From this folder:

```bash
make
```

Or manually:

```bash
g++ -std=c++17 -O2 -pthread -Iinclude src/server.cpp -o bin/server
g++ -std=c++17 -O2 -pthread -Iinclude src/client.cpp -o bin/client
```

## Run

### Server 

```bash
./bin/server 8784
```

The server will interactively ask if you want to enable rate limiting:

```
Enable rate limiting per client? [y/N]: y
Enter bandwidth limit (e.g. 1m = 1 MB/s, 500k = 500 KB/s): 1m
```

Or pass rate limit directly via command line:

```bash
./bin/server 8784 1048576
```

### Client

```bash
./bin/client 127.0.0.1 8784
```

Client will save downloaded files to `./downloads/` (auto-created).

## Commands

### Server Prompt

The server runs its own interactive console while listening for connections:
- `LIST`: Displays all files currently stored in the server's local `uploads/` directory, presented in a clean, human-readable format with dynamic file-type emojis and an aligned table view identical to the client.
- `QUIT`: Triggers a graceful shutdown, safely disconnecting all active clients and closing the socket.

### Client Prompt

In the interactive client terminal:
- `LIST`: Request the server for a list of available files to download. Output includes smart formatting, dynamic file-type emojis, and aligned columns.
- `UPLOAD <local_path>`: Upload a file (e.g., `UPLOAD preupload/my_file.bin`). A local `preupload/` directory is provided to easily organize files before uploading.
- `DOWNLOAD <filename>`: Download a file to the local `downloads/` directory.
- `QUIT`: Exit the client.

### Bonus behaviors

- **Resume upload**: if the server already has a partial `uploads/<filename>`, re-running `UPLOAD` continues from the last byte on disk.
- **Resume download**: if the client already has a partial `downloads/<filename>`, re-running `DOWNLOAD` continues from the last byte on disk.
- **Integrity check (SHA-256)**:
  - Upload: client sends SHA-256; server replies SHA-256 and `MATCH/MISMATCH`.
  - Download: server sends SHA-256; client verifies after saving.

## Protocol / Requirements mapping

- Multi-client: server uses `std::thread` per connection.
- LIST: server replies with `OK` + lines `filename\tsize`.
- UPLOAD: client sends `UPLOAD <filename> <bytes>` then raw bytes streamed in chunks.
- DOWNLOAD: client sends `DOWNLOAD <filename>` then server replies `OK <bytes>` then raw bytes streamed in chunks.
- Progress reporting: prints every ~10%.
- Chunked I/O: 64KB buffer.
- Message framing for control messages: 4-byte big-endian length prefix + payload.

## Load test (many clients)

You can spawn many concurrent client threads to test multi-client behavior:

```bash
./bin/client 127.0.0.1 8784 --load 10 --loops 2 --max-kb 512
```

- `--load N`: number of concurrent clients.
- `--loops L`: each client does L times upload+download.
- `--max-kb K`: random file size per loop from 1..K KB.

## Demo (2 simultaneous clients)

Open 3 terminals:

Terminal A:
```bash
./bin/server 8784
```

Terminal B:
```bash
./bin/client 127.0.0.1 8784
```

Terminal C:
```bash
./bin/client 127.0.0.1 8784
```

Try:
- On B: `UPLOAD path/to/file.bin`
- On C: `LIST` and `DOWNLOAD file.bin`
