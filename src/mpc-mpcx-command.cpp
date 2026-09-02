#include <arpa/inet.h>
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <netdb.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {
constexpr uint16_t DEFAULT_PORT = 4660;
constexpr double DEFAULT_TIMEOUT = 3.0;
constexpr uint32_t EEPROM_BASE = 0xFFFFFC00u;
constexpr uint32_t EEPROM_WRITE_ENABLE = 0xFFFFFCFFu;
constexpr int FIELD_WIDTH = 20;

struct Error : std::runtime_error { using std::runtime_error::runtime_error; };
struct Timeout : Error { using Error::Error; };
struct BusError : Error { using Error::Error; };

std::string hex_bytes(const std::vector<uint8_t>& data, char sep = ' ') {
    std::ostringstream os;
    os << std::hex << std::setfill('0');
    for (size_t i = 0; i < data.size(); ++i) {
        if (i) os << sep;
        os << std::setw(2) << static_cast<unsigned>(data[i]);
    }
    return os.str();
}

std::string hex_address(uint32_t address) {
    std::ostringstream os;
    os << "0x" << std::hex << std::setw(8) << std::setfill('0') << address;
    return os.str();
}

void field(const std::string& key, const std::string& value) {
    std::cout << std::left << std::setw(FIELD_WIDTH) << key << ": " << value << '\n';
}

class RbcpClient {
public:
    RbcpClient(std::string host, uint16_t port, double timeout)
        : host_(std::move(host)), port_(port), timeout_(timeout) {}

    std::vector<uint8_t> read(uint32_t address, size_t length) {
        if (length > 255) throw Error("one RBCP read is limited to 255 bytes");
        return transaction(0xC0, address, {}, static_cast<uint8_t>(length));
    }

    std::vector<uint8_t> write(uint32_t address, const std::vector<uint8_t>& data) {
        if (data.size() > 255) throw Error("one RBCP write is limited to 255 bytes");
        return transaction(0x80, address, data, static_cast<uint8_t>(data.size()));
    }

private:
    std::vector<uint8_t> transaction(uint8_t cmd, uint32_t addr,
                                     const std::vector<uint8_t>& payload, uint8_t len) {
        const uint8_t id = id_++;
        std::vector<uint8_t> packet = {0xff, cmd, id, len,
            static_cast<uint8_t>(addr >> 24), static_cast<uint8_t>(addr >> 16),
            static_cast<uint8_t>(addr >> 8), static_cast<uint8_t>(addr)};
        packet.insert(packet.end(), payload.begin(), payload.end());

        addrinfo hints{}; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;
        addrinfo* res = nullptr;
        const std::string ps = std::to_string(port_);
        const int gai = getaddrinfo(host_.c_str(), ps.c_str(), &hints, &res);
        if (gai) throw Error(gai_strerror(gai));
        const int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (fd < 0) { freeaddrinfo(res); throw Error(strerror(errno)); }
        const ssize_t sent = sendto(fd, packet.data(), packet.size(), 0, res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);
        if (sent != static_cast<ssize_t>(packet.size())) { close(fd); throw Error(strerror(errno)); }

        fd_set fds; FD_ZERO(&fds); FD_SET(fd, &fds);
        timeval tv{static_cast<long>(timeout_), static_cast<long>((timeout_ - static_cast<long>(timeout_)) * 1000000.0)};
        const int rv = select(fd + 1, &fds, nullptr, nullptr, &tv);
        if (rv == 0) { close(fd); throw Timeout("RBCP timeout"); }
        if (rv < 0) { close(fd); throw Error(strerror(errno)); }
        uint8_t buf[263];
        const ssize_t n = recvfrom(fd, buf, sizeof(buf), 0, nullptr, nullptr);
        close(fd);
        if (n < 8 || buf[0] != 0xff || buf[2] != id) throw Error("invalid RBCP reply");
        if (buf[1] & 1) throw BusError("RBCP bus error");
        return {buf + 8, buf + n};
    }
    std::string host_; uint16_t port_; double timeout_; uint8_t id_ = 0;
};

std::vector<uint8_t> read_retry(RbcpClient& c, uint32_t addr, size_t len) {
    for (int i = 0; i < 3; ++i) {
        try { return c.read(addr, len); }
        catch (const Timeout&) { if (i == 2) throw; }
    }
    throw Timeout("RBCP timeout");
}

std::vector<uint8_t> read_exact(RbcpClient& c, uint32_t addr, size_t len) {
    std::vector<uint8_t> out;
    for (size_t off = 0; off < len; off += 8) {
        const size_t n = std::min<size_t>(8, len - off);
        auto b = read_retry(c, addr + static_cast<uint32_t>(off), n);
        if (b.size() != n) throw Error("short read");
        out.insert(out.end(), b.begin(), b.end());
    }
    return out;
}

bool valid_tag(std::vector<uint8_t> b) {
    if (b.size() != 7) return false;
    for (auto x : b) {
        if (x == 0 || x == ' ' || x == '-' || (x >= '0' && x <= '9')) continue;
        x &= 0xdf;
        if (x < 'A' || x > 'Z') return false;
    }
    return true;
}

int classify(const std::vector<uint8_t>& d) {
    if (d.size() != 22) return 0;
    std::vector<uint8_t> a, b;
    for (size_t i = 6; i < 13; ++i) a.push_back(d[i] ? static_cast<uint8_t>(d[i] - 0x34) : 0);
    for (size_t i = 0; i < 7; ++i) b.push_back(d[i] ? static_cast<uint8_t>(d[i] - 0x2c) : 0);
    if (valid_tag(a)) return 2;
    if (valid_tag(b)) return 1;
    return 0;
}

std::string type_name(int t) {
    return t == 1 ? "MPCX (SiTCP-XG)" : t == 2 ? "MPC (normal SiTCP)" : "unknown";
}

std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw Error("cannot open file: " + path);
    return {(std::istreambuf_iterator<char>(f)), {}};
}

