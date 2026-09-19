#include "sitcp-sitcpxg-network-config.hpp"
#include "sitcp-sitcpxg-rbcp.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

namespace rbcp = sitcp_sitcpxg::rbcp;

constexpr uint32_t EEPROM_BASE = 0xFFFFFC00u;
constexpr uint32_t XG_IDENTIFIER = 0xFFFFFF08u;
constexpr int FIELD_WIDTH = 20;

std::vector<uint8_t> read_exact(rbcp::Client& client, uint32_t address,
                                size_t length) {
    std::vector<uint8_t> data;
    for (size_t offset = 0; offset < length; offset += 8) {
        const size_t chunk_size = std::min<size_t>(8, length - offset);
        const std::vector<uint8_t> block = rbcp::read_retry(
            client, address + static_cast<uint32_t>(offset), chunk_size);
        data.insert(data.end(), block.begin(), block.end());
    }
    return data;
}

std::vector<uint8_t> reconstruct_mpcx_payload(
    const std::vector<uint8_t>& eeprom) {
    std::vector<uint8_t> payload(eeprom.begin(), eeprom.begin() + 16);
    payload.insert(payload.end(), eeprom.begin() + 18, eeprom.begin() + 24);
    return payload;
}

std::vector<uint8_t> reconstruct_mpc_payload(
    const std::vector<uint8_t>& eeprom) {
    std::vector<uint8_t> payload(
        eeprom.begin() + 0x12, eeprom.begin() + 0x18);
    payload.insert(
        payload.end(), eeprom.begin() + 0x40, eeprom.begin() + 0x50);
    return payload;
}

std::string hex_bytes(const std::vector<uint8_t>& data,
                      size_t begin = 0, size_t end = SIZE_MAX,
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

int detect_target(rbcp::Client& client, std::string& reason) {
    try {
        const std::vector<uint8_t> identifier =
            rbcp::read_retry(client, XG_IDENTIFIER, 4);
        const std::vector<uint8_t> expected = {
            0x58, 0x54, 0x43, 0x50
        };
        if (identifier == expected) {
            reason = "SiTCP-XG identifier: 0x58544350";
            return 1;
        }

        reason = "SiTCP-XG identifier mismatch";
        return 2;
    } catch (const rbcp::BusError&) {
        reason = "SiTCP-XG identifier register: not supported";
        return 2;
    } catch (const rbcp::Timeout&) {
        reason = "SiTCP-XG identifier register: timeout";
        return -1;
    }
}

int run_mpc_mpcx_reader(int argc, char** argv) {
    try {
        if (argc < 2 ||
            (argc == 2 &&
             (std::string(argv[1]) == "-h" ||
              std::string(argv[1]) == "--help"))) {
            return argc < 2 ? 2 : 0;
        }

        const std::string ip = argv[1];
        uint16_t port = rbcp::DEFAULT_PORT;
        double timeout = rbcp::DEFAULT_TIMEOUT;

        for (int i = 2; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--port" && i + 1 < argc) {
                const unsigned long value = std::stoul(argv[++i]);
                if (value == 0 || value > 65535) {
                    throw rbcp::Error("invalid port");
                }
                port = static_cast<uint16_t>(value);
            } else if (option == "--timeout" && i + 1 < argc) {
                timeout = std::stod(argv[++i]);
                if (timeout <= 0) {
                    throw rbcp::Error("timeout must be positive");
                }
            } else if (option == "-h" || option == "--help") {
                return 0;
            } else {
                throw rbcp::Error("unknown option: " + option);
            }
        }

        rbcp::Client client(ip, port, timeout);
        const std::vector<uint8_t> eeprom =
            read_exact(client, EEPROM_BASE, 0x50);

        std::string detection_reason;
        const int target_type = detect_target(client, detection_reason);

        std::vector<uint8_t> payload;
        if (target_type == 1) {
            payload = reconstruct_mpcx_payload(eeprom);
        } else if (target_type == 2) {
            payload = reconstruct_mpc_payload(eeprom);
        }

        field("command", "read");
        field("target", ip + ":" + std::to_string(port));
        field("detected type", type_name(target_type));
        field("detection", detection_reason);
        if (!payload.empty()) {
            field("reconstructed payload", hex_bytes(payload));
        }

        // FC12..FC17 is the MAC location for both supported generations.
        field("MAC", hex_bytes(eeprom, 0x12, 0x18, ':'));

        if (target_type == 1) {
            field("MPCX FC00..FC0F", hex_bytes(eeprom, 0, 16));
        } else if (target_type == 2) {
            field("MPC FC40..FC4F", hex_bytes(eeprom, 0x40, 0x50));
        }

        field("EEPROM IP",
              std::to_string(eeprom[0x18]) + "." +
              std::to_string(eeprom[0x19]) + "." +
              std::to_string(eeprom[0x1A]) + "." +
              std::to_string(eeprom[0x1B]));
        field("status", "READ OK");

        std::cout << "raw EEPROM FC00..FC4F:\n";
        for (size_t offset = 0; offset < eeprom.size(); offset += 16) {
            std::cout
                << std::hex << std::uppercase
                << std::setw(8) << std::setfill('0')
                << (EEPROM_BASE + offset) << ": "
                << hex_bytes(
                       eeprom, offset,
                       std::min(offset + 16, eeprom.size()))
                << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}

void unified_usage(const char* program) {
    std::cerr
        << "Usage: " << program << " IP [options]\n\n"
        << "Reads MPC/MPCX EEPROM information and always displays both\n"
        << "current and EEPROM MAC/IP configuration.\n\n"
        << "Options:\n"
        << "  --port N       RBCP UDP port (default: "
        << sitcp_sitcpxg::network_config::DEFAULT_PORT << ")\n"
        << "  --timeout SEC  RBCP timeout in seconds (default: "
        << sitcp_sitcpxg::network_config::DEFAULT_TIMEOUT << ")\n"
        << "  -h, --help     Show this help\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2 ||
            (argc == 2 &&
             (std::string(argv[1]) == "-h" ||
              std::string(argv[1]) == "--help"))) {
            unified_usage(argv[0]);
            return argc < 2 ? 2 : 0;
        }

        const std::string host = argv[1];
        uint16_t port = sitcp_sitcpxg::network_config::DEFAULT_PORT;
        double timeout = sitcp_sitcpxg::network_config::DEFAULT_TIMEOUT;

        for (int i = 2; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--port" && i + 1 < argc) {
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

        std::cout << "network configuration:\n";
        sitcp_sitcpxg::network_config::show_all(
            host, port, timeout, "  ");

        std::cout << "\nMPC/MPCX information:\n";
        return run_mpc_mpcx_reader(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
