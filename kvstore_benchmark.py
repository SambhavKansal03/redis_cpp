#!/usr/bin/env python3
"""
kvstore_benchmark.py
Benchmarks the kvstore server with concurrent clients.

Measures:
  - Throughput: ops/sec for SET and GET
  - Latency:    mean, p50, p95, p99 in milliseconds

Usage:
  python3 kvstore_benchmark.py
"""

import socket
import time
import threading
import statistics
import argparse

# ── Config ────────────────────────────────────────────────
HOST         = "127.0.0.1"
PORT         = 6379
NUM_THREADS  = 8       # concurrent clients
OPS_PER_THREAD = 1000  # commands each client fires
VALUE        = "benchmarkvalue"
# ─────────────────────────────────────────────────────────


def send_command(sock, command: str) -> str:
    """Send one command, return the response (strips CRLF)."""
    sock.sendall((command + "\r\n").encode())
    return sock.recv(4096).decode().strip()


def run_worker(thread_id: int, op: str, results: list, errors: list):
    """
    One worker thread: opens a connection, fires OPS_PER_THREAD commands,
    records latency of each one, then closes the connection.
    """
    latencies = []
    try:
        with socket.create_connection((HOST, PORT), timeout=5) as sock:
            for i in range(OPS_PER_THREAD):
                key = f"bench:t{thread_id}:k{i}"

                t0 = time.perf_counter()
                if op == "SET":
                    send_command(sock, f"SET {key} {VALUE}")
                else:
                    send_command(sock, f"GET {key}")
                t1 = time.perf_counter()

                latencies.append((t1 - t0) * 1000)  # convert to ms
    except Exception as e:
        errors.append(str(e))

    results.append(latencies)


def benchmark(op: str):
    """
    Spawn NUM_THREADS workers concurrently, collect all latencies,
    then compute and print stats.
    """
    print(f"\n{'─'*50}")
    print(f"  Benchmarking {op}  "
          f"({NUM_THREADS} threads × {OPS_PER_THREAD} ops)")
    print(f"{'─'*50}")

    results = []   # list of latency lists, one per thread
    errors  = []
    threads = []

    wall_start = time.perf_counter()

    for t in range(NUM_THREADS):
        th = threading.Thread(
            target=run_worker,
            args=(t, op, results, errors)
        )
        threads.append(th)
        th.start()

    for th in threads:
        th.join()

    wall_end = time.perf_counter()

    # Flatten all latency samples
    all_latencies = [lat for worker in results for lat in worker]
    total_ops     = len(all_latencies)
    wall_time     = wall_end - wall_start
    throughput    = total_ops / wall_time

    if not all_latencies:
        print("  No results — is the server running?")
        return

    all_latencies.sort()
    mean = statistics.mean(all_latencies)
    p50  = all_latencies[int(0.50 * len(all_latencies))]
    p95  = all_latencies[int(0.95 * len(all_latencies))]
    p99  = all_latencies[int(0.99 * len(all_latencies))]
    p100 = all_latencies[-1]

    print(f"  Total ops   : {total_ops:,}")
    print(f"  Wall time   : {wall_time:.2f}s")
    print(f"  Throughput  : {throughput:,.0f} ops/sec  ← use this number")
    print(f"")
    print(f"  Latency (ms):")
    print(f"    mean : {mean:.3f}")
    print(f"    p50  : {p50:.3f}")
    print(f"    p95  : {p95:.3f}")
    print(f"    p99  : {p99:.3f}  ← use this number")
    print(f"    max  : {p100:.3f}")

    if errors:
        print(f"\n  ⚠ {len(errors)} error(s): {errors[:3]}")


def warmup():
    """Pre-populate keys so GET benchmark has something to read."""
    print("\n  Warming up (pre-loading keys)...")
    try:
        with socket.create_connection((HOST, PORT), timeout=5) as sock:
            for t in range(NUM_THREADS):
                for i in range(OPS_PER_THREAD):
                    key   = f"bench:t{t}:k{i}"
                    send_command(sock, f"SET {key} {VALUE}")
        print("  Warmup done.")
    except Exception as e:
        print(f"  Warmup failed — is the server running on port {PORT}? ({e})")
        exit(1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="kvstore benchmark")
    parser.add_argument("--threads", type=int, default=NUM_THREADS)
    parser.add_argument("--ops",     type=int, default=OPS_PER_THREAD)
    args = parser.parse_args()

    NUM_THREADS    = args.threads
    OPS_PER_THREAD = args.ops

    print("=" * 50)
    print("  kvstore benchmark")
    print(f"  {NUM_THREADS} clients × {OPS_PER_THREAD} ops each")
    print("=" * 50)

    warmup()
    benchmark("SET")
    benchmark("GET")

    print(f"\n{'='*50}")
    print("  Done. Copy the ops/sec and p99 numbers")
    print("  into your resume bullets.")
    print(f"{'='*50}\n")