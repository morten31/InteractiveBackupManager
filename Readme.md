# Filesystem Synchronization Tool

## Overview
A Linux command-line utility written in C for managing real-time directory backups. The program provides a CLI to create backups, which perform an initial recursive copy of a source directory and then continuously monitor it for changes (file modifications, creations, deletions, and moves) using the `inotify` API. Changes are synchronized to target directories on the fly.

The application is built using POSIX system calls and handles multiple parallel backup tasks by spawning independent background processes for each target directory.

## Features
* **Real-time synchronization:** Uses `inotify` to track filesystem events and apply them to backup directories immediately.
* **Multi-processing:** Each backup target is handled by a separate child process (`fork`), allowing the main process to remain responsive to user commands.
* **Symlink handling:** Correctly handles absolute symbolic links pointing inside the source directory by resolving them to relative paths in the destination directory.
* **POSIX-compliant system calls:** Fully implements file operations (`read`, `write`, `fstat`, `nftw`) and process management (`waitpid`, signals) without relying on high-level wrappers like `system()`.
* **Graceful shutdown:** Properly handles `SIGINT` and `SIGTERM` signals to terminate child processes cleanly and avoid memory leaks or zombie processes.

## Performance Benchmarks
The tool has been stress-tested for both throughput and system call overhead:
* **High Throughput:** Achieved **~190 MB/s** sustained copy speed for large continuous files (limited primarily by hardware I/O and page cache).
* **Massive File Trees:** Processed **over 2,400 files/sec** during initial recursive synchronization of highly nested directory structures (10,000 files).
* **Live Sync Latency:** Achieved a **sub-5ms** event response time from single filesystem modification to synchronization trigger.
* **Memory Safety:** Rigorously tested with Valgrind and AddressSanitizer, ensuring **0 memory leaks** and complete prevention of zombie processes.

## Interactive Commands
The program runs an interactive prompt. Available commands:

| Command | Description |
| :--- | :--- |
| `add <source> <target1> [target2]...` | Starts backing up `<source>` to given targets. Spawns a new background process for each target. |
| `list` | Lists all currently active background backup processes (showing their PIDs, source, and target paths). |
| `end <source> <target>` | Stops the real-time monitoring and terminates the worker process for a specific backup task. The copied files remain intact. |
| `restore <source> <target>` | Blocks the prompt, cleans the `<source>` directory, and completely restores it from the `<target>` backup. |
| `exit` | Sends termination signals to all active child processes, cleans up memory, and exits the program. |

## Build and Run

The project includes a `Makefile` for compilation. The source files are located in the `src/` directory. Add compiler flag `-DPERF_METRICS` for initial copy latency metrics.

To build the project:
```bash
make
