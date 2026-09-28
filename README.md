# CacheForge

A concurrent in-memory key-value store built in **C++20** with **LRU eviction, TTL expiration, thread-safe operations, disk persistence, automated testing, and performance benchmarking**.

CacheForge was built to explore the engineering concepts behind caching systems such as eviction policies, expiration, synchronization, persistence, and performance measurement while keeping the implementation small enough to understand end-to-end.

---

## Features

- In-memory key-value storage
- `SET`, `GET`, `DELETE`, and `EXISTS` operations
- Configurable cache capacity
- Least Recently Used (LRU) eviction
- Time-To-Live (TTL) expiration
- Thread-safe concurrent access
- Persistent save/load support
- LRU ordering preserved across persistence
- TTL-aware persistence across process restarts
- Interactive command-line interface
- 37 automated GoogleTest tests
- Release-mode performance benchmark suite
- CMake-based build system
- C++20

---

## Architecture

CacheForge combines a hash table with a doubly linked list to provide fast key lookup while maintaining LRU ordering.

```text
                        KeyValueStore
                             |
             +---------------+---------------+
             |               |               |
             v               v               v
      unordered_map      LRU list        TTL metadata
         data_          lru_order_          expiry_
             |               |               |
             +---------------+---------------+
                             |
                           mutex
                             |
                  thread-safe operations
                             |
              +--------------+--------------+
              |                             |
              v                             v
         Persistence                    CLI / API
        SAVE / LOAD                 SET / GET / ...
```

### Core structures

The main cache uses:

```cpp
std::unordered_map<std::string, Entry>
```

for key lookup and:

```cpp
std::list<std::string>
```

for LRU ordering.

Each cache entry stores an iterator into the LRU list, allowing an accessed entry to be moved without searching the entire list.

TTL expiration metadata is maintained separately using:

```cpp
std::unordered_map<
    std::string,
    std::chrono::steady_clock::time_point
>
```

---

## LRU Eviction

CacheForge supports an optional maximum capacity.

```cpp
cacheforge::KeyValueStore store(3);
```

A capacity of `0` means unlimited capacity.

When the configured capacity is exceeded, the least recently used entry is removed.

Example:

```text
SET A 100
SET B 200
SET C 300

GET A

SET D 400
```

Before the `GET`:

```text
LRU -> A -> B -> C -> MRU
```

After `GET A`:

```text
LRU -> B -> C -> A -> MRU
```

Adding `D` therefore evicts `B`:

```text
LRU -> C -> A -> D -> MRU
```

Both reads and updates affect recency.

---

## TTL Expiration

Entries can be stored with a Time-To-Live using `SETEX`.

```text
SETEX session 30 abc123
```

This stores:

```text
key   = session
value = abc123
TTL   = 30 seconds
```

After expiration:

```text
GET session
NOT_FOUND
```

Expired entries are treated as missing and are cleaned from the cache when encountered or during operations that perform expiration cleanup.

A normal `SET` on an existing TTL-enabled key removes its previous expiration.

---

## Thread Safety

CacheForge supports access from multiple threads.

Shared state is protected using:

```cpp
std::mutex
```

Public cache operations acquire the mutex before accessing shared structures.

Private helper functions assume the caller already owns the lock. This avoids attempting to lock the same non-recursive mutex twice.

Protected state includes:

```text
data_
lru_order_
expiry_
```

### Why not `std::shared_mutex`?

A `GET` operation is not completely read-only in an LRU cache.

Successful reads modify the LRU ordering:

```text
GET key
   |
   +--> lookup value
   |
   +--> move key to MRU position
```

TTL checks may also remove expired entries.

For the current architecture, operations therefore use an exclusive mutex.

This keeps synchronization straightforward and correct, although it can create lock contention under heavily concurrent workloads.

---

## Persistence

CacheForge can save the current cache to disk:

```text
SAVE cache.db
```

and restore it later:

```text
LOAD cache.db
```

Example:

```text
SET name Rajkumar
SET language C++
SET project CacheForge

SAVE cache.db
EXIT
```

After restarting CacheForge:

```text
LOAD cache.db

GET name
Rajkumar

GET language
C++

GET project
CacheForge
```

The cache state therefore survives process termination.

### Persistence format

Persistence files include a format marker:

```text
CACHEFORGE_V1
```

