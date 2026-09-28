# CacheForge

CacheForge is an in-memory key-value store built in C++20.

The current version provides a lightweight command-line interface supporting
SET, GET, DELETE, EXISTS, and SIZE operations, backed by a hash-based
in-memory storage engine.

## Current Features

- In-memory key-value storage
- SET and update operations
- GET operations
- DELETE operations
- Key existence checks
- Store size tracking
- C++20
- CMake build system
- Command-line interface

## Planned Features

- Automated unit testing with GoogleTest
- LRU cache eviction
- TTL-based key expiration
- Thread-safe concurrent access
- Thread pool
- TCP server and client
- Persistent append-only logging
- Crash recovery
- Performance benchmarking
- Docker support
- Continuous integration with GitHub Actions
