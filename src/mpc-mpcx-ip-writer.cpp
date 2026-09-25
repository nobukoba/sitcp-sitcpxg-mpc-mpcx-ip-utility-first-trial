#include "sitcp-sitcpxg-register-report.hpp"
#include "sitcp-sitcpxg-rbcp.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr double DEFAULT_TIMEOUT = sitcp_sitcpxg::rbcp::DEFAULT_TIMEOUT;
constexpr uint16_t DEFAULT_PORT = sitcp_sitcpxg::rbcp::DEFAULT_PORT;
constexpr uint32_t EEPROM_BASE = 0xFFFFFC00u;
constexpr uint32_t EEPROM_WRITE_ENABLE = 0xFFFFFCFFu;
constexpr uint32_t XG_IDENTIFIER = 0xFFFFFF08u;
constexpr size_t EEPROM_READ_SIZE = 0x50;
constexpr size_t MPC_FILE_SIZE = 22;
constexpr int FIELD_WIDTH = 20;

namespace rbcp = sitcp_sitcpxg::rbcp;
using RbcpClient = rbcp::Client;
using RbcpError = rbcp::Error;
using RbcpTimeout = rbcp::Timeout;
using RbcpBusError = rbcp::BusError;

std::vector<uint8_t> read_exact(
    RbcpClient& client, uint32_t address, size_t length,
    size_t chunk_size = 8) {
    std::vector<uint8_t> data;
    for (size_t offset = 0; offset < length; offset += chunk_size) {
        const size_t block_size =
            std::min(chunk_size, length - offset);
        const std::vector<uint8_t> block = rbcp::read_retry(
            client, address + static_cast<uint32_t>(offset), block_size);
        data.insert(data.end(), block.begin(), block.end());
    }
    return data;
}

void write_exact(
    RbcpClient& client, uint32_t address,
    const std::vector<uint8_t>& data, size_t chunk_size = 16) {
    for (size_t offset = 0; offset < data.size(); offset += chunk_size) {
        const size_t block_size =
            std::min(chunk_size, data.size() - offset);
        const std::vector<uint8_t> block(
            data.begin() + offset,
            data.begin() + offset + block_size);
        const std::vector<uint8_t> ack = client.write(
            address + static_cast<uint32_t>(offset), block);
        if (ack.size() != block_size) {
            throw RbcpError(
                "short RBCP ACK while writing EEPROM");
        }
    }
}

std::string hex_bytes(const std::vector<uint8_t>& data,
                      size_t begin = 0, size_t end = static_cast<size_t>(-1),
                      char separator = ' ') {
    end = std::min(end, data.size());

    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (size_t i = begin; i < end; ++i) {
        if (i != begin) {
            output << separator;
        }
        output << std::setw(2) << static_cast<unsigned>(data[i]);
    }
    return output.str();
}

void field(const std::string& name, const std::string& value) {
    std::cout << std::left << std::setw(FIELD_WIDTH)
              << name << ": " << value << '\n';
}

bool valid_tag(const std::vector<uint8_t>& tag) {
    if (tag.size() != 7) {
        return false;
    }

    for (size_t i = 0; i < tag.size(); ++i) {
        const uint8_t value = tag[i];
        if (value == 0 || value == 0x20 || value == '-' ||
            (value >= '0' && value <= '9')) {
            continue;
        }

        const uint8_t uppercase =
            static_cast<uint8_t>(value & 0xDF);
        if (uppercase >= 'A' && uppercase <= 'Z') {
            continue;
        }
        return false;
    }
    return true;
}

std::vector<uint8_t> decode_nonzero(const std::vector<uint8_t>& d, size_t begin,
                                    size_t len, uint8_t delta) {
    std::vector<uint8_t> out;
    out.reserve(len);
    for (size_t i = 0; i < len; ++i) {
        const uint8_t b = d[begin + i];
        out.push_back(b ? static_cast<uint8_t>(b - delta) : 0);
    }
    return out;
}

int classify_payload(const std::vector<uint8_t>& data) {
    if (data.size() != MPC_FILE_SIZE) {
        return 0;
    }
    if (valid_tag(decode_nonzero(data, 6, 7, 0x34))) {
        return 2;
    }
    if (valid_tag(decode_nonzero(data, 0, 7, 0x2C))) {
        return 1;
    }
    return 0;
}

std::string type_name(int t) {
    return t == 1 ? "MPCX (SiTCP-XG)" :
           t == 2 ? "MPC (normal SiTCP)" :
           t == -1 ? "ambiguous" : "unknown";
}

