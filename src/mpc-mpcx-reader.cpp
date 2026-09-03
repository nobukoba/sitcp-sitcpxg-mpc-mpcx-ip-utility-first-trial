#include <arpa/inet.h>

#include <algorithm>
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

constexpr double DEFAULT_TIMEOUT = 3.0;
constexpr uint16_t DEFAULT_PORT = 4660;
constexpr uint32_t EEPROM_BASE = 0xFFFFFC00u;
constexpr uint32_t XG_PROBE = 0xFFFFFF50u;
constexpr int FIELD_WIDTH = 20;

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
    Client(std::string host, uint16_t port, double timeout)
        : host_(std::move(host)), port_(port), timeout_(timeout) {}

    std::vector<uint8_t> read(uint32_t address, size_t length) {
        if (length > 255) {
            throw Error("one RBCP read is limited to 255 bytes");
        }

        const uint8_t id = id_++;
        std::vector<uint8_t> packet = {
            0xff,
            0xc0,
            id,
            static_cast<uint8_t>(length),
            static_cast<uint8_t>(address >> 24),
            static_cast<uint8_t>(address >> 16),
            static_cast<uint8_t>(address >> 8),
            static_cast<uint8_t>(address),
        };

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

        const ssize_t sent = sendto(fd,
                                    packet.data(),
                                    packet.size(),
                                    0,
                                    result->ai_addr,
                                    result->ai_addrlen);
        freeaddrinfo(result);
        if (sent != static_cast<ssize_t>(packet.size())) {
            close(fd);
            throw Error(strerror(errno));
        }

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(fd, &read_fds);

        timeval timeout_value{};
        timeout_value.tv_sec = static_cast<decltype(timeout_value.tv_sec)>(timeout_);
        timeout_value.tv_usec = static_cast<decltype(timeout_value.tv_usec)>(
            (timeout_ - static_cast<long>(timeout_)) * 1000000.0);

        const int ready = select(fd + 1, &read_fds, nullptr, nullptr, &timeout_value);
        if (ready == 0) {
            close(fd);
            throw Timeout("RBCP timeout");
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
        if ((reply[1] & 1u) != 0) {
            throw BusError("RBCP bus error");
        }

        return {reply + 8, reply + received};
    }

private:
    std::string host_;
    uint16_t port_;
    double timeout_;
    uint8_t id_ = 0;
};

std::vector<uint8_t> read_retry(Client& client, uint32_t address, size_t length) {
    for (int attempt = 0; attempt < 3; ++attempt) {
        try {
            return client.read(address, length);
        } catch (const Timeout&) {
            if (attempt == 2) {
                throw;
            }
        }
    }
    throw Timeout("timeout");
}

std::vector<uint8_t> read_exact(Client& client, uint32_t address, size_t length) {
    std::vector<uint8_t> out;
    for (size_t offset = 0; offset < length; offset += 8) {
        const size_t chunk = std::min<size_t>(8, length - offset);
        const auto block = read_retry(client, address + static_cast<uint32_t>(offset), chunk);
        if (block.size() != chunk) {
            throw Error("short read");
        }
        out.insert(out.end(), block.begin(), block.end());
    }
    return out;
}

bool valid_tag(std::vector<uint8_t> bytes) {
    if (bytes.size() != 7) {
        return false;
    }

    for (auto byte : bytes) {
        if (byte == 0 || byte == ' ' || byte == '-' || (byte >= '0' && byte <= '9')) {
            continue;
        }
        byte &= 0xdf;
        if (byte < 'A' || byte > 'Z') {
            return false;
        }
    }
    return true;
}

int classify(const std::vector<uint8_t>& data) {
    if (data.size() != 22) {
        return 0;
    }

    std::vector<uint8_t> normal_tag;
    std::vector<uint8_t> xg_tag;
    for (int i = 6; i < 13; ++i) {
        normal_tag.push_back(data[i] ? static_cast<uint8_t>(data[i] - 0x34) : 0);
    }
    for (int i = 0; i < 7; ++i) {
        xg_tag.push_back(data[i] ? static_cast<uint8_t>(data[i] - 0x2c) : 0);
    }

    if (valid_tag(normal_tag)) {
        return 2;
    }
    if (valid_tag(xg_tag)) {
        return 1;
    }
    return 0;
}

std::vector<uint8_t> reconstruct_xg(const std::vector<uint8_t>& eeprom) {
    std::vector<uint8_t> payload(eeprom.begin(), eeprom.begin() + 16);
    payload.insert(payload.end(), eeprom.begin() + 18, eeprom.begin() + 24);
    return payload;
}

