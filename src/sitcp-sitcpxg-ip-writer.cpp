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

namespace {
constexpr uint16_t DEFAULT_PORT = 4660;
constexpr double DEFAULT_TIMEOUT = 3.0;
constexpr uint32_t CURRENT_MAC = 0xFFFFFF12u;
constexpr uint32_t CURRENT_IP = 0xFFFFFF18u;
constexpr uint32_t EEPROM_MAC = 0xFFFFFC12u;
constexpr uint32_t EEPROM_IP = 0xFFFFFC18u;
constexpr uint32_t EEPROM_WE = 0xFFFFFCFFu;

struct Error : std::runtime_error { using std::runtime_error::runtime_error; };
struct Timeout : Error { using Error::Error; };

class Client {
    std::string host_;
    uint16_t port_;
    double timeout_;
    uint8_t id_ = 0;

    std::vector<uint8_t> transaction(uint8_t cmd, uint32_t addr,
                                     const std::vector<uint8_t>& data, uint8_t len) {
        const uint8_t id = id_++;
        std::vector<uint8_t> q = {
            0xff, cmd, id, len,
            static_cast<uint8_t>(addr >> 24),
            static_cast<uint8_t>(addr >> 16),
            static_cast<uint8_t>(addr >> 8),
            static_cast<uint8_t>(addr)
        };
        q.insert(q.end(), data.begin(), data.end());

        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        addrinfo* result = nullptr;
        const std::string port_string = std::to_string(port_);
        int rc = getaddrinfo(host_.c_str(), port_string.c_str(), &hints, &result);
        if (rc != 0) throw Error(gai_strerror(rc));

        const int fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (fd < 0) {
            freeaddrinfo(result);
            throw Error(strerror(errno));
        }

        if (sendto(fd, q.data(), q.size(), 0, result->ai_addr, result->ai_addrlen) !=
            static_cast<ssize_t>(q.size())) {
            freeaddrinfo(result);
            close(fd);
            throw Error(strerror(errno));
        }
        freeaddrinfo(result);

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        timeval tv{
            static_cast<long>(timeout_),
            static_cast<long>((timeout_ - static_cast<long>(timeout_)) * 1000000.0)
        };
        rc = select(fd + 1, &fds, nullptr, nullptr, &tv);
        if (rc == 0) {
            close(fd);
            throw Timeout("RBCP timeout");
        }
        if (rc < 0) {
            close(fd);
            throw Error(strerror(errno));
        }

        uint8_t reply[263];
        const ssize_t n = recvfrom(fd, reply, sizeof(reply), 0, nullptr, nullptr);
        close(fd);
        if (n < 8 || reply[0] != 0xff || reply[2] != id) throw Error("invalid RBCP reply");
        if (reply[1] & 1u) throw Error("RBCP bus error");
        return {reply + 8, reply + n};
    }

public:
    Client(std::string host, uint16_t port, double timeout)
        : host_(std::move(host)), port_(port), timeout_(timeout) {}

    std::vector<uint8_t> read(uint32_t addr, uint8_t len) {
        return transaction(0xc0, addr, {}, len);
    }

