#include "ip-config.hpp"

#define main mpc_mpcx_reader_legacy_main
#include "mpc-mpcx-reader.cpp"
#undef main

namespace {
void unified_usage(const char* p) {
    std::cerr << "Usage: " << p << " IP [options]\n\n"
              << "Reads MPC/MPCX EEPROM information and always displays both\n"
              << "current and EEPROM MAC/IP configuration.\n\n"
              << "Options:\n"
              << "  --port N       RBCP UDP port (default: " << ipconfig::DEFAULT_PORT << ")\n"
              << "  --timeout SEC  RBCP timeout in seconds (default: " << ipconfig::DEFAULT_TIMEOUT << ")\n"
              << "  -h, --help     Show this help\n";
}
}

int main(int argc, char** argv) {
    try {
        if (argc < 2 || (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help"))) {
            unified_usage(argv[0]);
            return 0;
        }

        const std::string host = argv[1];
        uint16_t port = ipconfig::DEFAULT_PORT;
        double timeout = ipconfig::DEFAULT_TIMEOUT;
        for (int i = 2; i < argc; ++i) {
            const std::string a = argv[i];
            if (a == "--port" && i + 1 < argc) {
                const auto p = std::stoul(argv[++i]);
                if (p == 0 || p > 65535) throw std::runtime_error("invalid port");
                port = static_cast<uint16_t>(p);
            } else if (a == "--timeout" && i + 1 < argc) {
                timeout = std::stod(argv[++i]);
                if (timeout <= 0) throw std::runtime_error("timeout must be positive");
            } else if (a == "-h" || a == "--help") {
                unified_usage(argv[0]);
                return 0;
            } else {
                throw std::runtime_error("unknown option: " + a);
            }
        }

        std::cout << "network configuration:\n";
        ipconfig::show_all(host, port, timeout, "  ");
        std::cout << "\nMPC/MPCX information:\n";
        return mpc_mpcx_reader_legacy_main(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
}
