#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <cstring>
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
constexpr uint32_t CURRENT_MAC = 0xFFFFFF12u;
constexpr uint32_t CURRENT_IP  = 0xFFFFFF18u;
constexpr uint32_t EEPROM_MAC  = 0xFFFFFC12u;
constexpr uint32_t EEPROM_IP   = 0xFFFFFC18u;
struct Error : std::runtime_error { using std::runtime_error::runtime_error; };
struct Timeout : Error { using Error::Error; };

class RbcpClient {
    std::string host_; uint16_t port_; double timeout_; uint8_t id_ = 0;
public:
    RbcpClient(std::string h, uint16_t p, double t) : host_(std::move(h)), port_(p), timeout_(t) {}
    std::vector<uint8_t> read(uint32_t addr, uint8_t len) {
        uint8_t id=id_++;
        std::vector<uint8_t> q={0xff,0xc0,id,len,(uint8_t)(addr>>24),(uint8_t)(addr>>16),(uint8_t)(addr>>8),(uint8_t)addr};
        addrinfo hints{}; hints.ai_family=AF_INET; hints.ai_socktype=SOCK_DGRAM; addrinfo* res=nullptr;
        std::string ps=std::to_string(port_); int rc=getaddrinfo(host_.c_str(),ps.c_str(),&hints,&res); if(rc) throw Error(gai_strerror(rc));
        int fd=socket(res->ai_family,res->ai_socktype,res->ai_protocol); if(fd<0){freeaddrinfo(res);throw Error(strerror(errno));}
        if(sendto(fd,q.data(),q.size(),0,res->ai_addr,res->ai_addrlen)!=(ssize_t)q.size()){freeaddrinfo(res);close(fd);throw Error(strerror(errno));}
        freeaddrinfo(res); fd_set f; FD_ZERO(&f); FD_SET(fd,&f); timeval tv{(long)timeout_,(long)((timeout_-(long)timeout_)*1000000)};
        rc=select(fd+1,&f,nullptr,nullptr,&tv); if(rc==0){close(fd);throw Timeout("RBCP timeout");} if(rc<0){close(fd);throw Error(strerror(errno));}
        uint8_t b[263]; ssize_t n=recvfrom(fd,b,sizeof(b),0,nullptr,nullptr); close(fd);
        if(n<8||b[0]!=0xff||b[2]!=id) throw Error("invalid RBCP reply"); if(b[1]&1) throw Error("RBCP bus error");
        return {b+8,b+n};
    }
};
std::vector<uint8_t> read_retry(RbcpClient& c,uint32_t a,uint8_t n){for(int i=0;i<3;i++)try{return c.read(a,n);}catch(const Timeout&){if(i==2)throw;}throw Timeout("RBCP timeout");}
std::string mac(const std::vector<uint8_t>& d){if(d.size()!=6)throw Error("short MAC read");std::ostringstream o;o<<std::hex<<std::uppercase<<std::setfill('0');for(size_t i=0;i<6;i++){if(i)o<<":";o<<std::setw(2)<<(unsigned)d[i];}return o.str();}
std::string ip(const std::vector<uint8_t>& d){if(d.size()!=4)throw Error("short IP read");return std::to_string(d[0])+"."+std::to_string(d[1])+"."+std::to_string(d[2])+"."+std::to_string(d[3]);}
void usage(const char* p){std::cerr<<"Usage: "<<p<<" IP [--port N] [--timeout SEC]\n";}
}
int main(int argc,char** argv){try{if(argc<2||(argc==2&&(std::string(argv[1])=="-h"||std::string(argv[1])=="--help"))){usage(argv[0]);return argc<2?2:0;}std::string target=argv[1];uint16_t port=DEFAULT_PORT;double timeout=DEFAULT_TIMEOUT;for(int i=2;i<argc;i++){std::string a=argv[i];if(a=="--port"&&i+1<argc){auto p=std::stoul(argv[++i]);if(!p||p>65535)throw Error("invalid port");port=(uint16_t)p;}else if(a=="--timeout"&&i+1<argc){timeout=std::stod(argv[++i]);if(timeout<=0)throw Error("timeout must be positive");}else throw Error("unknown option: "+a);}RbcpClient c(target,port,timeout);auto cm=read_retry(c,CURRENT_MAC,6),ci=read_retry(c,CURRENT_IP,4),em=read_retry(c,EEPROM_MAC,6),ei=read_retry(c,EEPROM_IP,4);std::cout<<"target       : "<<target<<":"<<port<<"\ncurrent MAC  : "<<mac(cm)<<"\ncurrent IP   : "<<ip(ci)<<"\nEEPROM MAC   : "<<mac(em)<<"\nEEPROM IP    : "<<ip(ei)<<"\nstatus       : READ OK\n";return 0;}catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<"\n";return 1;}}
