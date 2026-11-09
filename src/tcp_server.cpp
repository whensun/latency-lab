
#include "latency_lab/command_processor.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <condition_variable>
#include <cstdlib>
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
#include <utility>
#include <vector>

namespace {

constexpr std::size_t max_line_length = 4096;
constexpr std::size_t max_batch_commands = 32;

struct BatchResult {
    std::string output;
    bool quit = false;
};

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

    std::future<BatchResult> submit_batch(
        std::vector<std::string> commands
    ) {
        std::packaged_task<BatchResult()> task(
            [this, commands = std::move(commands)]() mutable {
                BatchResult batch;

                for (auto& command : commands) {
                    auto result = processor_.process(
                        std::move(command)
                    );

                    batch.output += result.output;
                    batch.output += "END\n";

                    if (result.quit) {
                        batch.quit = true;
                        break;
                    }
                }

                return batch;
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
            std::packaged_task<BatchResult()> task;

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
    std::queue<std::packaged_task<BatchResult()>> queue_;
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

void serve_client(
    int client,
    const std::shared_ptr<MatchingWorker>& worker,
    bool batching_enabled
) {
    std::string pending;
    char buffer[4096];

    const std::size_t batch_limit = batching_enabled
        ? max_batch_commands
        : 1;

    while (true) {
        if (pending.find('\n') == std::string::npos) {
            if (pending.size() > max_line_length) {
                send_all(client, "ERROR line too long\nEND\n");
                return;
            }

            const ssize_t count = recv(
                client,
                buffer,
                sizeof(buffer),
                0
            );

            if (count < 0 && errno == EINTR) {
                continue;
            }

            if (count <= 0) {
                return;
            }

            pending.append(
                buffer,
                static_cast<std::size_t>(count)
            );
        }

        std::vector<std::string> commands;
        commands.reserve(batch_limit);

        bool too_long = false;

        while (commands.size() < batch_limit) {
            const std::size_t newline = pending.find('\n');

            if (newline == std::string::npos) {
                break;
            }

            if (newline > max_line_length) {
                too_long = true;
                break;
            }

            commands.push_back(pending.substr(0, newline));
            pending.erase(0, newline + 1);
        }

        if (!commands.empty()) {
            const auto result = worker->submit_batch(
                std::move(commands)
            ).get();

            if (!send_all(client, result.output)) {
                return;
            }

            if (result.quit) {
                return;
            }
        }

        if (
            too_long ||
            (
                pending.find('\n') == std::string::npos &&
                pending.size() > max_line_length
            )
        ) {
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
        throw std::invalid_argument(
            "Usage: latency_server [port]"
        );
    }

    const std::string value = argv[1];

    if (
        value.empty() ||
        !std::all_of(
            value.begin(),
            value.end(),
            [](unsigned char c) {
                return std::isdigit(c) != 0;
            }
        )
    ) {
        throw std::invalid_argument(
            "port must be between 0 and 65535"
        );
    }

    const unsigned long number = std::stoul(value);

    if (number > 65535) {
        throw std::invalid_argument(
            "port must be between 0 and 65535"
        );
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

        setsockopt(
            listener,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse,
            sizeof(reuse)
        );

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);

        if (
            bind(
                listener,
                reinterpret_cast<sockaddr*>(&address),
                sizeof(address)
            ) < 0 ||
            listen(listener, 16) < 0
        ) {
            const std::string error = std::strerror(errno);
            close(listener);
            throw std::runtime_error(error);
        }

        socklen_t size = sizeof(address);

        if (
            getsockname(
                listener,
                reinterpret_cast<sockaddr*>(&address),
                &size
            ) < 0
        ) {
            const std::string error = std::strerror(errno);
            close(listener);
            throw std::runtime_error(error);
        }

        std::cout
            << "LISTENING 127.0.0.1:"
            << ntohs(address.sin_port)
            << std::endl;

        auto worker = std::make_shared<MatchingWorker>();

        const char* batch_setting = std::getenv(
            "LATENCY_WORKER_BATCH"
        );

        const bool batching_enabled =
            batch_setting != nullptr &&
            std::string_view(batch_setting) == "1";

        while (true) {
            const int client = accept(
                listener,
                nullptr,
                nullptr
            );

            if (client < 0 && errno == EINTR) {
                continue;
            }

            if (client < 0) {
                const std::string error = std::strerror(errno);
                close(listener);
                throw std::runtime_error(error);
            }

            const char* setting = std::getenv(
                "LATENCY_TCP_NODELAY"
            );

            if (
                setting != nullptr &&
                std::string_view(setting) == "1"
            ) {
                const int enabled = 1;

                if (
                    setsockopt(
                        client,
                        IPPROTO_TCP,
                        TCP_NODELAY,
                        &enabled,
                        sizeof(enabled)
                    ) != 0
                ) {
                    std::cerr
                        << "TCP_NODELAY ERROR "
                        << std::strerror(errno)
                        << '\n';

                    close(client);
                    continue;
                }
            }

            try {
                std::thread(
                    [client, worker, batching_enabled] {
                        try {
                            serve_client(
                                client,
                                worker,
                                batching_enabled
                            );
                        } catch (const std::exception& error) {
                            std::cerr
                                << "CLIENT ERROR "
                                << error.what()
                                << '\n';
                        }

                        close(client);
                    }
                ).detach();

            } catch (const std::system_error& error) {
                std::cerr
                    << "CLIENT THREAD ERROR "
                    << error.what()
                    << '\n';

                close(client);
            }
        }

    } catch (const std::exception& error) {
        std::cerr << "ERROR " << error.what() << '\n';
        return 1;
    }
}