int detect_target(RbcpClient& c, std::string& why) {
    try {
        const auto identifier = rbcp::read_retry(c, XG_IDENTIFIER, 4);
        const std::vector<uint8_t> expected = {0x58, 0x54, 0x43, 0x50};
        if (identifier == expected) {
            why = "SiTCP-XG identifier: 0x58544350";
            return 1;
        }
        why = "SiTCP-XG identifier mismatch";
        return 2;
    } catch (const RbcpBusError&) {
        why = "SiTCP-XG identifier register: not supported";
        return 2;
    } catch (const RbcpTimeout&) {
        why = "SiTCP-XG identifier register: timeout";
        return -1;
    }
}

std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open file: " + path);
    }

    return std::vector<uint8_t>(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

void set_write_enable(RbcpClient& c, bool enabled) {
    const auto ack = c.write(EEPROM_WRITE_ENABLE,
                             {static_cast<uint8_t>(enabled ? 0x00 : 0xFF)});
    if (ack.size() != 1) {
        throw RbcpError("unexpected RBCP ACK length for EEPROM write-enable");
    }
}

std::vector<uint8_t> program(
    RbcpClient& c, const std::vector<uint8_t>& payload, int type) {
    std::vector<uint8_t> image;
    if (type == 1) {
        image = read_exact(c, EEPROM_BASE, 24);
        std::copy(payload.begin(), payload.begin() + 16, image.begin());
        std::copy(payload.begin() + 16, payload.end(), image.begin() + 18);
    } else {
        image = read_exact(c, EEPROM_BASE, 0x50);
        std::copy(payload.begin(), payload.begin() + 6, image.begin() + 0x12);
        std::copy(payload.begin() + 6, payload.end(), image.begin() + 0x40);
    }

    set_write_enable(c, true);
    try {
        write_exact(c, EEPROM_BASE, image, 16);
    } catch (...) {
        try {
            set_write_enable(c, false);
        } catch (...) {
        }
        throw;
    }
    set_write_enable(c, false);

    const auto actual = read_exact(c, EEPROM_BASE, image.size());
    if (actual != image) {
        size_t i = 0;
        while (i < image.size() && actual[i] == image[i]) {
            ++i;
        }
        std::ostringstream os;
        os << "EEPROM read-back verification failed";
        if (i < image.size()) {
            os << " at 0x" << std::hex << (EEPROM_BASE + i)
               << ": expected 0x" << static_cast<unsigned>(image[i])
               << ", got 0x" << static_cast<unsigned>(actual[i]);
        }
        throw RbcpError(os.str());
    }
    return actual;
}

void usage(const char* argv0) {
    std::cerr << "Usage: " << argv0 << " <ip> <file> [options]\n\n"
              << "Options:\n"
              << "  --port N       RBCP UDP port (default: " << DEFAULT_PORT << ")\n"
              << "  --timeout SEC  RBCP timeout in seconds (default: " << DEFAULT_TIMEOUT << ")\n"
              << "  -h, --help     Show this help\n";
}
}

