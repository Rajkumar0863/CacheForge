#include "cacheforge/key_value_store.hpp"

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

int main() {
    cacheforge::KeyValueStore store;

    std::cout << "CacheForge CLI\n";
    std::cout << "Type HELP to see available commands.\n";

    std::string line;

    while (true) {
        std::cout << "> ";

        if (!std::getline(std::cin, line)) {
            break;
        }

        std::istringstream input(line);

        std::string command;
        input >> command;

        // -------------------------------------------------
        // Ignore empty input
        // -------------------------------------------------

        if (command.empty()) {
            continue;
        }

        // -------------------------------------------------
        // SET <key> <value>
        // -------------------------------------------------

        if (command == "SET") {
            std::string key;
            std::string value;

            if (!(input >> key >> value)) {
                std::cout
                    << "Usage: SET <key> <value>\n";

                continue;
            }

            store.set(key, value);

            std::cout << "OK\n";
        }

        // -------------------------------------------------
        // SETEX <key> <ttl_seconds> <value>
        // -------------------------------------------------

        else if (command == "SETEX") {
            std::string key;
            std::string value;

            long long ttl_seconds = 0;

            if (
                !(input >> key >> ttl_seconds >> value)
                || ttl_seconds <= 0
            ) {
                std::cout
                    << "Usage: SETEX <key> "
                    << "<ttl_seconds> <value>\n";

                continue;
            }

            store.set_with_ttl(
                key,
                value,
                std::chrono::seconds(ttl_seconds)
            );

            std::cout << "OK\n";
        }

        // -------------------------------------------------
        // GET <key>
        // -------------------------------------------------

        else if (command == "GET") {
            std::string key;

            if (!(input >> key)) {
                std::cout
                    << "Usage: GET <key>\n";

                continue;
            }

            auto value =
                store.get(key);

            if (value.has_value()) {
                std::cout
                    << value.value()
                    << '\n';
            } else {
                std::cout
                    << "NOT_FOUND\n";
            }
        }

        // -------------------------------------------------
        // DELETE <key>
        // -------------------------------------------------

        else if (command == "DELETE") {
            std::string key;

            if (!(input >> key)) {
                std::cout
                    << "Usage: DELETE <key>\n";

                continue;
            }

            if (store.remove(key)) {
                std::cout << "OK\n";
            } else {
                std::cout << "NOT_FOUND\n";
            }
        }

        // -------------------------------------------------
        // EXISTS <key>
        // -------------------------------------------------

        else if (command == "EXISTS") {
            std::string key;

            if (!(input >> key)) {
                std::cout
                    << "Usage: EXISTS <key>\n";

                continue;
            }

            std::cout
                << (
                    store.contains(key)
                        ? "true"
                        : "false"
                )
                << '\n';
        }

        // -------------------------------------------------
        // SIZE
        // -------------------------------------------------

        else if (command == "SIZE") {
            std::cout
                << store.size()
                << '\n';
        }

        // -------------------------------------------------
        // SAVE <filename>
        // -------------------------------------------------

        else if (command == "SAVE") {
            std::string filename;

            if (!(input >> filename)) {
                std::cout
                    << "Usage: SAVE <filename>\n";

                continue;
            }

            if (store.save(filename)) {
                std::cout << "OK\n";
            } else {
                std::cout << "ERROR\n";
            }
        }

        // -------------------------------------------------
        // LOAD <filename>
        // -------------------------------------------------

        else if (command == "LOAD") {
            std::string filename;

            if (!(input >> filename)) {
                std::cout
                    << "Usage: LOAD <filename>\n";

                continue;
            }

            if (store.load(filename)) {
                std::cout << "OK\n";
            } else {
                std::cout << "ERROR\n";
            }
        }

        // -------------------------------------------------
        // HELP
        // -------------------------------------------------

        else if (command == "HELP") {
            std::cout
                << "\nAvailable commands:\n"
                << "  SET <key> <value>\n"
                << "  SETEX <key> <ttl_seconds> <value>\n"
                << "  GET <key>\n"
                << "  DELETE <key>\n"
                << "  EXISTS <key>\n"
                << "  SIZE\n"
                << "  SAVE <filename>\n"
                << "  LOAD <filename>\n"
                << "  HELP\n"
                << "  EXIT\n\n";
        }

        // -------------------------------------------------
        // EXIT
        // -------------------------------------------------

        else if (
            command == "EXIT"
            || command == "QUIT"
        ) {
            std::cout
                << "Goodbye.\n";

            break;
        }

        // -------------------------------------------------
        // Unknown command
        // -------------------------------------------------

        else {
            std::cout
                << "Unknown command. "
                << "Type HELP for available commands.\n";
        }
    }

    return 0;
}