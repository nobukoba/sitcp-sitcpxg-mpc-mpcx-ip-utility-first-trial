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
constexpr int FW = 20;

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
    std::vector<uint8_t> payload(eeprom.begin() + 0x12,
                                 eeprom.begin() + 0x18);
    payload.insert(payload.end(), eeprom.begin() + 0x40,
                   eeprom.begin() + 0x50);
    return payload;
}
std::string hex(const std::vector<uint8_t>& d, size_t a = 0,
                size_t z = SIZE_MAX, char s = ' ') {
    z = std::min(z, d.size());
    std::ostringstream o; o << std::hex << std::setfill('0');
    for (size_t i = a; i < z; ++i) { if (i > a) o << s; o << std::setw(2) << static_cast<unsigned>(d[i]); }
    return o.str();
}
void field(const std::string& k, const std::string& v) {
    std::cout << std::left << std::setw(FW) << k << ": " << v << '\n';
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
int detect(rbcp::Client& c, std::string& why) {
    try {
        const auto identifier = rbcp::read_retry(c, XG_IDENTIFIER, 4);
        const std::vector<uint8_t> expected = {0x58, 0x54, 0x43, 0x50};
        if (identifier == expected) { why = "SiTCP-XG identifier: 0x58544350"; return 1; }
        why = "SiTCP-XG identifier mismatch"; return 2;
    } catch (const rbcp::BusError&) {
        why = "SiTCP-XG identifier register: not supported"; return 2;
    } catch (const rbcp::Timeout&) {
        why = "SiTCP-XG identifier register: timeout"; return -1;
    }
}
int run_mpc_mpcx_reader(int ac, char** av) {
    try {
        if (ac < 2 || (ac == 2 && (std::string(av[1]) == "-h" || std::string(av[1]) == "--help"))) return ac < 2 ? 2 : 0;
        std::string ip = av[1];
        uint16_t port = rbcp::DEFAULT_PORT; double timeout = rbcp::DEFAULT_TIMEOUT;
        for (int i = 2; i < ac; ++i) {
            const std::string a = av[i];
            if (a == "--port" && i + 1 < ac) { auto v = std::stoul(av[++i]); if (!v || v > 65535) throw rbcp::Error("invalid port"); port = static_cast<uint16_t>(v); }
            else if (a == "--timeout" && i + 1 < ac) { timeout = std::stod(av[++i]); if (timeout <= 0) throw rbcp::Error("timeout must be positive"); }
            else if (a == "-h" || a == "--help") return 0;
            else throw rbcp::Error("unknown option: " + a);
        }
        rbcp::Client client(ip, port, timeout);
        const auto e = read_exact(client, EEPROM_BASE, 0x50);
        std::string why; const int t = detect(client, why);
        const auto payload = t == 1 ? reconstruct_mpcx_payload(e) :
                             t == 2 ? reconstruct_mpc_payload(e) :
                                      std::vector<uint8_t>{};
        field("command","read"); field("target",ip+":"+std::to_string(port)); field("detected type",type_name(t)); field("detection",why);
        if (!payload.empty()) field("reconstructed payload",hex(payload));
        field("MAC",hex(e,0x12,0x18,':'));
        if(t==1) field("MPCX FC00..FC0F",hex(e,0,16)); else if(t==2) field("MPC FC40..FC4F",hex(e,0x40,0x50));
        field("EEPROM IP",std::to_string(e[0x18])+"."+std::to_string(e[0x19])+"."+std::to_string(e[0x1a])+"."+std::to_string(e[0x1b]));
        field("status","READ OK"); std::cout<<"raw EEPROM FC00..FC4F:\n";
        for(size_t o=0;o<e.size();o+=16) std::cout<<std::hex<<std::uppercase<<std::setw(8)<<std::setfill('0')<<(EEPROM_BASE+o)<<": "<<hex(e,o,std::min(o+16,e.size()))<<'\n';
        return 0;
    } catch(const std::exception& e) { std::cerr<<"ERROR: "<<e.what()<<'\n'; return 1; }
}
}

namespace {
void unified_usage(const char* p) {
    std::cerr << "Usage: " << p << " IP [options]\n\n"
              << "Reads MPC/MPCX EEPROM information and always displays both\n"
              << "current and EEPROM MAC/IP configuration.\n\n"
              << "Options:\n"
              << "  --port N       RBCP UDP port (default: " << sitcp_sitcpxg::network_config::DEFAULT_PORT << ")\n"
              << "  --timeout SEC  RBCP timeout in seconds (default: " << sitcp_sitcpxg::network_config::DEFAULT_TIMEOUT << ")\n"
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
        uint16_t port = sitcp_sitcpxg::network_config::DEFAULT_PORT;
        double timeout = sitcp_sitcpxg::network_config::DEFAULT_TIMEOUT;
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
        sitcp_sitcpxg::network_config::show_all(host, port, timeout, "  ");
        std::cout << "\nMPC/MPCX information:\n";
        return run_mpc_mpcx_reader(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
}
