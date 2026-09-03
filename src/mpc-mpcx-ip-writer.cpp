#include "ip-config.hpp"

#define main mpc_mpcx_writer_legacy_main
#include "mpc-mpcx-writer.cpp"
#undef main

#include <optional>

namespace {
void unified_usage(const char* p) {
    std::cerr << "Usage: " << p << " CURRENT_IP [MPC_OR_MPCX_FILE] [options]\n\n"
              << "Programs an MPC/MPCX file and/or changes the SiTCP IP address.\n"
              << "At least one of MPC_OR_MPCX_FILE or --set-ip NEW_IP is required.\n\n"
              << "Options:\n"
              << "  --set-ip NEW_IP  Change IP address\n"
              << "  --eeprom          Write --set-ip to EEPROM (default)\n"
              << "  --current         Write --set-ip to current/runtime IP\n"
              << "  --port N          RBCP UDP port (default: " << ipconfig::DEFAULT_PORT << ")\n"
              << "  --timeout SEC     RBCP timeout in seconds (default: " << ipconfig::DEFAULT_TIMEOUT << ")\n"
              << "  -h, --help        Show this help\n";
}
}

int main(int argc, char** argv) {
    try {
        if (argc < 2 || (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help"))) {
            unified_usage(argv[0]);
            return 0;
        }

        const std::string host = argv[1];
        std::optional<std::string> file;
        std::optional<std::string> new_ip;
        bool write_current = false;
        uint16_t port = ipconfig::DEFAULT_PORT;
        double timeout = ipconfig::DEFAULT_TIMEOUT;

        for (int i = 2; i < argc; ++i) {
            const std::string a = argv[i];
            if (a == "--set-ip" && i + 1 < argc) {
                new_ip = argv[++i];
                (void)ipconfig::parse_ipv4(*new_ip);
            } else if (a == "--current") {
                write_current = true;
            } else if (a == "--eeprom") {
                write_current = false;
            } else if (a == "--port" && i + 1 < argc) {
                const auto p = std::stoul(argv[++i]);
                if (p == 0 || p > 65535) throw std::runtime_error("invalid port");
                port = static_cast<uint16_t>(p);
            } else if (a == "--timeout" && i + 1 < argc) {
                timeout = std::stod(argv[++i]);
                if (timeout <= 0) throw std::runtime_error("timeout must be positive");
            } else if (a == "-h" || a == "--help") {
                unified_usage(argv[0]);
                return 0;
            } else if (!a.empty() && a[0] == '-') {
                throw std::runtime_error("unknown option: " + a);
            } else if (!file) {
                file = a;
            } else {
                throw std::runtime_error("unexpected argument: " + a);
            }
        }

        if (!file && !new_ip) {
            throw std::runtime_error("specify MPC_OR_MPCX_FILE and/or --set-ip NEW_IP");
        }
        if (write_current && !new_ip) {
            throw std::runtime_error("--current requires --set-ip NEW_IP");
        }

        std::cout << "before:\n";
        ipconfig::show_all(host, port, timeout, "  ");

        if (file) {
            std::vector<std::string> args = {argv[0], host, *file,
                                             "--port", std::to_string(port),
                                             "--timeout", std::to_string(timeout)};
            std::vector<char*> av;
            for (auto& s : args) av.push_back(s.data());
            const int rc = mpc_mpcx_writer_legacy_main(static_cast<int>(av.size()), av.data());
            if (rc != 0) return rc;
        }

        std::string verify_host = host;
        if (new_ip) {
            std::cout << "IP operation : " << (write_current ? "current/runtime" : "EEPROM")
                      << " -> " << *new_ip << '\n';
            if (write_current) {
                ipconfig::write_current_ip(host, *new_ip, port, timeout);
                verify_host = *new_ip;
            } else {
                ipconfig::write_eeprom_ip(host, *new_ip, port, timeout);
            }
        }

        std::cout << "after:\n";
        ipconfig::show_all(verify_host, port, timeout, "  ");
        std::cout << "status       : WRITE/VERIFY OK\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
}
