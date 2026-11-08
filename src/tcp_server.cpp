#include "latency_lab/command_processor.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdlib>
#include <netinet/tcp.h>

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <condition_variable>
#include <cstring>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

namespace {

constexpr std::size_t max_line_length = 4096;

class MatchingWorker {
public:
    MatchingWorker() : worker_([this] { run(); }) {}

    ~MatchingWorker() {
        {
            std::lock_guard lock(mutex_);
            stopping_ = true;
        }
        ready_.notify_one();
        worker_.join();
    }

    MatchingWorker(const MatchingWorker&) = delete;
    MatchingWorker& operator=(const MatchingWorker&) = delete;

    std::future<latency_lab::CommandResult> submit(std::string line) {
        std::packaged_task<latency_lab::CommandResult()> task(
            [this, line = std::move(line)]() mutable {
                return processor_.process(std::move(line));
            }
        );
        auto result = task.get_future();

        {
            std::lock_guard lock(mutex_);
            queue_.push(std::move(task));
        }
        ready_.notify_one();
        return result;
    }

private:
    void run() {
        while (true) {
            std::packaged_task<latency_lab::CommandResult()> task;

            {
                std::unique_lock lock(mutex_);
                ready_.wait(lock, [this] {
                    return stopping_ || !queue_.empty();
                });

                if (queue_.empty() && stopping_) {
                    return;
                }

                task = std::move(queue_.front());
                queue_.pop();
            }

            task();
        }
    }

    latency_lab::CommandProcessor processor_;
    std::mutex mutex_;
    std::condition_variable ready_;
    std::queue<std::packaged_task<latency_lab::CommandResult()>> queue_;
    bool stopping_ = false;
    std::thread worker_;
};

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

void serve_client(int client, const std::shared_ptr<MatchingWorker>& worker) {
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

            std::string line = pending.substr(0, newline);
            pending.erase(0, newline + 1);

            const auto result = worker->submit(std::move(line)).get();

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
        value.begin(), value.end(), [](unsigned char c) {
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
            listener, reinterpret_cast<sockaddr*>(&address), &size
        ) < 0) {
            const std::string error = std::strerror(errno);
            close(listener);
            throw std::runtime_error(error);
        }

        std::cout << "LISTENING 127.0.0.1:"
                  << ntohs(address.sin_port) << std::endl;

        auto worker = std::make_shared<MatchingWorker>();

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
            
            const char* setting = std::getenv("LATENCY_TCP_NODELAY");
            
            if (setting != nullptr && std::string_view(setting) == "1") {
                const int enabled = 1;
            
                if (setsockopt(
                    client,
                    IPPROTO_TCP,
                    TCP_NODELAY,
                    &enabled,
                    sizeof(enabled)
                ) != 0) {
                    std::cerr << "TCP_NODELAY ERROR "
                              << std::strerror(errno) << '\n';
                    close(client);
                    continue;
                }
            }
            
            try {
                std::thread([client, worker] {
                    try {
                        serve_client(client, worker);
                    } catch (const std::exception& error) {
                        std::cerr << "CLIENT ERROR " << error.what() << '\n';
                    }
                    close(client);
                }).detach();
            } catch (const std::system_error& error) {
                std::cerr << "CLIENT THREAD ERROR "
                          << error.what() << '\n';
                close(client);
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "ERROR " << error.what() << '\n';
        return 1;
    }
}
