#include "ip-config.hpp"

#include <iostream>
#include <string>

namespace {

void usage(const char* program) {
    std::cerr
        << "Usage: " << program << " CURRENT_IP NEW_IP [options]\n\n"
        << "Options:\n"
        << "  --eeprom      Write EEPROM IP (default)\n"
        << "  --current     Write current/runtime IP, reconnect to NEW_IP, and verify\n"
        << "  --port N      RBCP UDP port (default: " << ipconfig::DEFAULT_PORT << ")\n"
        << "  --timeout SEC RBCP timeout in seconds (default: " << ipconfig::DEFAULT_TIMEOUT << ")\n"
        << "  -h, --help    Show this help\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 3 ||
            (argc == 2 &&
             (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help"))) {
            usage(argv[0]);
            return argc < 3 ? 2 : 0;
        }

        const std::string host = argv[1];
        const std::string new_ip = argv[2];
        uint16_t port = ipconfig::DEFAULT_PORT;
        double timeout = ipconfig::DEFAULT_TIMEOUT;
        bool write_current = false;
        bool mode_explicit = false;

        for (int i = 3; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--eeprom") {
                if (mode_explicit && write_current) {
                    throw ipconfig::Error("--eeprom and --current are mutually exclusive");
                }
                write_current = false;
                mode_explicit = true;
            } else if (arg == "--current") {
                if (mode_explicit && !write_current) {
                    throw ipconfig::Error("--eeprom and --current are mutually exclusive");
                }
                write_current = true;
                mode_explicit = true;
            } else if (arg == "--port" && i + 1 < argc) {
                const auto value = std::stoul(argv[++i]);
                if (value == 0 || value > 65535) {
                    throw ipconfig::Error("invalid port");
                }
                port = static_cast<uint16_t>(value);
            } else if (arg == "--timeout" && i + 1 < argc) {
                timeout = std::stod(argv[++i]);
                if (timeout <= 0) {
                    throw ipconfig::Error("timeout must be positive");
                }
            } else if (arg == "-h" || arg == "--help") {
                usage(argv[0]);
                return 0;
            } else {
                throw ipconfig::Error("unknown option: " + arg);
            }
        }

        std::cout << "before:\n";
        ipconfig::show_all(host, port, timeout);

        if (write_current) {
            ipconfig::write_current_ip(host, new_ip, port, timeout);
            std::cout << "after (reconnected to " << new_ip << "):\n";
            ipconfig::show_all(new_ip, port, timeout);
            std::cout
                << "written target : current/runtime IP only\n"
                << "status         : WRITE/RECONNECT/VERIFY OK\n";
        } else {
            ipconfig::write_eeprom_ip(host, new_ip, port, timeout);
            std::cout << "after:\n";
            ipconfig::show_all(host, port, timeout);
            std::cout
                << "written target : EEPROM IP only\n"
                << "status         : WRITE/VERIFY OK\n";
        }

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
