#include <arpa/inet.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fstream>
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
constexpr uint32_t EEPROM_BASE = 0xFFFFFC00u;
constexpr uint32_t EEPROM_WRITE_ENABLE = 0xFFFFFCFFu;
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

std::string hex_bytes(const std::vector<uint8_t>& data, char sep = ' ') {
    std::ostringstream os;
    os << std::hex << std::setfill('0');
    for (size_t i = 0; i < data.size(); ++i) {
        if (i != 0) {
            os << sep;
        }
        os << std::setw(2) << static_cast<unsigned>(data[i]);
    }
    return os.str();
}

std::string hex_address(uint32_t address) {
    std::ostringstream os;
    os << "0x" << std::hex << std::setw(8) << std::setfill('0') << address;
    return os.str();
}

void field(const std::string& key, const std::string& value) {
    std::cout << std::left << std::setw(FIELD_WIDTH) << key << ": " << value << '\n';
}

class RbcpClient {
public:
    RbcpClient(std::string host, uint16_t port, double timeout)
        : host_(std::move(host)), port_(port), timeout_(timeout) {}

    std::vector<uint8_t> read(uint32_t address, size_t length) {
        if (length > 255) {
            throw Error("one RBCP read is limited to 255 bytes");
        }
        return transaction(0xC0, address, {}, static_cast<uint8_t>(length));
    }

    std::vector<uint8_t> write(uint32_t address, const std::vector<uint8_t>& data) {
        if (data.size() > 255) {
            throw Error("one RBCP write is limited to 255 bytes");
        }
        return transaction(0x80, address, data, static_cast<uint8_t>(data.size()));
    }

private:
    std::vector<uint8_t> transaction(uint8_t command,
                                     uint32_t address,
                                     const std::vector<uint8_t>& payload,
                                     uint8_t length) {
        const uint8_t id = id_++;
        std::vector<uint8_t> packet = {
            0xff,
            command,
            id,
            length,
            static_cast<uint8_t>(address >> 24),
            static_cast<uint8_t>(address >> 16),
            static_cast<uint8_t>(address >> 8),
            static_cast<uint8_t>(address),
        };
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
        timeval tv{
            static_cast<long>(timeout_),
            static_cast<long>((timeout_ - static_cast<long>(timeout_)) * 1000000.0),
        };

        const int rv = select(fd + 1, &read_fds, nullptr, nullptr, &tv);
        if (rv == 0) {
            close(fd);
            throw Timeout("RBCP timeout");
        }
        if (rv < 0) {
            close(fd);
            throw Error(strerror(errno));
        }

        uint8_t reply[263];
        const ssize_t n = recvfrom(fd, reply, sizeof(reply), 0, nullptr, nullptr);
        close(fd);

        if (n < 8 || reply[0] != 0xff || reply[2] != id) {
            throw Error("invalid RBCP reply");
        }
        if ((reply[1] & 1u) != 0) {
            throw BusError("RBCP bus error");
        }

        return {reply + 8, reply + n};
    }

    std::string host_;
    uint16_t port_;
    double timeout_;
    uint8_t id_ = 0;
};

std::vector<uint8_t> read_retry(RbcpClient& client, uint32_t address, size_t length) {
    for (int attempt = 0; attempt < 3; ++attempt) {
        try {
            return client.read(address, length);
        } catch (const Timeout&) {
            if (attempt == 2) {
                throw;
            }
        }
    }
    throw Timeout("RBCP timeout");
}