    void write(uint32_t addr, const std::vector<uint8_t>& data) {
        if (data.empty() || data.size() > 255) throw Error("invalid write length");
        (void)transaction(0x80, addr, data, static_cast<uint8_t>(data.size()));
    }
};

std::vector<uint8_t> read_retry(Client& c, uint32_t addr, uint8_t len) {
    for (int attempt = 0; attempt < 3; ++attempt) {
        try {
            return c.read(addr, len);
        } catch (const Timeout&) {
            if (attempt == 2) throw;
        }
    }
    throw Timeout("RBCP timeout");
}

std::vector<uint8_t> parse_ip(const std::string& s) {
    in_addr addr{};
    if (inet_pton(AF_INET, s.c_str(), &addr) != 1) throw Error("invalid IPv4 address: " + s);
    const auto* p = reinterpret_cast<const uint8_t*>(&addr.s_addr);
    return {p[0], p[1], p[2], p[3]};
}

std::string ip_string(const std::vector<uint8_t>& d) {
    if (d.size() != 4) throw Error("short IP read");
    return std::to_string(d[0]) + "." + std::to_string(d[1]) + "." +
           std::to_string(d[2]) + "." + std::to_string(d[3]);
}

std::string mac_string(const std::vector<uint8_t>& d) {
    if (d.size() != 6) throw Error("short MAC read");
    std::ostringstream out;
    out << std::hex << std::uppercase << std::setfill('0');
    for (size_t i = 0; i < d.size(); ++i) {
        if (i) out << ':';
        out << std::setw(2) << static_cast<unsigned>(d[i]);
    }
    return out.str();
}

void show(Client& c) {
    std::cout
        << "current MAC  : " << mac_string(read_retry(c, CURRENT_MAC, 6)) << '\n'
        << "current IP   : " << ip_string(read_retry(c, CURRENT_IP, 4)) << '\n'
        << "EEPROM MAC   : " << mac_string(read_retry(c, EEPROM_MAC, 6)) << '\n'
        << "EEPROM IP    : " << ip_string(read_retry(c, EEPROM_IP, 4)) << '\n';
}

void usage(const char* p) {
    std::cerr
        << "Usage: " << p << " CURRENT_IP NEW_IP [options]\n\n"
        << "Options:\n"
        << "  --eeprom      Write EEPROM IP (default)\n"
        << "  --current     Write current/runtime IP, then reconnect to NEW_IP and verify\n"
        << "  --port N      RBCP UDP port (default: " << DEFAULT_PORT << ")\n"
        << "  --timeout SEC RBCP timeout in seconds (default: " << DEFAULT_TIMEOUT << ")\n"
        << "  -h, --help    Show this help\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 3 ||
            (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help"))) {
            usage(argv[0]);
            return argc < 3 ? 2 : 0;
        }

        const std::string host = argv[1];
        const std::string new_host = argv[2];
        const auto new_ip = parse_ip(new_host);
        uint16_t port = DEFAULT_PORT;
        double timeout = DEFAULT_TIMEOUT;
        bool current = false;
        bool mode_explicit = false;

        for (int i = 3; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--eeprom") {
                if (mode_explicit && current) throw Error("--eeprom and --current are mutually exclusive");
                current = false;
                mode_explicit = true;
            } else if (arg == "--current") {
                if (mode_explicit && !current) throw Error("--eeprom and --current are mutually exclusive");
                current = true;
                mode_explicit = true;
            } else if (arg == "--port" && i + 1 < argc) {
                const auto p = std::stoul(argv[++i]);
                if (p == 0 || p > 65535) throw Error("invalid port");
                port = static_cast<uint16_t>(p);
            } else if (arg == "--timeout" && i + 1 < argc) {
                timeout = std::stod(argv[++i]);
                if (timeout <= 0) throw Error("timeout must be positive");
            } else if (arg == "-h" || arg == "--help") {
                usage(argv[0]);
                return 0;
            } else {
                throw Error("unknown option: " + arg);
            }
        }

        Client old_client(host, port, timeout);
        std::cout << "before:\n";
        show(old_client);

        if (!current) {
            old_client.write(EEPROM_WE, {0x00});
            try {
                old_client.write(EEPROM_IP, new_ip);
            } catch (...) {
                try { old_client.write(EEPROM_WE, {0xff}); } catch (...) {}
                throw;
            }
            old_client.write(EEPROM_WE, {0xff});

            const auto rb = read_retry(old_client, EEPROM_IP, 4);
            if (rb != new_ip) throw Error("EEPROM IP read-back mismatch");

            std::cout << "after:\n";
            show(old_client);
            std::cout
                << "written target : EEPROM IP only\n"
                << "status         : WRITE/VERIFY OK\n";
            return 0;
        }

        bool write_ack_received = true;
        try {
            old_client.write(CURRENT_IP, new_ip);
        } catch (const Timeout&) {
            // Changing the active IP can make the reply unreachable at the old address.
            // Do not retry the destructive write; verify by reconnecting to NEW_IP instead.
            write_ack_received = false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        Client new_client(new_host, port, timeout);
        const auto current_rb = read_retry(new_client, CURRENT_IP, 4);
        if (current_rb != new_ip) {
            throw Error("current IP read-back mismatch after reconnect to " + new_host);
        }

        std::cout << "after (reconnected to " << new_host << "):\n";
        show(new_client);
        std::cout
            << "written target : current/runtime IP only\n"
            << "write ACK      : " << (write_ack_received ? "received" : "not received; verified at new IP") << '\n'
            << "status         : WRITE/RECONNECT/VERIFY OK\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
}
