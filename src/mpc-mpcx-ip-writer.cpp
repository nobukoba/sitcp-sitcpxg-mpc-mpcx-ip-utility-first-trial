#include "ip-config.hpp"

#define main mpc_mpcx_writer_legacy_main
#include "mpc-mpcx-writer.cpp"
#undef main

namespace {

void unified_usage(const char* program) {
    std::cerr
        << "Usage: " << program << " CURRENT_IP MPC_OR_MPCX_FILE [options]\n\n"
        << "Programs an MPC/MPCX file and optionally changes the SiTCP IP address.\n\n"
        << "Options:\n"
        << "  --set-eeprom-ip IP   Set EEPROM/default IP address\n"
        << "  --set-current-ip IP  Set current/runtime IP address\n"
        << "  --port N             RBCP UDP port (default: "
        << ipconfig::DEFAULT_PORT << ")\n"
        << "  --timeout SEC        RBCP timeout in seconds (default: "
        << ipconfig::DEFAULT_TIMEOUT << ")\n"
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
        uint16_t port = ipconfig::DEFAULT_PORT;
        double timeout = ipconfig::DEFAULT_TIMEOUT;

        for (int i = 3; i < argc; ++i) {
            const std::string option = argv[i];

            if (option == "--set-eeprom-ip" && i + 1 < argc) {
                eeprom_ip = argv[++i];
                has_eeprom_ip = true;
                (void)ipconfig::parse_ipv4(eeprom_ip);
            } else if (option == "--set-current-ip" && i + 1 < argc) {
                current_ip = argv[++i];
                has_current_ip = true;
                (void)ipconfig::parse_ipv4(current_ip);
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
        ipconfig::show_all(host, port, timeout, "  ");

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

        const int writer_result = mpc_mpcx_writer_legacy_main(
            static_cast<int>(writer_argv.size()), &writer_argv[0]);
        if (writer_result != 0) {
            return writer_result;
        }

        if (has_eeprom_ip) {
            std::cout << "EEPROM IP operation : " << eeprom_ip << '\n';
            ipconfig::write_eeprom_ip(host, eeprom_ip, port, timeout);
        }

        std::string final_host = host;
        if (has_current_ip) {
            std::cout << "current IP operation: " << current_ip << '\n';
            ipconfig::write_current_ip(host, current_ip, port, timeout);
            final_host = current_ip;
        }

        std::cout << "after:\n";
        ipconfig::show_all(final_host, port, timeout, "  ");
        std::cout << "status       : WRITE/VERIFY OK\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