inline int run_mpc_mpcx_writer(int argc, char** argv) {
    try {
        if (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
            usage(argv[0]);
            return 0;
        }
        if (argc < 3) {
            usage(argv[0]);
            return 2;
        }

        const std::string ip = argv[1];
        const std::string file_path = argv[2];
        uint16_t port = DEFAULT_PORT;
        double timeout = DEFAULT_TIMEOUT;

        for (int i = 3; i < argc; ++i) {
            const std::string a = argv[i];
            if (a == "--port" && i + 1 < argc) {
                const unsigned long p = std::stoul(argv[++i]);
                if (p == 0 || p > 65535) {
                    throw std::runtime_error("invalid port");
                }
                port = static_cast<uint16_t>(p);
            } else if (a == "--timeout" && i + 1 < argc) {
                timeout = std::stod(argv[++i]);
                if (timeout <= 0) {
                    throw std::runtime_error("timeout must be positive");
                }
            } else if (a == "-h" || a == "--help") {
                usage(argv[0]);
                return 0;
            } else {
                throw std::runtime_error("unknown option: " + a);
            }
        }

        const auto payload = read_file(file_path);
        const int file_type = classify_payload(payload);
        if (payload.size() != MPC_FILE_SIZE || !(file_type == 1 || file_type == 2)) {
            std::cerr << "ERROR: invalid/unknown 22-byte MPC payload (writer type "
                      << file_type << ")\n";
            return 2;
        }

        RbcpClient client(ip, port, timeout);
        const auto eeprom = read_exact(client, EEPROM_BASE, EEPROM_READ_SIZE);
        std::string detection;
        const int target_type = detect_target(client, detection);

        field("command", "write");
        field("target", ip + ":" + std::to_string(port));
        field("file", file_path);
        field("file type", type_name(file_type));
        field("target type", type_name(target_type));
        field("detection", detection);
        field("writer type", std::to_string(file_type));
        field("file payload", hex_bytes(payload));

        if ((target_type == 1 || target_type == 2) && target_type != file_type) {
            field("status", "REFUSED: TARGET TYPE MISMATCH");
            return 7;
        }
        if (!(target_type == 1 || target_type == 2)) {
            field("status", "REFUSED: TARGET TYPE NOT DETECTED");
            return 7;
        }

        field("operation", "programming EEPROM");
        const auto rb = program(client, payload, file_type);
        if (file_type == 1) {
            field("preserved FC10..FC11", hex_bytes(rb, 16, 18));
            field("read-back FC00..FC17", hex_bytes(rb));
            field("read-back MAC", hex_bytes(rb, 18, 24, ':'));
        } else {
            field("read-back MAC", hex_bytes(rb, 0x12, 0x18, ':'));
            field("read-back FC40..FC4F", hex_bytes(rb, 0x40, 0x50));
        }
        field("write", "OK");
        field("read-back verify", "OK");
        field("EEPROM write protect", "ENABLED");
        field("status", "WRITE OK");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}


namespace {

void unified_usage(const char* program) {
    std::cerr
        << "Usage: " << program << " CURRENT_IP MPC_OR_MPCX_FILE [options]\n\n"
        << "Programs an MPC/MPCX file and optionally changes the SiTCP IP address.\n\n"
        << "Options:\n"
        << "  --set-eeprom-ip IP   Set EEPROM/default IP address\n"
        << "  --set-current-ip IP  Set current/runtime IP address\n"
        << "  --port N             RBCP UDP port (default: "
        << sitcp_sitcpxg::network_config::DEFAULT_PORT << ")\n"
        << "  --timeout SEC        RBCP timeout in seconds (default: "
        << sitcp_sitcpxg::network_config::DEFAULT_TIMEOUT << ")\n"
        << "  -h, --help           Show this help\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 &&
            (std::string(argv[1]) == "-h" ||
             std::string(argv[1]) == "--help")) {
            unified_usage(argv[0]);
            return 0;
        }

        if (argc < 3) {
            unified_usage(argv[0]);
            return 2;
        }

        const std::string host = argv[1];
        const std::string file = argv[2];

        bool has_eeprom_ip = false;
        bool has_current_ip = false;
        std::string eeprom_ip;
        std::string current_ip;
        uint16_t port = sitcp_sitcpxg::network_config::DEFAULT_PORT;
        double timeout = sitcp_sitcpxg::network_config::DEFAULT_TIMEOUT;

        for (int i = 3; i < argc; ++i) {
            const std::string option = argv[i];

            if (option == "--set-eeprom-ip" && i + 1 < argc) {
                eeprom_ip = argv[++i];
                has_eeprom_ip = true;
                (void)sitcp_sitcpxg::network_config::parse_ipv4(eeprom_ip);
            } else if (option == "--set-current-ip" && i + 1 < argc) {
                current_ip = argv[++i];
                has_current_ip = true;
                (void)sitcp_sitcpxg::network_config::parse_ipv4(current_ip);
            } else if (option == "--port" && i + 1 < argc) {
                const unsigned long value = std::stoul(argv[++i]);
                if (value == 0 || value > 65535) {
                    throw std::runtime_error("invalid port");
                }
                port = static_cast<uint16_t>(value);
            } else if (option == "--timeout" && i + 1 < argc) {
                timeout = std::stod(argv[++i]);
                if (timeout <= 0) {
                    throw std::runtime_error("timeout must be positive");
                }
            } else if (option == "-h" || option == "--help") {
                unified_usage(argv[0]);
                return 0;
            } else {
                throw std::runtime_error("unknown option: " + option);
            }
        }

        std::cout << "before:\n";
        sitcp_sitcpxg::register_report::show(host, port, timeout);

        std::vector<std::string> writer_args;
        writer_args.push_back(argv[0]);
        writer_args.push_back(host);
        writer_args.push_back(file);
        writer_args.push_back("--port");
        writer_args.push_back(std::to_string(port));
        writer_args.push_back("--timeout");
        writer_args.push_back(std::to_string(timeout));

        std::vector<char*> writer_argv;
        for (std::vector<std::string>::iterator it = writer_args.begin();
             it != writer_args.end(); ++it) {
            writer_argv.push_back(&(*it)[0]);
        }

        const int writer_result = run_mpc_mpcx_writer(
            static_cast<int>(writer_argv.size()), &writer_argv[0]);
        if (writer_result != 0) {
            return writer_result;
        }

        if (has_eeprom_ip) {
            std::cout << "EEPROM IP operation : " << eeprom_ip << '\n';
            sitcp_sitcpxg::network_config::write_eeprom_ip(host, eeprom_ip, port, timeout);
        }

        std::string final_host = host;
        if (has_current_ip) {
            std::cout << "current IP operation: " << current_ip << '\n';
            sitcp_sitcpxg::network_config::write_current_ip(host, current_ip, port, timeout);
            final_host = current_ip;
        }

        std::cout << "after:\n";
        sitcp_sitcpxg::register_report::show(final_host, port, timeout);
        std::cout << "status       : WRITE/VERIFY OK\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