Keys and values are serialized using quoted strings, allowing values containing spaces to round-trip through the persistence layer.

### LRU persistence

Entries are written in LRU order.

When the cache is restored, that ordering is reconstructed so eviction behaviour remains consistent after a restart.

### TTL persistence

CacheForge internally uses:

```cpp
std::chrono::steady_clock
```

for runtime TTL handling.

A `steady_clock::time_point` cannot safely be serialized and reused by a different process.

During `SAVE`, CacheForge therefore converts the remaining TTL into an absolute system-clock expiration timestamp.

During `LOAD`:

```text
saved expiration timestamp
          |
          v
compare with current wall clock
          |
    +-----+-----+
    |           |
 expired      valid
    |           |
  discard    calculate
             remaining TTL
                 |
                 v
        new steady_clock deadline
```

This means TTL continues to elapse while CacheForge is not running.

---

## Command-Line Interface

Start the CLI:

### Windows

```powershell
.\build\Release\cacheforge_cli.exe
```

Available commands:

| Command | Description |
|---|---|
| `SET <key> <value>` | Insert or update a value |
| `SETEX <key> <ttl> <value>` | Insert/update with TTL in seconds |
| `GET <key>` | Retrieve a value |
| `DELETE <key>` | Remove a key |
| `EXISTS <key>` | Check whether a key exists |
| `SIZE` | Return number of valid entries |
| `SAVE <filename>` | Persist the cache to disk |
| `LOAD <filename>` | Restore cache state from disk |
| `HELP` | Display available commands |
| `EXIT` | Exit CacheForge |

Example:

```text
CacheForge CLI
Type HELP to see available commands.

> SET language C++
OK

> GET language
C++

> SETEX session 5 abc123
OK

> EXISTS session
true

> SIZE
2

> SAVE cache.db
OK

> EXIT
Goodbye.
```

---

## Building CacheForge

### Requirements

- C++20-compatible compiler
- CMake 3.16+
- Git

The project has been developed and tested using MSVC on Windows.

Clone the repository:

```bash
git clone https://github.com/Rajkumar0863/CacheForge.git
cd CacheForge
```

Configure:

```bash
cmake -S . -B build
```

Build a Release configuration:

```bash
cmake --build build --config Release
```

CMake produces three main executables:

```text
cacheforge_cli
cacheforge_tests
cacheforge_benchmark
```

On a Visual Studio/MSVC build they are typically located under:

```text
build/Release/
```

---

## Testing

CacheForge uses **GoogleTest**.

GoogleTest is retrieved automatically through CMake `FetchContent`.

Run the test suite:

```bash
ctest --test-dir build -C Release --output-on-failure
```

Current test suite:

```text
37 automated tests
```

The suite covers:

- basic key-value operations
- updates and deletion
- missing keys
- empty strings
- cache size
- LRU eviction
- LRU updates after reads
- LRU updates after writes
- capacity enforcement
- TTL insertion
- TTL expiration
- TTL replacement
- interaction between TTL and LRU
- concurrent writers
- concurrent readers and writers
- concurrent updates to the same key
- concurrent set/get/remove operations
- capacity under concurrent writes
- concurrent TTL writes
- save/load round trips
- persistence of multiple entries
- replacement of existing state during load
- missing persistence files
- corrupted persistence files
- keys and values containing spaces
- LRU preservation across persistence
- capacity enforcement during load
- TTL persistence
- TTL expiration while CacheForge is stopped

---

## Benchmarking

CacheForge contains a standalone benchmark executable:

```text
cacheforge_benchmark
```

Build in **Release mode** before benchmarking:

```bash
cmake --build build --config Release
```

Run on Windows:

```powershell
.\build\Release\cacheforge_benchmark.exe
```

The benchmark suite measures:

- single-threaded SET throughput
- single-threaded GET throughput
- mixed 80% GET / 20% SET workload
- concurrent SET throughput
- concurrent GET throughput
- average GET latency
- GET p50 / p95 / p99 latency
- average SET latency
- SET p50 / p95 / p99 latency

The current benchmark configuration uses:

```text
Throughput operations : 500,000
Latency samples       : 50,000
Concurrent threads    : 8
```

A warm-up workload is executed before measurements.

### Example measured result

One Release-mode run produced approximately:

