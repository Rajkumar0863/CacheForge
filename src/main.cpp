#include "cacheforge/key_value_store.hpp"

#include <iostream>
#include <string>

int main() {
    cacheforge::KeyValueStore store;

    std::cout << "CacheForge v0.1\n";
    std::cout << "Commands: SET, GET, DELETE, EXISTS, SIZE, EXIT\n\n";

    std::string command;

    while (true) {
        std::cout << "> ";
        std::cin >> command;

        if (command == "SET") {
            std::string key;
            std::string value;

            std::cin >> key >> value;
            store.set(key, value);

            std::cout << "OK\n";
        }
        else if (command == "GET") {
            std::string key;
            std::cin >> key;

            auto value = store.get(key);

            if (value.has_value()) {
                std::cout << value.value() << "\n";
            } else {
                std::cout << "NOT_FOUND\n";
            }
        }
        else if (command == "DELETE") {
            std::string key;
            std::cin >> key;

            if (store.remove(key)) {
                std::cout << "DELETED\n";
            } else {
                std::cout << "NOT_FOUND\n";
            }
        }
        else if (command == "EXISTS") {
            std::string key;
            std::cin >> key;

            std::cout
                << (store.contains(key) ? "true" : "false")
                << "\n";
        }
        else if (command == "SIZE") {
            std::cout << store.size() << "\n";
        }
        else if (command == "EXIT") {
            std::cout << "Goodbye.\n";
            break;
        }
        else {
            std::cout << "UNKNOWN_COMMAND\n";
        }
    }

    return 0;
}