uint32_t parse_u32(const std::string& s) {
    size_t pos = 0; unsigned long v = std::stoul(s, &pos, 0);
    if (pos != s.size() || v > 0xffffffffUL) throw Error("invalid integer: " + s);
    return static_cast<uint32_t>(v);
}

std::vector<uint8_t> parse_hex(std::string s) {
    for (char& c : s) if (c == ',' || c == ':') c = ' ';
    std::istringstream is(s); std::vector<uint8_t> out; std::string tok;
    while (is >> tok) {
        if (tok.rfind("0x", 0) == 0 || tok.rfind("0X", 0) == 0) tok = tok.substr(2);
        const unsigned long v = std::stoul(tok, nullptr, 16);
        if (v > 255) throw Error("hex byte out of range: " + tok);
        out.push_back(static_cast<uint8_t>(v));
    }
    if (out.empty()) throw Error("no hex bytes supplied");
    return out;
}

struct TargetArgs { std::string ip; uint16_t port = DEFAULT_PORT; double timeout = DEFAULT_TIMEOUT; };
TargetArgs parse_target(int argc, char** argv, int start) {
    if (start >= argc) throw Error("missing IP address");
    TargetArgs a; a.ip = argv[start++];
    for (int i = start; i < argc; ++i) {
        std::string x = argv[i];
        if (x == "--port" && i + 1 < argc) { auto p = std::stoul(argv[++i]); if (!p || p > 65535) throw Error("invalid port"); a.port = static_cast<uint16_t>(p); }
        else if (x == "--timeout" && i + 1 < argc) { a.timeout = std::stod(argv[++i]); if (a.timeout <= 0) throw Error("timeout must be positive"); }
        else throw Error("unknown option: " + x);
    }
    return a;
}

void usage(const char* p) {
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
              << "  write IP FILE [--port N] [--timeout SEC]  (use mpc-mpcx-writer)\n";
}
}

