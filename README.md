# Multi-Client File Transfer Server

A multi-client TCP file transfer system written in C++. Supports concurrent uploads and downloads, resume-on-reconnect, SHA-256 integrity verification, per-client bandwidth limiting, and an interactive terminal UI with ANSI colors and progress bars.

## Requirements

- Linux (POSIX sockets)
- g++ with C++17 support (GCC 7+)
- GNU Make

## Build

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

When no rate limit argument is provided, the server will interactively ask:

```
Enable rate limiting per client? [y/N]: y
Enter bandwidth limit (e.g. 1m = 1 MB/s, 500k = 500 KB/s): 1m
```

Or pass the rate limit directly on the command line:

```bash
./bin/server 8784 1048576   # port=8784, rate limit=1 MB/s (in bytes/s)
```

### Client

```bash
./bin/client 127.0.0.1 8784
```

Downloaded files are saved to `./downloads/` (auto-created if not present).

## Commands

### Server console

The server runs an interactive console while accepting client connections.

| Command | Description |
|---------|-------------|
| `list`  | Show all files in `uploads/`, with file-type icons, sizes, and aligned columns |
| `quit`  | Gracefully shut down — waits for all active clients to finish before exiting |

### Client console

Commands are **case-insensitive** (e.g. `LIST`, `list`, and `List` all work). Type `help` to see available commands.

| Command | Description |
|---------|-------------|
| `list` | Request the server's file list. Output includes file-type icons and aligned columns |
| `upload <path>` | Upload a file to the server. A `preupload/` directory is provided for staging files |
| `download <filename>` | Download a file from the server into `./downloads/` |
| `quit` | Disconnect and exit |

**Upload example:**

```bash
upload preupload/notes.txt
```

**Download example:**

```bash
download notes.txt
```

## Features

### Resume upload / download

If a transfer is interrupted, re-running the same `upload` or `download` command automatically resumes from the last successfully transferred byte — no manual intervention needed.

### Per-client bandwidth limiting

The server applies a token-bucket rate limiter independently per client connection. Configure it interactively at startup or pass the limit in bytes/s as a CLI argument.

### SHA-256 integrity check

Every transfer is verified end-to-end:

- **Upload**: the client sends the file's SHA-256 hash alongside the data. The server recomputes the hash after saving and replies with `MATCH` or `MISMATCH`.
- **Download**: the server sends the hash before streaming. The client verifies after the file is fully received.

## Protocol reference

| Operation | Wire format |
|-----------|-------------|
| List | Client: `LIST` → Server: `OK\n<filename>\t<bytes>\n...` |
| Upload | Client: `UPLOAD <filename> <bytes> <sha256>` → Server: `OK OFFSET <n>` → raw bytes → Server: `OK Saved SHA256 <hash> MATCH\|MISMATCH` |
| Download | Client: `DOWNLOAD <filename>` → Server: `OK <bytes> <sha256> OFFSET <n>` → raw bytes |
| Quit | Client: `QUIT` → Server: `OK Bye` |

## Demo — 2 simultaneous clients

Open 3 terminals:

**Terminal A — start the server:**

```bash
./bin/server 8784
```

**Terminal B — client 1:**

```bash
./bin/client 127.0.0.1 8784
```

**Terminal C — client 2:**

```bash
./bin/client 127.0.0.1 8784
```

Try the following across terminals:

- Terminal B: `upload preupload/funny_cat_video.mp4`
- Terminal C: `list` → `download funny_cat_video.mp4`

## Project structure

```
.
├── src/
│   ├── server.cpp          # Server entry point and client handler
│   └── client.cpp          # Client entry point and interactive REPL
├── include/
│   ├── protocol.hpp        # Wire protocol constants and message formatters
│   ├── net.hpp             # read_n / write_n / send_frame / recv_frame
│   ├── transfer.hpp        # Chunked file send/receive with progress callback
│   ├── hash.hpp            # SHA-256 file hashing helpers
│   ├── sha256.hpp          # Pure C++ SHA-256 implementation
│   ├── rate_limiter.hpp    # Token-bucket rate limiter
│   ├── file_utils.hpp      # Directory listing and filename sanitization
│   ├── terminal_ui.hpp     # ANSI colors, progress bars, banners, and icons
│   └── load_test.hpp       # Multi-threaded load test runner
├── preupload/              # Staging area for files to upload (sample files included)
├── downloads/              # Destination for downloaded files (auto-created)
├── uploads/                # Server-side file storage (auto-created)
├── Makefile
├── CHANGELOG.md
└── README.md
```
