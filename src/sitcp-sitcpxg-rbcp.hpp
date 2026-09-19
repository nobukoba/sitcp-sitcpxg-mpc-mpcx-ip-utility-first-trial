#pragma once

#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <netdb.h>
#include <stdexcept>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace sitcp_sitcpxg {
namespace rbcp {

constexpr uint16_t DEFAULT_PORT = 4660;
constexpr double DEFAULT_TIMEOUT = 3.0;

struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Timeout : Error {
    using Error::Error;
};

struct BusError : Error {
    using Error::Error;
};

class Client {
public:
    Client(std::string host, uint16_t port = DEFAULT_PORT,
           double timeout = DEFAULT_TIMEOUT)
        : host_(std::move(host)), port_(port), timeout_(timeout) {}

    std::vector<uint8_t> read(uint32_t address, size_t length) {
        if (length == 0 || length > 255) {
            throw Error("invalid RBCP read length");
        }
        return transaction(0xC0, address, {}, static_cast<uint8_t>(length));
    }

    std::vector<uint8_t> write(uint32_t address, const std::vector<uint8_t>& data) {
        if (data.empty() || data.size() > 255) {
            throw Error("invalid RBCP write length");
        }
        return transaction(0x80, address, data, static_cast<uint8_t>(data.size()));
    }

private:
    std::vector<uint8_t> transaction(uint8_t command, uint32_t address,
                                     const std::vector<uint8_t>& payload, uint8_t length) {
        const uint8_t id = id_++;
        std::vector<uint8_t> packet = {0xff, command, id, length,
            static_cast<uint8_t>(address >> 24), static_cast<uint8_t>(address >> 16),
            static_cast<uint8_t>(address >> 8), static_cast<uint8_t>(address)};
        packet.insert(packet.end(), payload.begin(), payload.end());

        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        addrinfo* result = nullptr;
        const std::string port_string = std::to_string(port_);
        const int gai = getaddrinfo(host_.c_str(), port_string.c_str(), &hints, &result);
        if (gai != 0) {
            throw Error(gai_strerror(gai));
        }
        const int fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (fd < 0) {
            freeaddrinfo(result);
            throw Error(strerror(errno));
        }
        const ssize_t sent = sendto(fd, packet.data(), packet.size(), 0,
                                    result->ai_addr, result->ai_addrlen);
        freeaddrinfo(result);
        if (sent != static_cast<ssize_t>(packet.size())) {
            close(fd);
            throw Error(strerror(errno));
        }

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(fd, &read_fds);
        timeval tv{};
        tv.tv_sec = static_cast<decltype(tv.tv_sec)>(timeout_);
        tv.tv_usec = static_cast<decltype(tv.tv_usec)>((timeout_ - static_cast<long>(timeout_)) * 1000000.0);
        const int ready = select(fd + 1, &read_fds, nullptr, nullptr, &tv);
        if (ready == 0) {
            close(fd);
            throw Timeout("RBCP timeout from " + host_ + ":" +
                          std::to_string(port_));
        }
        if (ready < 0) {
            close(fd);
            throw Error(strerror(errno));
        }

        uint8_t reply[263];
        const ssize_t received = recvfrom(fd, reply, sizeof(reply), 0, nullptr, nullptr);
        close(fd);
        if (received < 8 || reply[0] != 0xff || reply[2] != id) {
            throw Error("invalid RBCP reply");
        }
        if ((reply[1] & 0x01u) != 0) {
            throw BusError("RBCP bus error");
        }
        return std::vector<uint8_t>(reply + 8, reply + received);
    }

    std::string host_;
    uint16_t port_;
    double timeout_;
    uint8_t id_ = 0;
};

inline std::vector<uint8_t> read_retry(Client& client, uint32_t address,
                                       size_t length, int attempts = 3) {
    for (int attempt = 0; attempt < attempts; ++attempt) {
        try {
            auto data = client.read(address, length);
            if (data.size() != length) throw Error("short RBCP read");
            return data;
        } catch (const Timeout&) {
            if (attempt + 1 == attempts) throw;
        }
    }
    throw Timeout("RBCP timeout");
}

} // namespace rbcp
} // namespace sitcp_sitcpxg
