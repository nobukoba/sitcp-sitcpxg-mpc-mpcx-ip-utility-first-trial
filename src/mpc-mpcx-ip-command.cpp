#include "ip-config.hpp"

#define main mpc_mpcx_command_legacy_main
#include "mpc-mpcx-command.cpp"
#undef main

namespace {
void unified_usage(const char* p) {
    std::cerr << "Usage: " << p << " COMMAND ...\n\n"
              << "Commands:\n"
              << "  inspect FILE\n"
              << "  read IP [--port N] [--timeout SEC]\n"
              << "  verify IP FILE [--port N] [--timeout SEC]\n"
              << "  mpcx-plan IP FILE [--port N] [--timeout SEC]\n"
              << "  probe IP ADDRESS [LENGTH] [--port N] [--timeout SEC]\n"
              << "  rbcp-read IP ADDRESS LENGTH [--port N] [--timeout SEC]\n"
              << "  rbcp-write IP ADDRESS HEX-BYTES [--port N] [--timeout SEC]\n"
              << "  clear IP --yes-really-clear [--port N] [--timeout SEC]\n"
              << "  ip-read IP [--port N] [--timeout SEC]\n"
              << "  ip-write CURRENT_IP NEW_IP [--eeprom|--current] [--port N] [--timeout SEC]\n"
              << "  write ...  Use mpc-mpcx-ip-writer\n\n"
              << "Defaults:\n"
              << "  --port N       RBCP UDP port (default: " << ipconfig::DEFAULT_PORT << ")\n"
              << "  --timeout SEC  RBCP timeout in seconds (default: " << ipconfig::DEFAULT_TIMEOUT << ")\n"
              << "  probe LENGTH   bytes to read (default: 1)\n"
              << "  ip-write       writes EEPROM IP unless --current is specified\n";
}

void parse_common(int argc, char** argv, int start, uint16_t& port, double& timeout) {
    for (int i = start; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--port" && i + 1 < argc) {
            const auto p = std::stoul(argv[++i]);
            if (p == 0 || p > 65535) throw std::runtime_error("invalid port");
            port = static_cast<uint16_t>(p);
        } else if (a == "--timeout" && i + 1 < argc) {
            timeout = std::stod(argv[++i]);
            if (timeout <= 0) throw std::runtime_error("timeout must be positive");
        } else {
            throw std::runtime_error("unknown option: " + a);
        }
    }
}
}

int main(int argc, char** argv) {
    try {
        if (argc < 2 || std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") {
            unified_usage(argv[0]);
            return 0;
        }

        const std::string cmd = argv[1];
        if (cmd == "ip-read") {
            if (argc < 3) throw std::runtime_error("usage: ip-read IP [options]");
            uint16_t port = ipconfig::DEFAULT_PORT;
            double timeout = ipconfig::DEFAULT_TIMEOUT;
            parse_common(argc, argv, 3, port, timeout);
            ipconfig::show_all(argv[2], port, timeout);
            return 0;
        }

        if (cmd == "ip-write") {
            if (argc < 4) throw std::runtime_error("usage: ip-write CURRENT_IP NEW_IP [--eeprom|--current] [options]");
            const std::string host = argv[2];
            const std::string new_ip = argv[3];
            (void)ipconfig::parse_ipv4(new_ip);
            uint16_t port = ipconfig::DEFAULT_PORT;
            double timeout = ipconfig::DEFAULT_TIMEOUT;
            bool current = false;
            for (int i = 4; i < argc; ++i) {
                const std::string a = argv[i];
                if (a == "--current") current = true;
                else if (a == "--eeprom") current = false;
                else if (a == "--port" && i + 1 < argc) {
                    const auto p = std::stoul(argv[++i]);
                    if (p == 0 || p > 65535) throw std::runtime_error("invalid port");
                    port = static_cast<uint16_t>(p);
                } else if (a == "--timeout" && i + 1 < argc) {
                    timeout = std::stod(argv[++i]);
                    if (timeout <= 0) throw std::runtime_error("timeout must be positive");
                } else throw std::runtime_error("unknown option: " + a);
            }
            std::cout << "before:\n";
            ipconfig::show_all(host, port, timeout, "  ");
            if (current) ipconfig::write_current_ip(host, new_ip, port, timeout);
            else ipconfig::write_eeprom_ip(host, new_ip, port, timeout);
            std::cout << "after:\n";
            ipconfig::show_all(current ? new_ip : host, port, timeout, "  ");
            std::cout << "status       : WRITE/VERIFY OK\n";
            return 0;
        }

        if (cmd == "read") {
            if (argc < 3) throw std::runtime_error("usage: read IP [options]");
            uint16_t port = ipconfig::DEFAULT_PORT;
            double timeout = ipconfig::DEFAULT_TIMEOUT;
            parse_common(argc, argv, 3, port, timeout);
            std::cout << "network configuration:\n";
            ipconfig::show_all(argv[2], port, timeout, "  ");
            std::cout << "\nMPC/MPCX information:\n";
        }

        if (cmd == "write") {
            std::cerr << "Use mpc-mpcx-ip-writer for the verified high-level write path.\n";
            return 8;
        }

        return mpc_mpcx_command_legacy_main(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
}
