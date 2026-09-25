#include "sitcp-sitcpxg-network-config.hpp"

#include <cstdint>
#include <iostream>
#include <string>

namespace {

namespace network_config = sitcp_sitcpxg::network_config;

void usage(const char* program) {
    std::cerr
        << "Usage: " << program << " CURRENT_IP NEW_IP [options]" << std::endl
        << std::endl
        << "Options:" << std::endl
        << "  --eeprom      Write EEPROM IP (default)" << std::endl
        << "  --current     Write current/runtime IP, reconnect to NEW_IP, and verify"
        << std::endl
        << "  --port N      RBCP UDP port (default: "
        << network_config::DEFAULT_PORT << ")" << std::endl
        << "  --timeout SEC RBCP timeout in seconds (default: "
        << network_config::DEFAULT_TIMEOUT << ")" << std::endl
        << "  -h, --help    Show this help" << std::endl;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 3 ||
            (argc == 2 &&
             (std::string(argv[1]) == "-h" ||
              std::string(argv[1]) == "--help"))) {
            usage(argv[0]);
            return argc < 3 ? 2 : 0;
        }

        const std::string host = argv[1];
        const std::string new_ip = argv[2];
        uint16_t port = network_config::DEFAULT_PORT;
        double timeout = network_config::DEFAULT_TIMEOUT;
        bool write_current = false;
        bool mode_explicit = false;

        for (int i = 3; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--eeprom") {
                if (mode_explicit && write_current) {
                    throw network_config::Error(
                        "--eeprom and --current are mutually exclusive");
                }
                write_current = false;
                mode_explicit = true;
            } else if (option == "--current") {
                if (mode_explicit && !write_current) {
                    throw network_config::Error(
                        "--eeprom and --current are mutually exclusive");
                }
                write_current = true;
                mode_explicit = true;
            } else if (option == "--port" && i + 1 < argc) {
                const unsigned long value = std::stoul(argv[++i]);
                if (value == 0 || value > 65535) {
                    throw network_config::Error("invalid port");
                }
                port = static_cast<uint16_t>(value);
            } else if (option == "--timeout" && i + 1 < argc) {
                timeout = std::stod(argv[++i]);
                if (timeout <= 0) {
                    throw network_config::Error(
                        "timeout must be positive");
                }
            } else if (option == "-h" || option == "--help") {
                usage(argv[0]);
                return 0;
            } else {
                throw network_config::Error(
                    "unknown option: " + option);
            }
        }

        network_config::show_compact(host, port, timeout, "before");
        if (write_current) {
            network_config::write_current_ip(host, new_ip, port, timeout);
        } else {
            network_config::write_eeprom_ip(host, new_ip, port, timeout);
        }
        network_config::show_compact(
            write_current ? new_ip : host, port, timeout, "after");
        network_config::print_success();

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << std::endl;
        return 1;
    }
}
