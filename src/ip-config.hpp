#pragma once

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <netdb.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace ipconfig {

constexpr uint16_t DEFAULT_PORT = 4660;
constexpr double DEFAULT_TIMEOUT = 3.0;
constexpr uint32_t CURRENT_MAC = 0xFFFFFF12u;
constexpr uint32_t CURRENT_IP  = 0xFFFFFF18u;
constexpr uint32_t EEPROM_MAC  = 0xFFFFFC12u;
constexpr uint32_t EEPROM_IP   = 0xFFFFFC18u;
constexpr uint32_t EEPROM_WE   = 0xFFFFFCFFu;

struct Error : std::runtime_error { using std::runtime_error::runtime_error; };
struct Timeout : Error { using Error::Error; };

class Client {
public:
    Client(std::string host, uint16_t port = DEFAULT_PORT, double timeout = DEFAULT_TIMEOUT)
        : host_(std::move(host)), port_(port), timeout_(timeout) {}

    std::vector<uint8_t> read(uint32_t address, uint8_t length) {
        return transaction(0xC0, address, {}, length);
    }

    void write(uint32_t address, const std::vector<uint8_t>& data) {
        if (data.empty() || data.size() > 255) throw Error("invalid RBCP write length");
        const auto reply = transaction(0x80, address, data, static_cast<uint8_t>(data.size()));
        if (reply.size() != data.size()) throw Error("short RBCP write acknowledgement");
    }

private:
    std::vector<uint8_t> transaction(uint8_t command, uint32_t address,
                                     const std::vector<uint8_t>& payload, uint8_t length) {
        const uint8_t id = id_++;
        std::vector<uint8_t> packet = {
            0xff, command, id, length,
            static_cast<uint8_t>(address >> 24),
            static_cast<uint8_t>(address >> 16),
            static_cast<uint8_t>(address >> 8),
            static_cast<uint8_t>(address)
        };
        packet.insert(packet.end(), payload.begin(), payload.end());

        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        addrinfo* result = nullptr;
        const std::string port_string = std::to_string(port_);
        const int gai = getaddrinfo(host_.c_str(), port_string.c_str(), &hints, &result);
        if (gai != 0) throw Error(gai_strerror(gai));

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
        timeval tv{
            static_cast<long>(timeout_),
            static_cast<long>((timeout_ - static_cast<long>(timeout_)) * 1000000.0)
        };
        const int rv = select(fd + 1, &read_fds, nullptr, nullptr, &tv);
        if (rv == 0) {
            close(fd);
            throw Timeout("RBCP timeout from " + host_ + ":" + std::to_string(port_));
        }
        if (rv < 0) {
            close(fd);
            throw Error(strerror(errno));
        }

        uint8_t reply[263];
        const ssize_t n = recvfrom(fd, reply, sizeof(reply), 0, nullptr, nullptr);
        close(fd);
        if (n < 8 || reply[0] != 0xff || reply[2] != id) throw Error("invalid RBCP reply");
        if (reply[1] & 0x01u) throw Error("RBCP bus error");
        return {reply + 8, reply + n};
    }

    std::string host_;
    uint16_t port_;
    double timeout_;
    uint8_t id_ = 0;
};

inline std::vector<uint8_t> read_retry(Client& client, uint32_t address, uint8_t length) {
    for (int attempt = 0; attempt < 3; ++attempt) {
        try {
            auto data = client.read(address, length);
            if (data.size() != length) throw Error("short RBCP read");
            return data;
        } catch (const Timeout&) {
            if (attempt == 2) throw;
        }
    }
    throw Timeout("RBCP timeout");
}

inline std::vector<uint8_t> parse_ipv4(const std::string& value) {
    in_addr address{};
    if (inet_pton(AF_INET, value.c_str(), &address) != 1)
        throw Error("invalid IPv4 address: " + value);
    const auto* p = reinterpret_cast<const uint8_t*>(&address.s_addr);
    return {p[0], p[1], p[2], p[3]};
}

inline std::string ipv4(const std::vector<uint8_t>& data) {
    if (data.size() != 4) throw Error("invalid IP read length");
    return std::to_string(data[0]) + "." + std::to_string(data[1]) + "." +
           std::to_string(data[2]) + "." + std::to_string(data[3]);
}

inline std::string mac(const std::vector<uint8_t>& data) {
    if (data.size() != 6) throw Error("invalid MAC read length");
    std::ostringstream out;
    out << std::hex << std::uppercase << std::setfill('0');
    for (size_t i = 0; i < data.size(); ++i) {
        if (i) out << ':';
        out << std::setw(2) << static_cast<unsigned>(data[i]);
    }
    return out.str();
}

struct Snapshot {
    std::vector<uint8_t> current_mac;
    std::vector<uint8_t> current_ip;
    std::vector<uint8_t> eeprom_mac;
    std::vector<uint8_t> eeprom_ip;
};

inline Snapshot read_snapshot(Client& client) {
    return {
        read_retry(client, CURRENT_MAC, 6),
        read_retry(client, CURRENT_IP, 4),
        read_retry(client, EEPROM_MAC, 6),
        read_retry(client, EEPROM_IP, 4)
    };
}

inline void print_snapshot(const Snapshot& s, const std::string& prefix = "") {
    std::cout << prefix << "current MAC  : " << mac(s.current_mac) << '\n'
              << prefix << "current IP   : " << ipv4(s.current_ip) << '\n'
              << prefix << "EEPROM MAC   : " << mac(s.eeprom_mac) << '\n'
              << prefix << "EEPROM IP    : " << ipv4(s.eeprom_ip) << '\n';
}

inline Snapshot show_all(const std::string& host, uint16_t port, double timeout,
                         const std::string& prefix = "") {
    Client client(host, port, timeout);
    const auto snapshot = read_snapshot(client);
    print_snapshot(snapshot, prefix);
    return snapshot;
}

inline void write_eeprom_ip(const std::string& host, const std::string& new_ip,
                            uint16_t port, double timeout) {
    Client client(host, port, timeout);
    const auto bytes = parse_ipv4(new_ip);
    client.write(EEPROM_WE, {0x00});
    try {
        client.write(EEPROM_IP, bytes);
    } catch (...) {
        try { client.write(EEPROM_WE, {0xff}); } catch (...) {}
        throw;
    }
    client.write(EEPROM_WE, {0xff});
    if (read_retry(client, EEPROM_IP, 4) != bytes)
        throw Error("EEPROM IP read-back mismatch");
}

inline void write_current_ip(const std::string& host, const std::string& new_ip,
                             uint16_t port, double timeout) {
    const auto bytes = parse_ipv4(new_ip);
    Client old_client(host, port, timeout);
    try {
        old_client.write(CURRENT_IP, bytes);
    } catch (const Timeout&) {
        // The address can change before the acknowledgement reaches the old address.
        // Do not retry a potentially successful destructive write.
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    Client new_client(new_ip, port, timeout);
    if (read_retry(new_client, CURRENT_IP, 4) != bytes)
        throw Error("current IP read-back mismatch at new address " + new_ip);
}

}  // namespace ipconfig
