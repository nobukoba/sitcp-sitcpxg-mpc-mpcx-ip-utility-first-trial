#include "sitcp-sitcpxg-network-config.hpp"

#include <iostream>
#include <string>

namespace {

void usage(const char* program) {
    std::cerr
        << "Usage: " << program << " IP [options]\n\n"
        << "Options:\n"
        << "  --port N       RBCP UDP port (default: "\n        << sitcp_sitcpxg::network_config::DEFAULT_PORT << ")\n"
        << "  --timeout SEC  RBCP timeout in seconds (default: "\n        << sitcp_sitcpxg::network_config::DEFAULT_TIMEOUT << ")\n"
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

        const std::string host = argv[1];
        uint16_t port = sitcp_sitcpxg::network_config::DEFAULT_PORT;
        double timeout = sitcp_sitcpxg::network_config::DEFAULT_TIMEOUT;

        for (int i = 2; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--port" && i + 1 < argc) {
                const auto value = std::stoul(argv[++i]);
                if (value == 0 || value > 65535) {
                    throw sitcp_sitcpxg::network_config::Error("invalid port");
                }
                port = static_cast<uint16_t>(value);
            } else if (arg == "--timeout" && i + 1 < argc) {
                timeout = std::stod(argv[++i]);
                if (timeout <= 0) {
                    throw sitcp_sitcpxg::network_config::Error("timeout must be positive");
                }
            } else if (arg == "-h" || arg == "--help") {
                usage(argv[0]);
                return 0;
            } else {
                throw sitcp_sitcpxg::network_config::Error(\n                    "unknown option: " + arg);
            }
        }

        std::cout << "target       : " << host << ':' << port << '\n';
        sitcp_sitcpxg::network_config::show_all(host, port, timeout);
        std::cout << "status       : READ OK\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
