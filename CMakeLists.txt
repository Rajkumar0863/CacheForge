cmake_minimum_required(VERSION 3.16)

project(CacheForge
    VERSION 0.1
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# ============================================================
# CacheForge core library
# ============================================================

add_library(cacheforge
    src/key_value_store.cpp
)

target_include_directories(cacheforge
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)


# ============================================================
# Command-line application
# ============================================================

add_executable(cacheforge_cli
    src/main.cpp
)

target_link_libraries(cacheforge_cli
    PRIVATE
        cacheforge
)


# ============================================================
# Benchmark executable
# ============================================================

add_executable(cacheforge_benchmark
    benchmarks/cache_benchmark.cpp
)

target_link_libraries(cacheforge_benchmark
    PRIVATE
        cacheforge
)


# ============================================================
# Testing
# ============================================================

include(CTest)

if(BUILD_TESTING)

    include(FetchContent)

    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    )

    # Required for GoogleTest when using MSVC on Windows.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)

    add_executable(cacheforge_tests
        tests/key_value_store_test.cpp
    )

    target_link_libraries(cacheforge_tests
        PRIVATE
            cacheforge
            GTest::gtest_main
    )

    include(GoogleTest)

    gtest_discover_tests(cacheforge_tests)

endif()