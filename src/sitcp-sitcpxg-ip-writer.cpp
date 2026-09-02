#include <iostream>
int main(int argc,char**argv){
    std::cerr << "sitcp-sitcpxg-ip-writer: write support is intentionally disabled until the runtime and EEPROM IP register mappings are verified for both SiTCP and SiTCP-XG.\n";
    std::cerr << "Use sitcp-sitcpxg-ip-reader for read-only inspection.\n";
    (void)argc; (void)argv;
    return 8;
}
