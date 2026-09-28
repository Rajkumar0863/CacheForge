#include "cacheforge/key_value_store.hpp"

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

int main() {
    cacheforge::KeyValueStore store;

    std::cout << "CacheForge CLI\n";
    std::cout << "Commands:\n";
    std::cout << "  SET <key> <value>\n";
    std::cout << "  SETEX <key> <ttl_seconds> <value>\n";
    std::cout << "  GET <key>\n";
    std::cout << "  DELETE <key>\n";
    std::cout << "  EXISTS <key>\n";
    std::cout << "  SIZE\n";
    std::cout << "  EXIT\n\n";

    std::string line;

    while (true) {
        std::cout << "> ";

        if (!std::getline(std::cin, line)) {
            break;
        }

        std::istringstream input(line);

        std::string command;
        input >> command;

        if (command == "SET") {
            std::string key;
            std::string value;

            input >> key >> value;

            if (key.empty() || value.empty()) {
                std::cout << "Usage: SET <key> <value>\n";
                continue;
            }

            store.set(key, value);

            std::cout << "OK\n";
        }

        else if (command == "SETEX") {
            std::string key;
            std::string value;
            long long ttl_seconds;

            if (!(input >> key >> ttl_seconds >> value)) {
                std::cout
                    << "Usage: SETEX <key> <ttl_seconds> <value>\n";
                continue;
            }

            if (ttl_seconds <= 0) {
                std::cout << "TTL must be greater than 0\n";
                continue;
            }

            store.set_with_ttl(
                key,
                value,
                std::chrono::seconds(ttl_seconds)
            );

            std::cout << "OK\n";
        }

        else if (command == "GET") {
            std::string key;

            input >> key;

            if (key.empty()) {
                std::cout << "Usage: GET <key>\n";
                continue;
            }

            auto value = store.get(key);

            if (value.has_value()) {
                std::cout << value.value() << '\n';
            } else {
                std::cout << "NOT_FOUND\n";
            }
        }

        else if (command == "DELETE") {
            std::string key;

            input >> key;

            if (key.empty()) {
                std::cout << "Usage: DELETE <key>\n";
                continue;
            }

            if (store.remove(key)) {
                std::cout << "DELETED\n";
            } else {
                std::cout << "NOT_FOUND\n";
            }
        }

        else if (command == "EXISTS") {
            std::string key;

            input >> key;

            if (key.empty()) {
                std::cout << "Usage: EXISTS <key>\n";
                continue;
            }

            std::cout
                << (store.contains(key) ? "true" : "false")
                << '\n';
        }

        else if (command == "SIZE") {
            std::cout << store.size() << '\n';
        }

        else if (command == "EXIT") {
            std::cout << "Goodbye.\n";
            break;
        }

        else if (command.empty()) {
            continue;
        }

        else {
            std::cout << "Unknown command\n";
        }
    }

    return 0;
}