| Workload | Throughput |
|---|---:|
| SET | 1.01M ops/sec |
| GET | 2.78M ops/sec |
| Mixed 80/20 | 2.90M ops/sec |
| Concurrent SET — 8 threads | 309K ops/sec |
| Concurrent GET — 8 threads | 3.38M ops/sec |

Latency from the same run:

| Operation | Average | p50 | p95 | p99 |
|---|---:|---:|---:|---:|
| GET | 0.201 μs | 0.200 μs | 0.400 μs | 0.600 μs |
| SET | 0.644 μs | 0.400 μs | 0.700 μs | 1.200 μs |

> Benchmark results are machine- and environment-dependent. These measurements represent a local Release build and should not be interpreted as universal CacheForge performance.

The benchmark suite is intended primarily for comparing implementation changes under the same environment.

---

## Concurrency Performance

An interesting result from the benchmark is the difference between concurrent reads and writes.

CacheForge currently uses one mutex protecting the cache's shared state.

Concurrent writes therefore compete for the same lock:

```text
Thread 1 ----\
Thread 2 -----\
Thread 3 ------> mutex -> cache
Thread 4 -----/
...          /
Thread 8 ----/
```

This prioritizes a simple correctness model over maximum write scalability.

It also creates a clear optimization path for future versions, such as:

- cache sharding
- per-shard locks
- alternative LRU designs
- reduced critical sections

Benchmarking these changes against the current implementation would allow performance improvements to be measured rather than assumed.

---

## Complexity

Typical operations have the following expected complexity:

| Operation | Expected Complexity |
|---|---:|
| `SET` | O(1) average |
| `GET` | O(1) average |
| `DELETE` | O(1) average |
| `EXISTS` | O(1) average |
| LRU update | O(1) |
| Single LRU eviction | O(1) |
| TTL lookup | O(1) average |
| `SIZE` | O(t), where t is tracked TTL entries due to expiration cleanup |
| `SAVE` | O(n) |
| `LOAD` | O(n) expected |

The O(1) average lookup characteristics come from `std::unordered_map`, while `std::list::splice` allows LRU positions to be updated without copying or searching the list.

---

## Project Structure

```text
CacheForge/
|
+-- benchmarks/
|   +-- cache_benchmark.cpp
|
+-- include/
|   +-- cacheforge/
|       +-- key_value_store.hpp
|
+-- src/
|   +-- key_value_store.cpp
|   +-- main.cpp
|
+-- tests/
|   +-- key_value_store_test.cpp
|
+-- .gitignore
+-- CMakeLists.txt
+-- LICENSE
+-- README.md
```

---

## Design Goals

CacheForge focuses on a few engineering principles:

### Correctness before optimization

Concurrency was introduced with a simple mutex-based synchronization model before attempting more complicated locking strategies.

### Measurable performance

A dedicated benchmark suite measures throughput and latency so performance changes can be evaluated empirically.

### Separation of concerns

The core `KeyValueStore` is implemented as a reusable library.

The CLI, tests, and benchmark suite are separate executables that link against the same core implementation.

### Testability

Features are accompanied by automated tests, including interactions between LRU, TTL, persistence, capacity limits, and concurrency.

---

## Future Improvements

Potential extensions include:

- cache sharding for improved concurrent write scalability
- configurable persistence paths
- automatic periodic snapshots
- background TTL cleanup
- more efficient persistence formats
- crash-safe atomic persistence
- network protocol / TCP server
- command pipelining
- richer benchmark workloads
- Linux CI builds
- sanitizers and static analysis
- GitHub Actions continuous integration

---

## What I Learned

Building CacheForge involved more than implementing a hash map.

The project required reasoning about interactions between several systems concepts:

- maintaining O(1) LRU updates
- coordinating multiple data structures safely
- avoiding recursive mutex deadlocks
- handling expiration under concurrent access
- understanding the difference between `steady_clock` and wall-clock time
- preserving TTL semantics across process restarts
- restoring LRU state from persistent storage
- designing tests for nondeterministic concurrent operations
- distinguishing correctness tests from performance benchmarks
- interpreting lock contention through measured benchmark results

These interactions are where much of the engineering complexity of the project lies.

---

## License

This project is licensed under the MIT License.

See [`LICENSE`](LICENSE) for details.

---

## Author

**Rajkumar Vijayan**

MSc Software Development (International Systems)  
University of Limerick, Ireland

GitHub: [Rajkumar0863](https://github.com/Rajkumar0863)