int main(int argc, char** argv) {
    try {
        if (argc < 2 || std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") { usage(argv[0]); return argc < 2 ? 2 : 0; }
        const std::string cmd = argv[1];

        if (cmd == "inspect") {
            if (argc != 3) throw Error("usage: inspect FILE");
            auto p = read_file(argv[2]);
            field("command", "inspect"); field("file", argv[2]); field("size", std::to_string(p.size()) + " bytes");
            field("payload type", type_name(classify(p))); field("writer type", std::to_string(classify(p))); field("payload", hex_bytes(p));
            return classify(p) ? 0 : 2;
        }

        if (cmd == "read") {
            auto a = parse_target(argc, argv, 2); RbcpClient c(a.ip, a.port, a.timeout); auto e = read_exact(c, EEPROM_BASE, 0x50);
            field("command", "read"); field("target", a.ip + ":" + std::to_string(a.port)); field("EEPROM FC00..FC4F", hex_bytes(e)); field("status", "READ OK"); return 0;
        }

        if (cmd == "probe") {
            if (argc < 4) throw Error("usage: probe IP ADDRESS [LENGTH]");
            const std::string ip = argv[2]; const uint32_t addr = parse_u32(argv[3]); size_t len = 1; int opt = 4;
            if (opt < argc && std::string(argv[opt]).rfind("--",0) != 0) { len = std::stoul(argv[opt++], nullptr, 0); }
            std::vector<std::string> tmp = {argv[0], ip}; for (int i=opt;i<argc;++i) tmp.emplace_back(argv[i]);
            std::vector<char*> av; for (auto& s:tmp) av.push_back(s.data()); auto a = parse_target((int)av.size(), av.data(), 1);
            auto d = RbcpClient(a.ip,a.port,a.timeout).read(addr,len); field("command","probe"); field("target",a.ip+":"+std::to_string(a.port)); field("address",hex_address(addr)); field("data",hex_bytes(d)); field("status","RBCP REACHABLE"); return 0;
        }

        if (cmd == "rbcp-read") {
            if (argc < 5) throw Error("usage: rbcp-read IP ADDRESS LENGTH");
            const std::string ip=argv[2]; uint32_t addr=parse_u32(argv[3]); size_t len=std::stoul(argv[4],nullptr,0);
            std::vector<std::string> tmp={argv[0],ip}; for(int i=5;i<argc;++i)tmp.emplace_back(argv[i]); std::vector<char*> av;for(auto&s:tmp)av.push_back(s.data());auto a=parse_target((int)av.size(),av.data(),1);
            auto d=RbcpClient(a.ip,a.port,a.timeout).read(addr,len);field("command","rbcp-read");field("address",hex_address(addr));field("data",hex_bytes(d));return 0;
        }

        if (cmd == "rbcp-write") {
            if (argc < 5) throw Error("usage: rbcp-write IP ADDRESS HEX-BYTES");
            const std::string ip=argv[2];uint32_t addr=parse_u32(argv[3]);auto bytes=parse_hex(argv[4]);
            std::vector<std::string> tmp={argv[0],ip};for(int i=5;i<argc;++i)tmp.emplace_back(argv[i]);std::vector<char*> av;for(auto&s:tmp)av.push_back(s.data());auto a=parse_target((int)av.size(),av.data(),1);
            auto ack=RbcpClient(a.ip,a.port,a.timeout).write(addr,bytes);(void)ack;field("command","rbcp-write");field("address",hex_address(addr));field("data",hex_bytes(bytes));field("status","WRITE OK");return 0;
        }

        if (cmd == "verify" || cmd == "mpcx-plan") {
            if (argc < 4) throw Error("missing IP or FILE");
            const std::string ip=argv[2], file=argv[3];auto payload=read_file(file);int t=classify(payload);if(!t)throw Error("invalid/unknown 22-byte MPC payload");
            std::vector<std::string> tmp={argv[0],ip};for(int i=4;i<argc;++i)tmp.emplace_back(argv[i]);std::vector<char*> av;for(auto&s:tmp)av.push_back(s.data());auto a=parse_target((int)av.size(),av.data(),1);RbcpClient c(a.ip,a.port,a.timeout);auto e=read_exact(c,EEPROM_BASE,0x50);
            if(cmd=="mpcx-plan"){if(t!=1)throw Error("payload is not classified as SiTCP-XG");std::vector<uint8_t> expected(e.begin(),e.begin()+24);std::copy(payload.begin(),payload.begin()+16,expected.begin());std::copy(payload.begin()+16,payload.end(),expected.begin()+18);field("command","mpcx-plan");field("preserved FC10..FC11",hex_bytes({e[16],e[17]}));field("EEPROM record",hex_bytes(expected));field("status","NO WRITE PERFORMED");return 0;}
            bool ok=false;if(t==1){std::vector<uint8_t> expected(e.begin(),e.begin()+24);std::copy(payload.begin(),payload.begin()+16,expected.begin());std::copy(payload.begin()+16,payload.end(),expected.begin()+18);ok=std::equal(expected.begin(),expected.end(),e.begin());}else{ok=std::equal(payload.begin(),payload.begin()+6,e.begin()+0x12)&&std::equal(payload.begin()+6,payload.end(),e.begin()+0x40);}field("command","verify");field("file type",type_name(t));field("match",ok?"YES":"NO");field("status",ok?"VERIFY OK":"VERIFY FAILED");return ok?0:6;
        }

        if (cmd == "clear") {
            if (argc < 4 || std::string(argv[3]) != "--yes-really-clear") throw Error("clear is destructive; add --yes-really-clear immediately after IP");
            const std::string ip=argv[2];std::vector<std::string> tmp={argv[0],ip};for(int i=4;i<argc;++i)tmp.emplace_back(argv[i]);std::vector<char*> av;for(auto&s:tmp)av.push_back(s.data());auto a=parse_target((int)av.size(),av.data(),1);RbcpClient c(a.ip,a.port,a.timeout);
            c.write(EEPROM_WRITE_ENABLE,{0x00});try{std::vector<uint8_t> ff(16,0xff);for(uint32_t off=0;off<0x80;off+=16)c.write(EEPROM_BASE+off,ff);}catch(...){try{c.write(EEPROM_WRITE_ENABLE,{0xff});}catch(...){}throw;}c.write(EEPROM_WRITE_ENABLE,{0xff});field("command","clear");field("EEPROM area","0xFFFFFC00..0xFFFFFC7F");field("status","CLEAR OK");return 0;
        }

        if (cmd == "write") {
            std::cerr << "Use mpc-mpcx-writer for the verified high-level write path.\n";
            return 8;
        }

        throw Error("unknown command: " + cmd);
    } catch (const std::exception& e) { std::cerr << "ERROR: " << e.what() << '\n'; return 1; }
}
