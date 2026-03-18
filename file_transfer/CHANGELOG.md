# Changelog

All notable changes to the `file_transfer` project will be documented in this file.

## [Unreleased]

### Changed
- **Integrity check**: Replaced MD5 with SHA-256 for file integrity verification. Created `include/sha256.hpp` (pure C++ implementation), updated `hash.hpp`, `protocol.hpp`, `server.cpp`, `client.cpp`, `Makefile`, and `README.md`.

### Added
- **Modern Terminal UI**: Created `include/terminal_ui.hpp` with ANSI colors, Unicode progress bars (█░), styled banner, file list formatting with icons, colored success/error messages, spinner, and `HELP` command. Client now supports case-insensitive commands.
- **Interactive Rate Limit**: Server now interactively asks whether to enable rate limiting when no rate limit argument is passed via command line. Supports human-readable input: `1m` (1 MB/s), `500k` (500 KB/s), or raw bytes.

### Fixed
- **Security**: Fixed a Denial of Service (DoS) vulnerability via memory exhaustion in `include/net.hpp` (`recv_frame`). Added a constant `kMaxFrameSize = 10MB` to limit incoming packet sizes, preventing malicious clients from requesting arbitrary memory allocations and crashing the server.
- **Architecture**: Replaced `std::thread().detach()` with a managed thread vector and `join()` in `src/server.cpp`. Added `kMaxThreads = 128` connection limit to prevent thread exhaustion attacks (Slowloris).
- **Reliability**: Added graceful shutdown via `SIGINT`/`SIGTERM` signal handlers in `src/server.cpp`. Server now waits for all active client threads to finish before exiting cleanly.
