cmake_minimum_required(VERSION 3.16)

project(CacheForge
    VERSION 0.1
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Core CacheForge library
add_library(cacheforge
    src/key_value_store.cpp
)

target_include_directories(cacheforge
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

# Command-line application
add_executable(cacheforge_cli
    src/main.cpp
)

target_link_libraries(cacheforge_cli
    PRIVATE
        cacheforge
)