#include <arpa/inet.h>
#include <cerrno>
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
#include <unistd.h>
#include <vector>

namespace {
constexpr uint16_t DEFAULT_PORT = 4660;
constexpr double DEFAULT_TIMEOUT = 3.0;
constexpr uint32_t CURRENT_MAC = 0xFFFFFF12u;
constexpr uint32_t CURRENT_IP  = 0xFFFFFF18u;
constexpr uint32_t EEPROM_MAC  = 0xFFFFFC12u;
constexpr uint32_t EEPROM_IP   = 0xFFFFFC18u;

struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Timeout : Error {
    using Error::Error;
};

class RbcpClient {
    std::string host_;
    uint16_t port_;
    double timeout_;
    uint8_t id_ = 0;

public:
    RbcpClient(std::string host, uint16_t port, double timeout)
        : host_(std::move(host)), port_(port), timeout_(timeout) {}

    std::vector<uint8_t> read(uint32_t addr, uint8_t len) {
        const uint8_t id = id_++;
        const std::vector<uint8_t> request = {
            0xff,
            0xc0,
            id,
            len,
            static_cast<uint8_t>(addr >> 24),
            static_cast<uint8_t>(addr >> 16),
            static_cast<uint8_t>(addr >> 8),
            static_cast<uint8_t>(addr)
        };

        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        addrinfo* result = nullptr;
        const std::string port_string = std::to_string(port_);
        int rc = getaddrinfo(host_.c_str(), port_string.c_str(), &hints, &result);
        if (rc != 0) {
            throw Error(gai_strerror(rc));
        }

        const int fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (fd < 0) {
            freeaddrinfo(result);
            throw Error(strerror(errno));
        }

        if (sendto(fd, request.data(), request.size(), 0,
                   result->ai_addr, result->ai_addrlen) !=
            static_cast<ssize_t>(request.size())) {
            freeaddrinfo(result);
            close(fd);
            throw Error(strerror(errno));
        }
        freeaddrinfo(result);

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(fd, &read_fds);
        timeval tv{
            static_cast<long>(timeout_),
            static_cast<long>((timeout_ - static_cast<long>(timeout_)) * 1000000)
        };

        rc = select(fd + 1, &read_fds, nullptr, nullptr, &tv);
        if (rc == 0) {
            close(fd);
            throw Timeout("RBCP timeout");
        }
        if (rc < 0) {
            close(fd);
            throw Error(strerror(errno));
        }

        uint8_t reply[263];
        const ssize_t reply_size = recvfrom(fd, reply, sizeof(reply), 0, nullptr, nullptr);
        close(fd);

        if (reply_size < 8 || reply[0] != 0xff || reply[2] != id) {
            throw Error("invalid RBCP reply");
        }
        if ((reply[1] & 0x01u) != 0) {
            throw Error("RBCP bus error");
        }

        return {reply + 8, reply + reply_size};
    }
};

std::vector<uint8_t> read_retry(RbcpClient& client, uint32_t addr, uint8_t len) {
    for (int attempt = 0; attempt < 3; ++attempt) {
        try {
            return client.read(addr, len);
        } catch (const Timeout&) {
            if (attempt == 2) {
                throw;
            }
        }
    }
    throw Timeout("RBCP timeout");
}

std::string mac(const std::vector<uint8_t>& data) {
    if (data.size() != 6) {
        throw Error("short MAC read");
    }
    std::ostringstream out;
    out << std::hex << std::uppercase << std::setfill('0');
    for (size_t i = 0; i < data.size(); ++i) {
        if (i != 0) {
            out << ':';
        }
        out << std::setw(2) << static_cast<unsigned>(data[i]);
    }
    return out.str();
}

std::string ip(const std::vector<uint8_t>& data) {
    if (data.size() != 4) {
        throw Error("short IP read");
    }
    return std::to_string(data[0]) + "." +
           std::to_string(data[1]) + "." +
           std::to_string(data[2]) + "." +
           std::to_string(data[3]);
}

void usage(const char* program) {
    std::cerr << "Usage: " << program << " IP [--port N] [--timeout SEC]\n";
}
}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2 ||
            (argc == 2 &&
             (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help"))) {
            usage(argv[0]);
            return argc < 2 ? 2 : 0;
        }

        const std::string target = argv[1];
        uint16_t port = DEFAULT_PORT;
        double timeout = DEFAULT_TIMEOUT;

        for (int i = 2; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--port" && i + 1 < argc) {
                const auto value = std::stoul(argv[++i]);
                if (value == 0 || value > 65535) {
                    throw Error("invalid port");
                }
                port = static_cast<uint16_t>(value);
            } else if (arg == "--timeout" && i + 1 < argc) {
                timeout = std::stod(argv[++i]);
                if (timeout <= 0) {
                    throw Error("timeout must be positive");
                }
            } else {
                throw Error("unknown option: " + arg);
            }
        }

        RbcpClient client(target, port, timeout);
        const auto current_mac = read_retry(client, CURRENT_MAC, 6);
        const auto current_ip = read_retry(client, CURRENT_IP, 4);
        const auto eeprom_mac = read_retry(client, EEPROM_MAC, 6);
        const auto eeprom_ip = read_retry(client, EEPROM_IP, 4);

        std::cout
            << "target       : " << target << ':' << port << '\n'
            << "current MAC  : " << mac(current_mac) << '\n'
            << "current IP   : " << ip(current_ip) << '\n'
            << "EEPROM MAC   : " << mac(eeprom_mac) << '\n'
            << "EEPROM IP    : " << ip(eeprom_ip) << '\n'
            << "status       : READ OK\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
