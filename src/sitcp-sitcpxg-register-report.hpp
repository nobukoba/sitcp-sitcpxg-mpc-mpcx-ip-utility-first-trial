#pragma once

#include "sitcp-sitcpxg-network-config.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace sitcp_sitcpxg {
namespace register_report {

namespace rbcp = sitcp_sitcpxg::rbcp;

constexpr uint32_t EEPROM_BASE = 0xFFFFFC00u;
constexpr uint32_t XG_IDENTIFIER = 0xFFFFFF08u;
constexpr int FIELD_WIDTH = 20;

inline std::vector<uint8_t> read_exact(rbcp::Client& client, uint32_t address,
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

inline std::vector<uint8_t> reconstruct_mpcx_payload(
    const std::vector<uint8_t>& eeprom) {
    std::vector<uint8_t> payload(eeprom.begin(), eeprom.begin() + 16);
    payload.insert(payload.end(), eeprom.begin() + 18, eeprom.begin() + 24);
    return payload;
}

inline std::vector<uint8_t> reconstruct_mpc_payload(
    const std::vector<uint8_t>& eeprom) {
    std::vector<uint8_t> payload(
        eeprom.begin() + 0x12, eeprom.begin() + 0x18);
    payload.insert(
        payload.end(), eeprom.begin() + 0x40, eeprom.begin() + 0x50);
    return payload;
}

inline std::string hex_bytes(const std::vector<uint8_t>& data,
                      size_t begin = 0, size_t end = static_cast<size_t>(-1),
                      char separator = ' ') {
    end = std::min(end, data.size());

    std::ostringstream output;
    output << std::hex << std::uppercase << std::setfill('0');
    for (size_t i = begin; i < end; ++i) {
        if (i != begin) {
            output << separator;
        }
        output << std::setw(2) << static_cast<unsigned>(data[i]);
    }
    return output.str();
}

inline void field(const std::string& name, const std::string& value) {
    std::cout << std::left << std::setw(FIELD_WIDTH)
              << name << ": " << value << '\n';
}

inline uint16_t be16(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(data[offset]) << 8) |
        static_cast<uint16_t>(data[offset + 1]));
}

inline std::string format_mac(const std::vector<uint8_t>& data, size_t offset) {
    return hex_bytes(data, offset, offset + 6, ':');
}

inline std::string format_ipv4(const std::vector<uint8_t>& data, size_t offset) {
    return std::to_string(data[offset]) + "." +
           std::to_string(data[offset + 1]) + "." +
           std::to_string(data[offset + 2]) + "." +
           std::to_string(data[offset + 3]);
}

inline std::string decimal_hex(uint32_t value, int width = 4) {
    std::ostringstream output;
    output << std::dec << value << " (0x" << std::hex << std::uppercase
           << std::setfill('0') << std::setw(width) << value << ")";
    return output.str();
}

