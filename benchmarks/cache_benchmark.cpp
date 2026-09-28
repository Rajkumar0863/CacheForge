#include "cacheforge/key_value_store.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

using cacheforge::KeyValueStore;

namespace {

using Clock = std::chrono::steady_clock;

constexpr std::size_t OPERATION_COUNT = 500000;
constexpr std::size_t LATENCY_SAMPLE_COUNT = 50000;
constexpr int THREAD_COUNT = 8;


// ============================================================
// BENCHMARK RESULT
// ============================================================

struct BenchmarkResult {
    std::string name;

    std::size_t operations;

    double elapsed_seconds;

    double operations_per_second;
};


// ============================================================
// LATENCY RESULT
// ============================================================

struct LatencyResult {
    double average_microseconds;
    double p50_microseconds;
    double p95_microseconds;
    double p99_microseconds;
};


// ============================================================
// UTILITY FUNCTIONS
// ============================================================

double duration_seconds(
    Clock::time_point start,
    Clock::time_point end
) {
    return std::chrono::duration<double>(
        end - start
    ).count();
}


double duration_microseconds(
    Clock::time_point start,
    Clock::time_point end
) {
    return std::chrono::duration<
        double,
        std::micro
    >(
        end - start
    ).count();
}


double percentile(
    const std::vector<double>& sorted_values,
    double p
) {
    if (sorted_values.empty()) {
        return 0.0;
    }

    const double index =
        p * static_cast<double>(
            sorted_values.size() - 1
        );

    const auto lower =
        static_cast<std::size_t>(
            std::floor(index)
        );

    const auto upper =
        static_cast<std::size_t>(
            std::ceil(index)
        );

    if (lower == upper) {
        return sorted_values[lower];
    }

    const double fraction =
        index - static_cast<double>(lower);

    return
        sorted_values[lower]
        + fraction
            * (
                sorted_values[upper]
                - sorted_values[lower]
            );
}


LatencyResult calculate_latency(
    std::vector<double> latencies
) {
    if (latencies.empty()) {
        return {
            0.0,
            0.0,
            0.0,
            0.0
        };
    }

    const double total =
        std::accumulate(
            latencies.begin(),
            latencies.end(),
            0.0
        );

    const double average =
        total
        / static_cast<double>(
            latencies.size()
        );

    std::sort(
        latencies.begin(),
        latencies.end()
    );

    return {
        average,
        percentile(latencies, 0.50),
        percentile(latencies, 0.95),
        percentile(latencies, 0.99)
    };
}


void print_result(
    const BenchmarkResult& result
) {
    std::cout
        << std::left
        << std::setw(30)
        << result.name
        << std::right
        << std::setw(15)
        << result.operations
        << std::setw(18)
        << std::fixed
        << std::setprecision(3)
        << result.elapsed_seconds
        << std::setw(22)
        << std::fixed
        << std::setprecision(0)
        << result.operations_per_second
        << '\n';
}


void print_latency(
    const std::string& name,
    const LatencyResult& latency
) {
    std::cout
        << '\n'
        << name
        << '\n';

    std::cout
        << "  Average : "
        << std::fixed
        << std::setprecision(3)
        << latency.average_microseconds
        << " us\n";

    std::cout
        << "  p50     : "
        << latency.p50_microseconds
        << " us\n";

    std::cout
        << "  p95     : "
        << latency.p95_microseconds
        << " us\n";

    std::cout
        << "  p99     : "
        << latency.p99_microseconds
        << " us\n";
}


// ============================================================
// WARM-UP
// ============================================================

void warm_up() {
    KeyValueStore store;

    constexpr std::size_t warm_up_operations =
        50000;

    for (
        std::size_t i = 0;
        i < warm_up_operations;
        ++i
    ) {
        store.set(
            "warm_" + std::to_string(i),
            "value"
        );
    }

    for (
        std::size_t i = 0;
        i < warm_up_operations;
        ++i
    ) {
        store.get(
            "warm_" + std::to_string(i)
        );
    }
}


// ============================================================
// SET THROUGHPUT
// ============================================================

BenchmarkResult benchmark_set() {
    KeyValueStore store;

    const auto start =
        Clock::now();

    for (
        std::size_t i = 0;
        i < OPERATION_COUNT;
        ++i
    ) {
        store.set(
            "key_" + std::to_string(i),
            "value_" + std::to_string(i)
        );
    }

    const auto end =
        Clock::now();

    const double seconds =
        duration_seconds(start, end);

    return {
        "SET throughput",
        OPERATION_COUNT,
        seconds,
        static_cast<double>(OPERATION_COUNT)
            / seconds
    };
}


// ============================================================
// GET THROUGHPUT
// ============================================================

BenchmarkResult benchmark_get() {
    KeyValueStore store;

    for (
        std::size_t i = 0;
        i < OPERATION_COUNT;
        ++i
    ) {
        store.set(
            "key_" + std::to_string(i),
            "value"
        );
    }

    const auto start =
        Clock::now();

    for (
        std::size_t i = 0;
        i < OPERATION_COUNT;
        ++i
    ) {
        auto value =
            store.get(
                "key_" + std::to_string(i)
            );

        // Prevent the operation from becoming completely
        // irrelevant to the optimizer.
        if (!value.has_value()) {
            std::cerr
                << "Unexpected missing key.\n";
        }
    }

    const auto end =
        Clock::now();

    const double seconds =
        duration_seconds(start, end);

    return {
        "GET throughput",
        OPERATION_COUNT,
        seconds,
        static_cast<double>(OPERATION_COUNT)
            / seconds
    };
}


// ============================================================
// MIXED WORKLOAD
// ============================================================

BenchmarkResult benchmark_mixed() {
    KeyValueStore store;

    constexpr std::size_t initial_entries =
        100000;

    for (
        std::size_t i = 0;
        i < initial_entries;
        ++i
    ) {
        store.set(
            "key_" + std::to_string(i),
            "value"
        );
    }

    const auto start =
        Clock::now();

    for (
        std::size_t i = 0;
        i < OPERATION_COUNT;
        ++i
    ) {
        // 80% GET
        // 20% SET
        if (i % 5 == 0) {
            store.set(
                "key_"
                    + std::to_string(
                        i % initial_entries
                    ),
                "updated"
            );
        } else {
            auto value =
                store.get(
                    "key_"
                    + std::to_string(
                        i % initial_entries
                    )
                );

            if (!value.has_value()) {
                std::cerr
                    << "Unexpected missing key.\n";
            }
        }
    }

    const auto end =
        Clock::now();

    const double seconds =
        duration_seconds(start, end);

    return {
        "Mixed workload (80/20)",
        OPERATION_COUNT,
        seconds,
        static_cast<double>(OPERATION_COUNT)
            / seconds
    };
}


// ============================================================
// CONCURRENT WRITE BENCHMARK
// ============================================================

BenchmarkResult benchmark_concurrent_writes() {
    KeyValueStore store;

    const std::size_t operations_per_thread =
        OPERATION_COUNT
        / static_cast<std::size_t>(
            THREAD_COUNT
        );

    const std::size_t total_operations =
        operations_per_thread
        * static_cast<std::size_t>(
            THREAD_COUNT
        );

    std::atomic<bool> start_flag{false};

    std::vector<std::thread> threads;

    threads.reserve(THREAD_COUNT);

    for (
        int thread_id = 0;
        thread_id < THREAD_COUNT;
        ++thread_id
    ) {
        threads.emplace_back(
            [
                &store,
                &start_flag,
                thread_id,
                operations_per_thread
            ]() {
                while (
                    !start_flag.load(
                        std::memory_order_acquire
                    )
                ) {
                    std::this_thread::yield();
                }

                for (
                    std::size_t i = 0;
                    i < operations_per_thread;
                    ++i
                ) {
                    store.set(
                        "thread_"
                            + std::to_string(thread_id)
                            + "_key_"
                            + std::to_string(i),
                        "value"
                    );
                }
            }
        );
    }

    const auto start =
        Clock::now();

    start_flag.store(
        true,
        std::memory_order_release
    );

    for (auto& thread : threads) {
        thread.join();
    }

    const auto end =
        Clock::now();

    const double seconds =
        duration_seconds(start, end);

    return {
        "Concurrent SET (8 threads)",
        total_operations,
        seconds,
        static_cast<double>(
            total_operations
        ) / seconds
    };
}


// ============================================================
// CONCURRENT READ BENCHMARK
// ============================================================

BenchmarkResult benchmark_concurrent_reads() {
    KeyValueStore store;

    constexpr std::size_t key_count =
        100000;

    for (
        std::size_t i = 0;
        i < key_count;
        ++i
    ) {
        store.set(
            "key_" + std::to_string(i),
            "value"
        );
    }

    const std::size_t operations_per_thread =
        OPERATION_COUNT
        / static_cast<std::size_t>(
            THREAD_COUNT
        );

    const std::size_t total_operations =
        operations_per_thread
        * static_cast<std::size_t>(
            THREAD_COUNT
        );

    std::atomic<bool> start_flag{false};

    std::atomic<std::size_t> successful_reads{0};

    std::vector<std::thread> threads;

    threads.reserve(THREAD_COUNT);

    for (
        int thread_id = 0;
        thread_id < THREAD_COUNT;
        ++thread_id
    ) {
        threads.emplace_back(
            [
                &store,
                &start_flag,
                &successful_reads,
                thread_id,
                operations_per_thread
            ]() {
                while (
                    !start_flag.load(
                        std::memory_order_acquire
                    )
                ) {
                    std::this_thread::yield();
                }

                std::size_t local_successes = 0;

                for (
                    std::size_t i = 0;
                    i < operations_per_thread;
                    ++i
                ) {
                    const std::size_t key_index =
                        (
                            i
                            + static_cast<std::size_t>(
                                thread_id
                            )
                        )
                        % key_count;

                    auto value =
                        store.get(
                            "key_"
                            + std::to_string(
                                key_index
                            )
                        );

                    if (value.has_value()) {
                        ++local_successes;
                    }
                }

                successful_reads.fetch_add(
                    local_successes,
                    std::memory_order_relaxed
                );
            }
        );
    }

    const auto start =
        Clock::now();

    start_flag.store(
        true,
        std::memory_order_release
    );

    for (auto& thread : threads) {
        thread.join();
    }

    const auto end =
        Clock::now();

    if (
        successful_reads.load()
        != total_operations
    ) {
        std::cerr
            << "Warning: some concurrent reads failed.\n";
    }

    const double seconds =
        duration_seconds(start, end);

    return {
        "Concurrent GET (8 threads)",
        total_operations,
        seconds,
        static_cast<double>(
            total_operations
        ) / seconds
    };
}


// ============================================================
// GET LATENCY
// ============================================================

LatencyResult benchmark_get_latency() {
    KeyValueStore store;

    constexpr std::size_t key_count =
        10000;

    for (
        std::size_t i = 0;
        i < key_count;
        ++i
    ) {
        store.set(
            "latency_key_"
                + std::to_string(i),
            "value"
        );
    }

    std::vector<double> latencies;

    latencies.reserve(
        LATENCY_SAMPLE_COUNT
    );

    for (
        std::size_t i = 0;
        i < LATENCY_SAMPLE_COUNT;
        ++i
    ) {
        const std::string key =
            "latency_key_"
            + std::to_string(
                i % key_count
            );

        const auto start =
            Clock::now();

        auto value =
            store.get(key);

        const auto end =
            Clock::now();

        if (!value.has_value()) {
            std::cerr
                << "Unexpected missing key.\n";
        }

        latencies.push_back(
            duration_microseconds(
                start,
                end
            )
        );
    }

    return calculate_latency(
        std::move(latencies)
    );
}


// ============================================================
// SET LATENCY
// ============================================================

LatencyResult benchmark_set_latency() {
    KeyValueStore store;

    std::vector<double> latencies;

    latencies.reserve(
        LATENCY_SAMPLE_COUNT
    );

    for (
        std::size_t i = 0;
        i < LATENCY_SAMPLE_COUNT;
        ++i
    ) {
        const std::string key =
            "latency_set_"
            + std::to_string(i);

        const auto start =
            Clock::now();

        store.set(
            key,
            "value"
        );

        const auto end =
            Clock::now();

        latencies.push_back(
            duration_microseconds(
                start,
                end
            )
        );
    }

    return calculate_latency(
        std::move(latencies)
    );
}

} // namespace


