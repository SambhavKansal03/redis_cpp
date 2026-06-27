# Redis-Inspired Multithreaded Key-Value Store

A production-inspired, in-memory key-value store built from scratch in **C++17**, closely mirroring the core architecture of Redis and LevelDB. Supports concurrent clients via a thread pool, crash recovery via AOF persistence, and a RESP-style TCP protocol.

---

## Architecture

Built across 4 independent phases, each fully functional before the next was added:

```
Phase 3 — Concurrency Layer
Thread pool · reader-writer locks · mutex · race condition prevention
                        ↓
Phase 2 — TCP Server
POSIX sockets · client connections · RESP protocol · command parser
                        ↓
Phase 1 — Storage Engine  ← started here
HashMap · TTL expiry · LRU eviction · GET / SET / DEL / EXPIRE
                        ↓
Phase 4 — Persistence (AOF)
Append-only log · crash recovery · benchmark: ops/sec + p99 latency
```

### File Structure

```
kvstore/
├── CMakeLists.txt
├── main.cpp
├── aof.h / aof.cpp          ← Phase 4: AOF persistence
├── store/
│   ├── entry.h              ← TTL-aware entry struct
│   ├── kvstore.h
│   └── kvstore.cpp          ← HashMap + reader-writer locks
└── server/
    ├── thread_pool.h
    ├── thread_pool.cpp      ← Phase 3: fixed-size thread pool
    ├── server.h
    ├── server.cpp           ← POSIX TCP server
    ├── client_handler.h
    └── client_handler.cpp   ← RESP protocol + command dispatch
```

---

## Features

- **GET, SET, DEL, EXPIRE** commands over a raw TCP connection
- **RESP-style wire protocol** (compatible with `redis-cli` and `nc`)
- **Fixed-size thread pool** — N worker threads (defaults to hardware core count) handle concurrent clients without per-connection thread creation overhead
- **Reader-writer locking** (`std::shared_mutex`) — multiple reads proceed concurrently, writes are exclusive
- **TTL expiry** — keys expire lazily on access
- **AOF persistence** — every write is flushed to `appendonly.aof` and replayed on restart for zero data loss

---

## Benchmark

Measured with 8 concurrent clients, 1,000 ops each (custom Python benchmark):

| Operation | Throughput | p99 Latency |
|-----------|-----------|-------------|
| SET       | ~22,000 ops/sec | 1.04ms |
| GET       | ~22,000 ops/sec | 1.01ms |

---

## Build & Run

**Requirements:** C++17 compiler, CMake ≥ 3.15

```bash
git clone https://github.com/SambhavKansal03/kvstore.git
cd kvstore
mkdir build && cd build
cmake ..
make
./kvstore          # listens on port 6379
```

---

## Testing

**Connect with netcat:**
```bash
nc localhost 6379
```

**Supported commands:**
```
PING                        → +PONG
SET name sambhav            → +OK
GET name                    → +sambhav
SET temp bye 5000           → +OK  (expires in 5000ms)
EXPIRE name 1000            → :1
DEL name                    → :1
DBSIZE                      → :1
KEYS                        → *1 ...
FLUSHALL                    → +OK
```

**Test concurrency** — open two terminals with `nc localhost 6379` simultaneously. The server handles both without blocking. You'll see separate `fd` numbers in the server logs.

**Test crash recovery:**
```bash
# Run server, set some keys, kill with Ctrl+C
./kvstore
# In another terminal:
SET city delhi
SET name sambhav
^C

# Restart — keys survive
./kvstore
GET city     # → +delhi
GET name     # → +sambhav
```

---

## Run the Benchmark

```bash
# With the server running in Terminal 1:
python3 kvstore_benchmark.py

# Custom options:
python3 kvstore_benchmark.py --threads 16 --ops 2000
```

---

## Design Decisions

**Why a fixed thread pool instead of thread-per-connection?**  
Thread creation is expensive. A fixed pool reuses N threads across all connections, avoiding OS overhead on every new client. Workers sleep on a `std::condition_variable` and wake only when there's a task.

**Why `std::shared_mutex` instead of a regular mutex?**  
GET is a read — multiple clients can read concurrently without corrupting state. `shared_mutex` allows many simultaneous readers but gives writers exclusive access, improving throughput under read-heavy workloads.

**Why AOF instead of snapshots?**  
AOF appends every write command to a log file and flushes immediately. On restart, the log is replayed line by line to rebuild state. This gives strong durability guarantees with minimal complexity — the same model Redis uses by default.