inline void show_xg_parameters(const std::vector<uint8_t>& registers,
                               const std::string& region) {
    const uint16_t disconnect_timeout = be16(registers, 0x2A);
    const double disconnect_seconds =
        (static_cast<double>(disconnect_timeout) + 1.0) * 0.256;
    const uint16_t msl = be16(registers, 0x2C);

    std::ostringstream disconnect;
    disconnect << decimal_hex(disconnect_timeout) << " ("
               << std::fixed << std::setprecision(3)
               << disconnect_seconds << " s, "
               << std::setprecision(2)
               << disconnect_seconds / 60.0 << " min)";

    std::ostringstream msl_value;
    msl_value << decimal_hex(msl) << " ("
              << std::fixed << std::setprecision(1)
              << static_cast<double>(msl) * 0.5 << " ms)";

    field("TCP port", decimal_hex(be16(registers, 0x1C)));
    field("TCP MSS", decimal_hex(be16(registers, 0x20)) + " bytes");
    field("RBCP UDP port", decimal_hex(be16(registers, 0x22)));
    field("keepalive nonempty",
          decimal_hex(be16(registers, 0x24)) + " ms");
    field("keepalive empty",
          decimal_hex(be16(registers, 0x26)) + " ms");
    field("connect timeout",
          decimal_hex(be16(registers, 0x28)) + " ms");
    field("disconnect timeout", disconnect.str());
    field("TCP MSL", msl_value.str());
    field("retransmission time",
          decimal_hex(be16(registers, 0x2E)) + " ms");
    field("server MAC", format_mac(registers, 0x32));
    field("server IP", format_ipv4(registers, 0x38));
    field("server TCP port", decimal_hex(be16(registers, 0x3C)));
    const uint16_t transmission_rate = be16(registers, 0x40);
    if (transmission_rate >= 1 && transmission_rate <= 10000) {
        field("transmission rate",
              decimal_hex(transmission_rate) + " Mbps");
    } else {
        field("transmission rate raw",
              decimal_hex(transmission_rate) +
              " (outside documented range 1..10000)");
        field(region + "40.." + region + "4F",
              hex_bytes(registers, 0x40, 0x50));
        field("rate note",
              "raw value; may contain data unrelated to XG rate");
    }
}

inline std::string type_name(int type) {
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

inline int detect_target(rbcp::Client& client, std::string& reason) {
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

inline void dump(uint32_t base, const std::vector<uint8_t>& data) {
    for (size_t offset = 0; offset < data.size(); offset += 16) {
        std::ostringstream line;
        line << std::hex << std::uppercase << std::setfill('0')
             << std::setw(8) << (base + offset) << ": "
             << hex_bytes(data, offset, offset + 16);
        std::cout << line.str() << '\n';
    }
}

inline void show(const std::string& host, uint16_t port, double timeout) {
    rbcp::Client client(host, port, timeout);
    std::string detection;
    const int target_type = detect_target(client, detection);
    const std::vector<uint8_t> runtime = read_exact(client, 0xFFFFFF00u, 0x50);
    const std::vector<uint8_t> eeprom = read_exact(client, EEPROM_BASE, 0x50);

    field("target", host + ":" + std::to_string(port));
    field("detected type", type_name(target_type));
    field("detection", detection);
    std::cout << "\nRuntime registers (0xFFFFFF00..0xFFFFFF4F, 80 bytes):\n";
    field("current MAC", format_mac(runtime, 0x12));
    field("current IP", format_ipv4(runtime, 0x18));
    if (target_type == 1) {
        show_xg_parameters(runtime, "FF");
    }
    std::cout << "raw runtime FF00..FF4F:\n";
    dump(0xFFFFFF00u, runtime);

    std::cout << "\nEEPROM (0xFFFFFC00..0xFFFFFC4F, 80 bytes):\n";
    field("EEPROM MAC", format_mac(eeprom, 0x12));
    field("EEPROM IP", format_ipv4(eeprom, 0x18));
    if (target_type == 1) {
        show_xg_parameters(eeprom, "FC");
    }
    std::cout << "raw EEPROM FC00..FC4F:\n";
    dump(EEPROM_BASE, eeprom);

    std::cout << "\nMPC/MPCX information (EEPROM):\n";
    std::vector<uint8_t> payload;
    if (target_type == 1) {
        payload = reconstruct_mpcx_payload(eeprom);
    } else if (target_type == 2) {
        payload = reconstruct_mpc_payload(eeprom);
    }
    if (!payload.empty()) {
        field("reconstructed payload", hex_bytes(payload));
    }
    field("MAC", format_mac(eeprom, 0x12));
    if (target_type == 1) {
        field("MPCX FC00..FC0F", hex_bytes(eeprom, 0, 16));
    } else if (target_type == 2) {
        field("MPC FC40..FC4F", hex_bytes(eeprom, 0x40, 0x50));
    }
}

}  // namespace register_report
}  // namespace sitcp_sitcpxg
