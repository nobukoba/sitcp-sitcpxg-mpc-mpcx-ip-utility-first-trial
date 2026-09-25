#pragma once

#include "sitcp-sitcpxg-rbcp.hpp"

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace sitcp_sitcpxg {
namespace eeprom_init {

// Recovered from MPC Writer XG 0.4.1-2-gc782, 0x40a0a0.
// This is an initialization flag, not a payload-validity or rate check.
inline bool mpcx_needs_initialization(const std::vector<uint8_t>& eeprom) {
    if (eeprom.size() < 0x50) {
        throw rbcp::Error("MPCX initialization requires an 80-byte EEPROM image");
    }
    return (eeprom[0x10] & 0x80u) != 0;
}

inline std::vector<uint8_t> read_mpcx_runtime(rbcp::Client& client) {
    std::vector<uint8_t> runtime;
    for (uint32_t offset = 0; offset < 0x50; offset += 8) {
        const uint32_t address = 0xFFFFFF00u + offset;
        try {
            const std::vector<uint8_t> block = rbcp::read_retry(client, address, 8);
            runtime.insert(runtime.end(), block.begin(), block.end());
        } catch (const rbcp::Error& error) {
            std::ostringstream message;
            message << "MPCX EEPROM initialization requires complete runtime "
                    << "FF00..FF4F; read failed at 0x" << std::hex
                    << std::uppercase << address << ": " << error.what()
                    << ". No EEPROM writes performed; "
                    << "no verified MPCX default image is available.";
            throw rbcp::Error(message.str());
        }
    }
    const uint8_t xg_identifier[] = {0x58, 0x54, 0x43, 0x50};
    if (!std::equal(xg_identifier, xg_identifier + 4, runtime.begin() + 8)) {
        throw rbcp::Error("MPCX initialization: runtime XG identifier mismatch; "
                          "no EEPROM writes performed");
    }
    if ((runtime[0x10] & 0x80u) != 0) {
        throw rbcp::Error("MPCX initialization: runtime control RESET bit is set; "
                          "no EEPROM writes performed");
    }
    return runtime;
}

// Called only after file validation and independent target generation detection.
inline std::vector<uint8_t> prepare_mpcx_image(
    rbcp::Client& client, const std::vector<uint8_t>& eeprom,
    const std::vector<uint8_t>& payload, bool& initialized) {
    if (payload.size() != 22) {
        throw rbcp::Error("MPCX image requires a 22-byte payload");
    }
    initialized = mpcx_needs_initialization(eeprom);
    std::vector<uint8_t> image;
    if (initialized) {
        // The official RAM image supplies FC10..FC11 and FC18..FC4F,
        // including transmission rate at FC40..FC41. Payload bytes win below.
        image = read_mpcx_runtime(client);
    } else {
        // Existing configuration must not be replaced by current RAM values.
        image.assign(eeprom.begin(), eeprom.begin() + 24);
    }
    std::copy(payload.begin(), payload.begin() + 16, image.begin());
    std::copy(payload.begin() + 16, payload.end(), image.begin() + 18);
    return image;
}

}  // namespace eeprom_init
}  // namespace sitcp_sitcpxg
