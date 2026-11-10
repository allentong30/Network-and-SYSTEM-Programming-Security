# SysProbe: Remote System Information and Health Checks

A Linux C midterm project. The TCP server uses `fork()` to create one child per connection, without threads. Tests on WSL Ubuntu demonstrated 12 concurrent clients and three robustness comparisons. See the [report](docs/REPORT.md) and [actual results](evidence/results.json).

## Build and run

Run in Ubuntu or WSL:

```bash
cd /mnt/c/examcode/midterm-sysprobe
# For a GitHub clone, enter its midterm-sysprobe directory instead.
sudo apt-get update
sudo apt-get install build-essential python3
make
./build/1/server --debug
```

In another terminal:

```bash
./build/1/client 127.0.0.1 9090 SYSINFO --debug
./build/1/client 127.0.0.1 9090 MEMORY
./build/1/client 127.0.0.1 9090 DISK
./build/1/client 127.0.0.1 9090 PING
./build/1/client 127.0.0.1 9090 'HOLD 3'
```

Press Ctrl+C to stop the server. The parent closes the listener, terminates children, and reaps them with waitpid. Connections support multiple framed requests; the CLI sends one request per invocation. HOLD accepts 1-10 seconds for concurrency demonstrations. QUIT returns a goodbye response and closes the session.

## Two levels of debug control

```bash
make DEBUG=1                     # Include debug code in build/1.
./build/1/server --debug          # Print DEBUG messages.
./build/1/server                  # Suppress DEBUG messages.
make DEBUG=0                     # Remove debug code in build/0.
./build/0/server --debug          # DEBUG messages remain unavailable.
```

Build directories are separate, so switching does not require clean. The client also accepts `--debug`. INFO and error messages are independent of debug control. Run server examples sequentially to avoid port conflicts.

## Verification

```bash
make test
ldd build/1/server
ldd build/1/client
```

Tests start real C servers and clients, inject abnormal TCP traffic using Python, and open 12 simultaneous connections. The `evidence/` directory stores process snapshots, dynamic linking output, logs, and JSON results. Rerunning tests updates the text evidence. Python is only a test driver; the application and shared library are written in C.

## Robustness comparisons

`--baseline` is a simplified teaching mode for local comparisons. It retains length bounds, the fork architecture, and parent cleanup.

| Scenario | Default robust mode | Baseline mode |
|---|---|---|
| Fragmented TCP body | Receives the complete body in a loop | Closes after an incomplete single recv |
| Sender stops after one header byte | Absolute frame deadline releases the child | MSG_WAITALL without a deadline keeps the child waiting |
| Unknown command followed by PING | Returns ERR and preserves the session | Parser drops the connection |

Other protections include a 4096-byte frame limit, rejection of empty frames and NUL bytes, partial I/O handling, EINTR/EAGAIN retries, MSG_NOSIGNAL, a 64-child limit, socket/fork error handling, child reaping, and SO_REUSEADDR.

## Packet capture and submission

Follow the [capture guide](docs/PACKET_CAPTURE.md) to record traffic and complete the analysis. The [report](docs/REPORT.md) includes system design, source descriptions, comparisons, evidence, and limitations. Add your name, student ID, any required terminal screenshots, and packet analysis before submission. Packet capture is pending.

The default listener binds to loopback. For communication between computers, use `--bind 0.0.0.0 --port 9090` and connect to the server's reachable IPv4 address. WSL NAT and forwarding depend on the environment. This demonstration uses plaintext TCP without authentication, TLS, or a remote shell. Disk queries are fixed to `/`; client input is never executed as a system command.

## Course example references

Examples supplied in `C:\CODE`: `sockets/server/server4.c` (fork server), `TLS-Socket/TLS-Socket/program1/server_plain.c` (TCP workflow), `develop_debug/debug/debug_ultra_min.c` (compile-time/runtime debug), `develop_debug/shared_library/README` (shared libraries), and `signal4/Signal4.c` (signals and reaping). This project implements these concepts independently; example binaries and TLS private keys are not included.