std::vector<uint8_t> reconstruct_normal(const std::vector<uint8_t>& eeprom) {
    std::vector<uint8_t> payload(eeprom.begin() + 0x12, eeprom.begin() + 0x18);
    payload.insert(payload.end(), eeprom.begin() + 0x40, eeprom.begin() + 0x50);
    return payload;
}

std::string hex_bytes(const std::vector<uint8_t>& data,
                      size_t begin = 0,
                      size_t end = SIZE_MAX,
                      char separator = ' ') {
    end = std::min(end, data.size());
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (size_t i = begin; i < end; ++i) {
        if (i > begin) {
            out << separator;
        }
        out << std::setw(2) << static_cast<unsigned>(data[i]);
    }
    return out.str();
}

void field(const std::string& key, const std::string& value) {
    std::cout << std::left << std::setw(FIELD_WIDTH) << key << ": " << value << '\n';
}

std::string type_name(int type) {
    if (type == 1) {
        return "MPCX (SiTCP-XG)";
    }
    if (type == 2) {
        return "MPC (normal SiTCP)";
    }
    if (type == -1) {
        return "ambiguous";
    }
    return "unknown";
}

int detect_target(Client& client,
                  const std::vector<uint8_t>& eeprom,
                  std::string& reason) {
    const bool xg_valid = classify(reconstruct_xg(eeprom)) == 1;
    const bool normal_valid = classify(reconstruct_normal(eeprom)) == 2;

    if (xg_valid && !normal_valid) {
        reason = "EEPROM payload";
        return 1;
    }
    if (normal_valid && !xg_valid) {
        reason = "EEPROM payload";
        return 2;
    }
    if (!xg_valid && !normal_valid) {
        reason = "EEPROM payload not classified";
        return 0;
    }

    try {
        (void)read_retry(client, XG_PROBE, 1);
        reason = "XG register probe: readable";
        return 1;
    } catch (const BusError&) {
        reason = "XG register probe: bus error";
        return 2;
    } catch (const Timeout&) {
        reason = "XG register probe: timeout";
        return -1;
    }
}

void usage(const char* program) {
    std::cerr << "Usage: " << program << " <ip> [options]\n\n"
              << "Options:\n"
              << "  --port N       RBCP UDP port (default: " << DEFAULT_PORT << ")\n"
              << "  --timeout SEC  RBCP timeout in seconds (default: " << DEFAULT_TIMEOUT << ")\n"
              << "  -h, --help     Show this help\n";
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

        const std::string ip = argv[1];
        uint16_t port = DEFAULT_PORT;
        double timeout = DEFAULT_TIMEOUT;

        for (int i = 2; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--port" && i + 1 < argc) {
                const unsigned long value = std::stoul(argv[++i]);
                if (value == 0 || value > 65535) {
                    throw Error("invalid port");
                }
                port = static_cast<uint16_t>(value);
            } else if (option == "--timeout" && i + 1 < argc) {
                timeout = std::stod(argv[++i]);
                if (timeout <= 0) {
                    throw Error("timeout must be positive");
                }
            } else if (option == "-h" || option == "--help") {
                usage(argv[0]);
                return 0;
            } else {
                throw Error("unknown option: " + option);
            }
        }

        Client client(ip, port, timeout);
        const auto eeprom = read_exact(client, EEPROM_BASE, 0x50);

        std::string detection_reason;
        const int type = detect_target(client, eeprom, detection_reason);
        const auto payload = type == 1
                                 ? reconstruct_xg(eeprom)
                                 : type == 2 ? reconstruct_normal(eeprom)
                                             : std::vector<uint8_t>{};

        field("command", "read");
        field("target", ip + ":" + std::to_string(port));
        field("detected type", type_name(type));
        field("detection", detection_reason);

        if (!payload.empty()) {
            field("reconstructed payload", hex_bytes(payload));
        }
        if (type == 1) {
            field("MPCX FC00..FC0F", hex_bytes(eeprom, 0, 16));
            field("MAC", hex_bytes(eeprom, 0x12, 0x18, ':'));
        } else if (type == 2) {
            field("MAC", hex_bytes(eeprom, 0x12, 0x18, ':'));
            field("MPC FC40..FC4F", hex_bytes(eeprom, 0x40, 0x50));
        }

        field("EEPROM IP",
              std::to_string(eeprom[0x18]) + "." + std::to_string(eeprom[0x19]) + "." +
                  std::to_string(eeprom[0x1a]) + "." + std::to_string(eeprom[0x1b]));
        field("status", "READ OK");

        std::cout << "raw EEPROM FC00..FC4F:\n";
        for (size_t offset = 0; offset < eeprom.size(); offset += 16) {
            std::cout << std::hex << std::uppercase << std::setw(8) << std::setfill('0')
                      << (EEPROM_BASE + offset) << ": "
                      << hex_bytes(eeprom, offset, std::min(offset + 16, eeprom.size()))
                      << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
