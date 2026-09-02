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
constexpr uint16_t DEFAULT_PORT=4660; constexpr double DEFAULT_TIMEOUT=3.0;
constexpr uint32_t CURRENT_MAC=0xFFFFFF12u,CURRENT_IP=0xFFFFFF18u,EEPROM_MAC=0xFFFFFC12u,EEPROM_IP=0xFFFFFC18u,EEPROM_WE=0xFFFFFCFFu;
struct Error:std::runtime_error{using std::runtime_error::runtime_error;}; struct Timeout:Error{using Error::Error;};
class Client{std::string h_;uint16_t p_;double t_;uint8_t id_=0;std::vector<uint8_t>x(uint8_t cmd,uint32_t a,const std::vector<uint8_t>&d,uint8_t len){uint8_t id=id_++;std::vector<uint8_t>q={0xff,cmd,id,len,(uint8_t)(a>>24),(uint8_t)(a>>16),(uint8_t)(a>>8),(uint8_t)a};q.insert(q.end(),d.begin(),d.end());addrinfo hi{};hi.ai_family=AF_INET;hi.ai_socktype=SOCK_DGRAM;addrinfo*r=nullptr;auto ps=std::to_string(p_);int rc=getaddrinfo(h_.c_str(),ps.c_str(),&hi,&r);if(rc)throw Error(gai_strerror(rc));int fd=socket(r->ai_family,r->ai_socktype,r->ai_protocol);if(fd<0){freeaddrinfo(r);throw Error(strerror(errno));}if(sendto(fd,q.data(),q.size(),0,r->ai_addr,r->ai_addrlen)!=(ssize_t)q.size()){freeaddrinfo(r);close(fd);throw Error(strerror(errno));}freeaddrinfo(r);fd_set f;FD_ZERO(&f);FD_SET(fd,&f);timeval tv{(long)t_,(long)((t_-(long)t_)*1000000)};rc=select(fd+1,&f,nullptr,nullptr,&tv);if(rc==0){close(fd);throw Timeout("RBCP timeout");}if(rc<0){close(fd);throw Error(strerror(errno));}uint8_t b[263];ssize_t n=recvfrom(fd,b,sizeof(b),0,nullptr,nullptr);close(fd);if(n<8||b[0]!=0xff||b[2]!=id)throw Error("invalid RBCP reply");if(b[1]&1)throw Error("RBCP bus error");return {b+8,b+n};}public:Client(std::string h,uint16_t p,double t):h_(std::move(h)),p_(p),t_(t){}std::vector<uint8_t>read(uint32_t a,uint8_t n){return x(0xc0,a,{},n);}void write(uint32_t a,const std::vector<uint8_t>&d){if(d.empty()||d.size()>255)throw Error("invalid write length");x(0x80,a,d,(uint8_t)d.size());}};
std::vector<uint8_t>rr(Client&c,uint32_t a,uint8_t n){for(int i=0;i<3;i++)try{return c.read(a,n);}catch(const Timeout&){if(i==2)throw;}throw Timeout("timeout");}
std::vector<uint8_t>parse_ip(const std::string&s){in_addr a{};if(inet_pton(AF_INET,s.c_str(),&a)!=1)throw Error("invalid IPv4 address: "+s);auto*p=(uint8_t*)&a.s_addr;return {p[0],p[1],p[2],p[3]};}
std::string ips(const std::vector<uint8_t>&d){return std::to_string(d[0])+"."+std::to_string(d[1])+"."+std::to_string(d[2])+"."+std::to_string(d[3]);}
std::string mac(const std::vector<uint8_t>&d){std::ostringstream o;o<<std::hex<<std::uppercase<<std::setfill('0');for(size_t i=0;i<d.size();i++){if(i)o<<":";o<<std::setw(2)<<(unsigned)d[i];}return o.str();}
void show(Client&c){std::cout<<"current MAC  : "<<mac(rr(c,CURRENT_MAC,6))<<"\ncurrent IP   : "<<ips(rr(c,CURRENT_IP,4))<<"\nEEPROM MAC   : "<<mac(rr(c,EEPROM_MAC,6))<<"\nEEPROM IP    : "<<ips(rr(c,EEPROM_IP,4))<<"\n";}
void usage(const char*p){std::cerr<<"Usage: "<<p<<" CURRENT_IP NEW_IP [--eeprom] [--port N] [--timeout SEC]\n";}
}
int main(int ac,char**av){try{if(ac<3||(ac==2&&(std::string(av[1])=="-h"||std::string(av[1])=="--help"))){usage(av[0]);return ac<3?2:0;}std::string host=av[1];auto newip=parse_ip(av[2]);uint16_t port=DEFAULT_PORT;double timeout=DEFAULT_TIMEOUT;for(int i=3;i<ac;i++){std::string a=av[i];if(a=="--eeprom"){}else if(a=="--port"&&i+1<ac){auto p=std::stoul(av[++i]);if(!p||p>65535)throw Error("invalid port");port=p;}else if(a=="--timeout"&&i+1<ac){timeout=std::stod(av[++i]);if(timeout<=0)throw Error("timeout must be positive");}else if(a=="--current")throw Error("--current is not enabled yet; current/runtime IP changes need separate verified handling");else throw Error("unknown option: "+a);}Client c(host,port,timeout);std::cout<<"before:\n";show(c);c.write(EEPROM_WE,{0x00});try{c.write(EEPROM_IP,newip);}catch(...){try{c.write(EEPROM_WE,{0xff});}catch(...){}throw;}c.write(EEPROM_WE,{0xff});auto rb=rr(c,EEPROM_IP,4);if(rb!=newip)throw Error("EEPROM IP read-back mismatch");std::cout<<"after:\n";show(c);std::cout<<"written target : EEPROM IP only\nstatus         : WRITE/VERIFY OK\n";return 0;}catch(const std::exception&e){std::cerr<<"ERROR: "<<e.what()<<"\n";return 1;}}