// ============================================================
// MAIN
// ============================================================

int main() {
    std::cout
        << "============================================\n"
        << "          CacheForge Benchmark Suite        \n"
        << "============================================\n\n";

    std::cout
        << "Operations per throughput benchmark : "
        << OPERATION_COUNT
        << '\n';

    std::cout
        << "Latency samples                    : "
        << LATENCY_SAMPLE_COUNT
        << '\n';

    std::cout
        << "Concurrent threads                 : "
        << THREAD_COUNT
        << "\n\n";

    std::cout
        << "Warming up...\n";

    warm_up();

    std::cout
        << "Warm-up complete.\n\n";


    // --------------------------------------------------------
    // Throughput benchmarks
    // --------------------------------------------------------

    std::cout
        << std::left
        << std::setw(30)
        << "Benchmark"
        << std::right
        << std::setw(15)
        << "Operations"
        << std::setw(18)
        << "Time (sec)"
        << std::setw(22)
        << "Operations/sec"
        << '\n';

    std::cout
        << std::string(85, '-')
        << '\n';

    const auto set_result =
        benchmark_set();

    print_result(set_result);


    const auto get_result =
        benchmark_get();

    print_result(get_result);


    const auto mixed_result =
        benchmark_mixed();

    print_result(mixed_result);


    const auto concurrent_write_result =
        benchmark_concurrent_writes();

    print_result(
        concurrent_write_result
    );


    const auto concurrent_read_result =
        benchmark_concurrent_reads();

    print_result(
        concurrent_read_result
    );


    // --------------------------------------------------------
    // Latency benchmarks
    // --------------------------------------------------------

    std::cout
        << "\n============================================\n"
        << "                 Latency                    \n"
        << "============================================\n";

    const auto get_latency =
        benchmark_get_latency();

    print_latency(
        "GET latency",
        get_latency
    );


    const auto set_latency =
        benchmark_set_latency();

    print_latency(
        "SET latency",
        set_latency
    );


    std::cout
        << "\n============================================\n"
        << "Benchmark complete.\n"
        << "============================================\n";

    return 0;
}