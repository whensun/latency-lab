#include "latency_lab/command_processor.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

constexpr std::size_t max_line_length = 4096;

bool send_all(int client, std::string_view message) {
    std::size_t offset = 0;

    while (offset < message.size()) {
        const ssize_t count = send(
            client,
            message.data() + offset,
            message.size() - offset,
            MSG_NOSIGNAL
        );

        if (count < 0 && errno == EINTR) {
            continue;
        }

        if (count <= 0) {
            return false;
        }

        offset += static_cast<std::size_t>(count);
    }

    return true;
}

void serve_client(int client, latency_lab::CommandProcessor& processor) {
    std::string pending;
    char buffer[4096];

    while (true) {
        const ssize_t count = recv(client, buffer, sizeof(buffer), 0);

        if (count < 0 && errno == EINTR) {
            continue;
        }

        if (count <= 0) {
            return;
        }

        pending.append(buffer, static_cast<std::size_t>(count));
        std::size_t newline;

        while ((newline = pending.find('\n')) != std::string::npos) {
            if (newline > max_line_length) {
                send_all(client, "ERROR line too long\nEND\n");
                return;
            }

            const std::string line = pending.substr(0, newline);
            pending.erase(0, newline + 1);

            const auto result = processor.process(line);

            if (!send_all(client, result.output + "END\n")) {
                return;
            }

            if (result.quit) {
                return;
            }
        }

        if (pending.size() > max_line_length) {
            send_all(client, "ERROR line too long\nEND\n");
            return;
        }
    }
}

unsigned short parse_port(int argc, char** argv) {
    if (argc == 1) {
        return 9000;
    }

    if (argc != 2) {
        throw std::invalid_argument("Usage: latency_server [port]");
    }

    const std::string value = argv[1];

    if (value.empty() || !std::all_of(
        value.begin(),
        value.end(),
        [](unsigned char c) {
            return std::isdigit(c) != 0;
        }
    )) {
        throw std::invalid_argument("port must be between 0 and 65535");
    }

    const unsigned long number = std::stoul(value);

    if (number > 65535) {
        throw std::invalid_argument("port must be between 0 and 65535");
    }

    return static_cast<unsigned short>(number);
}

}

int main(int argc, char** argv) {
    try {
        const unsigned short port = parse_port(argc, argv);
        const int listener = socket(AF_INET, SOCK_STREAM, 0);

        if (listener < 0) {
            throw std::runtime_error(std::strerror(errno));
        }

        const int reuse = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);

        if (
            bind(listener, reinterpret_cast<sockaddr*>(&address),
                 sizeof(address)) < 0 ||
            listen(listener, 16) < 0
        ) {
            const std::string error = std::strerror(errno);
            close(listener);
            throw std::runtime_error(error);
        }

        socklen_t size = sizeof(address);

        if (getsockname(
            listener,
            reinterpret_cast<sockaddr*>(&address),
            &size
        ) < 0) {
            const std::string error = std::strerror(errno);
            close(listener);
            throw std::runtime_error(error);
        }

        std::cout << "LISTENING 127.0.0.1:"
                  << ntohs(address.sin_port) << std::endl;

        latency_lab::CommandProcessor processor;

        while (true) {
            const int client = accept(listener, nullptr, nullptr);

            if (client < 0 && errno == EINTR) {
                continue;
            }

            if (client < 0) {
                const std::string error = std::strerror(errno);
                close(listener);
                throw std::runtime_error(error);
            }

            serve_client(client, processor);
            close(client);
        }
    } catch (const std::exception& error) {
        std::cerr << "ERROR " << error.what() << '\n';
        return 1;
    }
}