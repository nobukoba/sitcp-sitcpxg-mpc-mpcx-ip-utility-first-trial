#include <iostream>

int main(int argc, char** argv) {
    std::cerr
        << "sitcp-sitcpxg-ip-writer: IP-only SiTCP Utility compatible writer is not implemented yet.\n"
        << "This command is intentionally independent of MPC/MPCX license/payload handling.\n"
        << "It will change only the SiTCP/SiTCP-XG IP configuration.\n"
        << "EEPROM will be the default target; changing the current/runtime IP will require an explicit option.\n"
        << "Write support remains disabled until the exact IP-only access method is verified.\n";
    (void)argc;
    (void)argv;
    return 8;
}
