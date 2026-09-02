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
struct Error:std::runtime_error{using std::runtime_error::runtime_error;}; struct Timeout:Error{using Error::Error;};
class RbcpClient{std::string h_;uint16_t p_;double t_;uint8_t id_=0;public:RbcpClient(std::string h,uint16_t p,double t):h_(std::move(h)),p_(p),t_(t){}std::vector<uint8_t>read(uint32_t a,uint8_t l){uint8_t id=id_++;std::vector<uint8_t>q={0xff,0xc0,id,l,(uint8_t)(a>>24),(uint8_t)(a>>16),(uint8_t)(a>>8),(uint8_t)a};addrinfo hi{};hi.ai_family=AF_INET;hi.ai_socktype=SOCK_DGRAM;addrinfo*r=nullptr;auto ps=std::to_string(p_);int rc=getaddrinfo(h_.c_str(),ps.c_str(),&hi,&r);if(rc)throw Error(gai_strerror(rc));int fd=socket(r->ai_family,r->ai_socktype,r->ai_protocol);if(fd<0){freeaddrinfo(r);throw Error(strerror(errno));}if(sendto(fd,q.data(),q.size(),0,r->ai_addr,r->ai_addrlen)!=(ssize_t)q.size()){freeaddrinfo(r);close(fd);throw Error(strerror(errno));}freeaddrinfo(r);fd_set f;FD_ZERO(&f);FD_SET(fd,&f);timeval tv{(long)t_,(long)((t_-(long)t_)*1000000)};rc=select(fd+1,&f,nullptr,nullptr,&tv);if(rc==0){close(fd);throw Timeout("RBCP timeout");}if(rc<0){close(fd);throw Error(strerror(errno));}uint8_t b[263];ssize_t n=recvfrom(fd,b,sizeof(b),0,nullptr,nullptr);close(fd);if(n<8||b[0]!=0xff||b[2]!=id)throw Error("invalid RBCP reply");if(b[1]&1)throw Error("RBCP bus error");return {b+8,b+n};}};
std::vector<uint8_t>rr(RbcpClient&c,uint32_t a,uint8_t n){for(int i=0;i<3;i++)try{return c.read(a,n);}catch(const Timeout&){if(i==2)throw;}throw Timeout("RBCP timeout");}
std::string mac(const std::vector<uint8_t>&d){if(d.size()!=6)throw Error("short MAC read");std::ostringstream o;o<<std::hex<<std::uppercase<<std::setfill('0');for(size_t i=0;i<d.size();++i){if(i)o<<':';o<<std::setw(2)<<(unsigned)d[i];}return o.str();}
std::string ip(const std::vector<uint8_t>&d){if(d.size()!=4)throw Error("short IP read");return std::to_string(d[0])+"."+std::to_string(d[1])+"."+std::to_string(d[2])+"."+std::to_string(d[3]);}
void usage(const char*p){std::cerr<<"Usage: "<<p<<" IP [options]\n\nOptions:\n  --port N       RBCP UDP port (default: "<<DEFAULT_PORT<<")\n  --timeout SEC  RBCP timeout in seconds (default: "<<DEFAULT_TIMEOUT<<")\n  -h, --help     Show this help\n";}
}
int main(int ac,char**av){try{if(ac<2||(ac==2&&(std::string(av[1])=="-h"||std::string(av[1])=="--help"))){usage(av[0]);return ac<2?2:0;}std::string target=av[1];uint16_t port=DEFAULT_PORT;double timeout=DEFAULT_TIMEOUT;for(int i=2;i<ac;++i){std::string a=av[i];if(a=="--port"&&i+1<ac){auto v=std::stoul(av[++i]);if(!v||v>65535)throw Error("invalid port");port=(uint16_t)v;}else if(a=="--timeout"&&i+1<ac){timeout=std::stod(av[++i]);if(timeout<=0)throw Error("timeout must be positive");}else throw Error("unknown option: "+a);}RbcpClient c(target,port,timeout);std::cout<<"target       : "<<target<<':'<<port<<'\n'<<"current MAC  : "<<mac(rr(c,CURRENT_MAC,6))<<'\n'<<"current IP   : "<<ip(rr(c,CURRENT_IP,4))<<'\n'<<"EEPROM MAC   : "<<mac(rr(c,EEPROM_MAC,6))<<'\n'<<"EEPROM IP    : "<<ip(rr(c,EEPROM_IP,4))<<'\n'<<"status       : READ OK\n";return 0;}catch(const std::exception&e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}}