std::vector<uint8_t> read_exact(RbcpClient& client, uint32_t address, size_t length) {
    std::vector<uint8_t> out;
    for (size_t offset = 0; offset < length; offset += 8) {
        const size_t chunk_length = std::min<size_t>(8, length - offset);
        auto block = read_retry(client,
                                address + static_cast<uint32_t>(offset),
                                chunk_length);
        if (block.size() != chunk_length) {
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

    for (size_t i = 6; i < 13; ++i) {
        normal_tag.push_back(data[i] ? static_cast<uint8_t>(data[i] - 0x34) : 0);
    }
    for (size_t i = 0; i < 7; ++i) {
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

std::string type_name(int type) {
    if (type == 1) {
        return "MPCX (SiTCP-XG)";
    }
    if (type == 2) {
        return "MPC (normal SiTCP)";
    }
    return "unknown";
}

std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw Error("cannot open file: " + path);
    }
    return {(std::istreambuf_iterator<char>(file)), {}};
}

uint32_t parse_u32(const std::string& value) {
    size_t parsed = 0;
    const unsigned long number = std::stoul(value, &parsed, 0);
    if (parsed != value.size() || number > 0xffffffffUL) {
        throw Error("invalid integer: " + value);
    }
    return static_cast<uint32_t>(number);
}

std::vector<uint8_t> parse_hex(std::string value) {
    for (char& c : value) {
        if (c == ',' || c == ':') {
            c = ' ';
        }
    }

    std::istringstream input(value);
    std::vector<uint8_t> out;
    std::string token;

    while (input >> token) {
        if (token.rfind("0x", 0) == 0 || token.rfind("0X", 0) == 0) {
            token = token.substr(2);
        }
        const unsigned long number = std::stoul(token, nullptr, 16);
        if (number > 255) {
            throw Error("hex byte out of range: " + token);
        }
        out.push_back(static_cast<uint8_t>(number));
    }

    if (out.empty()) {
        throw Error("no hex bytes supplied");
    }
    return out;
}

struct TargetArgs {
    std::string ip;
    uint16_t port = DEFAULT_PORT;
    double timeout = DEFAULT_TIMEOUT;
};

TargetArgs parse_target(int argc, char** argv, int start) {
    if (start >= argc) {
        throw Error("missing IP address");
    }

    TargetArgs args;
    args.ip = argv[start++];

    for (int i = start; i < argc; ++i) {
        const std::string option = argv[i];
        if (option == "--port" && i + 1 < argc) {
            const unsigned long port = std::stoul(argv[++i]);
            if (port == 0 || port > 65535) {
                throw Error("invalid port");
            }
            args.port = static_cast<uint16_t>(port);
        } else if (option == "--timeout" && i + 1 < argc) {
            args.timeout = std::stod(argv[++i]);
            if (args.timeout <= 0) {
                throw Error("timeout must be positive");
            }
        } else {
            throw Error("unknown option: " + option);
        }
    }

    return args;
}

std::vector<char*> make_argv(std::vector<std::string>& args) {
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (auto& arg : args) {
        argv.push_back(arg.data());
    }
    return argv;
}

void usage(const char* program) {
    std::cerr << "Usage: " << program << " COMMAND ...\n\n"
              << "Commands:\n"
              << "  inspect FILE\n"
              << "  read IP [--port N] [--timeout SEC]\n"
              << "  verify IP FILE [--port N] [--timeout SEC]\n"
              << "  mpcx-plan IP FILE [--port N] [--timeout SEC]\n"
              << "  probe IP ADDRESS [LENGTH] [--port N] [--timeout SEC]\n"
              << "  rbcp-read IP ADDRESS LENGTH [--port N] [--timeout SEC]\n"
              << "  rbcp-write IP ADDRESS HEX-BYTES [--port N] [--timeout SEC]\n"
              << "  clear IP --yes-really-clear [--port N] [--timeout SEC]\n"
              << "  write IP FILE [--port N] [--timeout SEC]"
              << "  (use mpc-mpcx-ip-writer)\n\n"
              << "Defaults:\n"
              << "  --port N       RBCP UDP port (default: " << DEFAULT_PORT << ")\n"
              << "  --timeout SEC  RBCP timeout in seconds (default: " << DEFAULT_TIMEOUT << ")\n"
              << "  probe LENGTH   bytes to read (default: 1)\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2 || std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") {
            usage(argv[0]);
            return argc < 2 ? 2 : 0;
        }

        const std::string command = argv[1];

        if (command == "inspect") {
            if (argc != 3) {
                throw Error("usage: inspect FILE");
            }

            const auto payload = read_file(argv[2]);
            const int type = classify(payload);
            field("command", "inspect");
            field("file", argv[2]);
            field("size", std::to_string(payload.size()) + " bytes");
            field("payload type", type_name(type));
            field("writer type", std::to_string(type));
            field("payload", hex_bytes(payload));
            return type != 0 ? 0 : 2;
        }

        if (command == "read") {
            const auto args = parse_target(argc, argv, 2);
            RbcpClient client(args.ip, args.port, args.timeout);
            const auto eeprom = read_exact(client, EEPROM_BASE, 0x50);
            field("command", "read");
            field("target", args.ip + ":" + std::to_string(args.port));
            field("EEPROM FC00..FC4F", hex_bytes(eeprom));
            field("status", "READ OK");
            return 0;
        }

        if (command == "probe") {
            if (argc < 4) {
                throw Error("usage: probe IP ADDRESS [LENGTH]");
            }

            const std::string ip = argv[2];
            const uint32_t address = parse_u32(argv[3]);
            size_t length = 1;
            int option_start = 4;

            if (option_start < argc && std::string(argv[option_start]).rfind("--", 0) != 0) {
                length = std::stoul(argv[option_start++], nullptr, 0);
            }

            std::vector<std::string> target_args = {argv[0], ip};
            for (int i = option_start; i < argc; ++i) {
                target_args.emplace_back(argv[i]);
            }
            auto target_argv = make_argv(target_args);
            const auto args = parse_target(
                static_cast<int>(target_argv.size()), target_argv.data(), 1);

            RbcpClient client(args.ip, args.port, args.timeout);
            const auto data = client.read(address, length);
            field("command", "probe");
            field("target", args.ip + ":" + std::to_string(args.port));
            field("address", hex_address(address));
            field("data", hex_bytes(data));
            field("status", "RBCP REACHABLE");
            return 0;
        }

        if (command == "rbcp-read") {
            if (argc < 5) {
                throw Error("usage: rbcp-read IP ADDRESS LENGTH");
            }

            const std::string ip = argv[2];
            const uint32_t address = parse_u32(argv[3]);
            const size_t length = std::stoul(argv[4], nullptr, 0);

            std::vector<std::string> target_args = {argv[0], ip};
            for (int i = 5; i < argc; ++i) {
                target_args.emplace_back(argv[i]);
            }
            auto target_argv = make_argv(target_args);
            const auto args = parse_target(
                static_cast<int>(target_argv.size()), target_argv.data(), 1);

            RbcpClient client(args.ip, args.port, args.timeout);
            const auto data = client.read(address, length);
            field("command", "rbcp-read");
            field("address", hex_address(address));
            field("data", hex_bytes(data));
            return 0;
        }

        if (command == "rbcp-write") {
            if (argc < 5) {
                throw Error("usage: rbcp-write IP ADDRESS HEX-BYTES");
            }

            const std::string ip = argv[2];
            const uint32_t address = parse_u32(argv[3]);
            const auto bytes = parse_hex(argv[4]);

            std::vector<std::string> target_args = {argv[0], ip};
            for (int i = 5; i < argc; ++i) {
                target_args.emplace_back(argv[i]);
            }
            auto target_argv = make_argv(target_args);
            const auto args = parse_target(
                static_cast<int>(target_argv.size()), target_argv.data(), 1);

            RbcpClient client(args.ip, args.port, args.timeout);
            (void)client.write(address, bytes);
            field("command", "rbcp-write");
            field("address", hex_address(address));
            field("data", hex_bytes(bytes));
            field("status", "WRITE OK");
            return 0;
        }

        if (command == "verify" || command == "mpcx-plan") {
            if (argc < 4) {
                throw Error("missing IP or FILE");
            }

            const std::string ip = argv[2];
            const std::string file = argv[3];
            const auto payload = read_file(file);
            const int type = classify(payload);
            if (type == 0) {
                throw Error("invalid/unknown 22-byte MPC payload");
            }

            std::vector<std::string> target_args = {argv[0], ip};
            for (int i = 4; i < argc; ++i) {
                target_args.emplace_back(argv[i]);
            }
            auto target_argv = make_argv(target_args);
            const auto args = parse_target(
                static_cast<int>(target_argv.size()), target_argv.data(), 1);

            RbcpClient client(args.ip, args.port, args.timeout);
            const auto eeprom = read_exact(client, EEPROM_BASE, 0x50);

            if (command == "mpcx-plan") {
                if (type != 1) {
                    throw Error("payload is not classified as SiTCP-XG");
                }

                std::vector<uint8_t> expected(eeprom.begin(), eeprom.begin() + 24);
                std::copy(payload.begin(), payload.begin() + 16, expected.begin());
                std::copy(payload.begin() + 16, payload.end(), expected.begin() + 18);
                field("command", "mpcx-plan");
                field("preserved FC10..FC11", hex_bytes({eeprom[16], eeprom[17]}));
                field("EEPROM record", hex_bytes(expected));
                field("status", "NO WRITE PERFORMED");
                return 0;
            }

            bool ok = false;
            if (type == 1) {
                std::vector<uint8_t> expected(eeprom.begin(), eeprom.begin() + 24);
                std::copy(payload.begin(), payload.begin() + 16, expected.begin());
                std::copy(payload.begin() + 16, payload.end(), expected.begin() + 18);
                ok = std::equal(expected.begin(), expected.end(), eeprom.begin());
            } else {
                ok = std::equal(payload.begin(), payload.begin() + 6, eeprom.begin() + 0x12) &&
                     std::equal(payload.begin() + 6, payload.end(), eeprom.begin() + 0x40);
            }

            field("command", "verify");
            field("file type", type_name(type));
            field("match", ok ? "YES" : "NO");
            field("status", ok ? "VERIFY OK" : "VERIFY FAILED");
            return ok ? 0 : 6;
        }

        if (command == "clear") {
            if (argc < 4 || std::string(argv[3]) != "--yes-really-clear") {
                throw Error(
                    "clear is destructive; add --yes-really-clear immediately after IP");
            }

            const std::string ip = argv[2];
            std::vector<std::string> target_args = {argv[0], ip};
            for (int i = 4; i < argc; ++i) {
                target_args.emplace_back(argv[i]);
            }
            auto target_argv = make_argv(target_args);
            const auto args = parse_target(
                static_cast<int>(target_argv.size()), target_argv.data(), 1);

            RbcpClient client(args.ip, args.port, args.timeout);
            client.write(EEPROM_WRITE_ENABLE, {0x00});
            try {
                const std::vector<uint8_t> ff(16, 0xff);
                for (uint32_t offset = 0; offset < 0x80; offset += 16) {
                    client.write(EEPROM_BASE + offset, ff);
                }
            } catch (...) {
                try {
                    client.write(EEPROM_WRITE_ENABLE, {0xff});
                } catch (...) {
                }
                throw;
            }
            client.write(EEPROM_WRITE_ENABLE, {0xff});

            field("command", "clear");
            field("EEPROM area", "0xFFFFFC00..0xFFFFFC7F");
            field("status", "CLEAR OK");
            return 0;
        }

        if (command == "write") {
            std::cerr << "Use mpc-mpcx-ip-writer for the verified high-level write path.\n";
            return 8;
        }

        throw Error("unknown command: " + command);
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
