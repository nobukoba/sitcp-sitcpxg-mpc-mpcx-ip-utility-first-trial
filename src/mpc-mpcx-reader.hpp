#pragma once

#include <arpa/inet.h>
#include <algorithm>
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
constexpr double DEFAULT_TIMEOUT=3.0;\nconstexpr uint16_t DEFAULT_PORT=4660;\nconstexpr uint32_t EEPROM_BASE=0xFFFFFC00u;\nconstexpr uint32_t XG_PROBE=0xFFFFFF50u;\nconstexpr int FW=20;
struct Error:std::runtime_error{using std::runtime_error::runtime_error;};\n\nstruct Timeout:Error{using Error::Error;};\n\nstruct BusError:Error{using Error::Error;};
class Client{\nstd::string h_;uint16_t p_;double t_;\nuint8_t id_=0;public:Client(std::string h,uint16_t p,double t):h_(std::move(h)),p_(p),t_(t){} std::vector<uint8_t> read(uint32_t a,size_t n){\nuint8_t id=id_++;\nstd::vector<uint8_t> q={0xff,0xc0,id,(uint8_t)n,(uint8_t)(a>>24),(uint8_t)(a>>16),(uint8_t)(a>>8),(uint8_t)a};addrinfo hi{};hi.ai_family=AF_INET;hi.ai_socktype=SOCK_DGRAM;addrinfo*r=nullptr;\nauto ps=std::to_string(p_);\nint g=getaddrinfo(h_.c_str(),ps.c_str(),&hi,&r);\nif(g)throw Error(gai_strerror(g));\nint fd=socket(r->ai_family,r->ai_socktype,r->ai_protocol);\nif(fd<0){freeaddrinfo(r);\nthrow Error(strerror(errno));}\nif(sendto(fd,q.data(),q.size(),0,r->ai_addr,r->ai_addrlen)!=(ssize_t)q.size()){freeaddrinfo(r);close(fd);\nthrow Error(strerror(errno));}freeaddrinfo(r);\nfd_set f;FD_ZERO(&f);FD_SET(fd,&f);\ntimeval tv{}; tv.tv_sec=static_cast<decltype(tv.tv_sec)>(t_); tv.tv_usec=static_cast<decltype(tv.tv_usec)>((t_-static_cast<long>(t_))*1000000.0);\nint rv=select(fd+1,&f,nullptr,nullptr,&tv);\nif(rv==0){close(fd);\nthrow Timeout("RBCP timeout");}\nif(rv<0){close(fd);\nthrow Error(strerror(errno));}uint8_t b[263];\nssize_t z=recvfrom(fd,b,sizeof(b),0,nullptr,nullptr);close(fd);\nif(z<8||b[0]!=0xff||b[2]!=id)throw Error("invalid RBCP reply");\nif(b[1]&1)throw BusError("RBCP bus error");\nreturn {b+8,b+z};}};
std::vector<uint8_t> rr(Client&c,uint32_t a,size_t n){\nfor(int i=0;i<3;i++)try{\nreturn c.read(a,n);}\ncatch(const Timeout&){\nif(i==2)throw;}\nthrow Timeout("timeout");}
std::vector<uint8_t> exact(Client&c,uint32_t a,size_t n){\nstd::vector<uint8_t>o;\nfor(size_t x=0;x<n;x+=8){\nsize_t m=std::min<size_t>(8,n-x);\nauto b=rr(c,a+x,m);\nif(b.size()!=m)throw Error("short read");o.insert(o.end(),b.begin(),b.end());}\nreturn o;}
bool tag(std::vector<uint8_t>b){\nif(b.size()!=7)return false;\nfor(auto x:b){\nif(x==0||x==' '||x=='-'||(x>='0'&&x<='9'))continue;x&=0xdf;\nif(x<'A'||x>'Z')return false;}\nreturn true;}
int cls(const std::vector<uint8_t>&d){\nif(d.size()!=22)return 0;\nstd::vector<uint8_t>a,b;\nfor(int i=6;i<13;i++)a.push_back(d[i]?d[i]-0x34:0);\nfor(int i=0;i<7;i++)b.push_back(d[i]?d[i]-0x2c:0);\nif(tag(a))return 2;\nif(tag(b))return 1;\nreturn 0;}
std::vector<uint8_t>xgp(const std::vector<uint8_t>&e){\nstd::vector<uint8_t>p(e.begin(),e.begin()+16);p.insert(p.end(),e.begin()+18,e.begin()+24);\nreturn p;} std::vector<uint8_t>np(const std::vector<uint8_t>&e){\nstd::vector<uint8_t>p(e.begin()+0x12,e.begin()+0x18);p.insert(p.end(),e.begin()+0x40,e.begin()+0x50);\nreturn p;}
std::string hex(const std::vector<uint8_t>&d,size_t a=0,size_t z=SIZE_MAX,char s=' '){z=std::min(z,d.size());\nstd::ostringstream o;o<<std::hex<<std::setfill('0');\nfor(size_t i=a;i<z;i++){\nif(i>a)o<<s;o<<std::setw(2)<<(unsigned)d[i];}\nreturn o.str();} void field(std::string k,std::string v){\nstd::cout<<std::left<<std::setw(FW)<<k<<": "<<v<<'\n';}
std::string tn(int t){\nreturn t==1?"MPCX (SiTCP-XG)":t==2?"MPC (normal SiTCP)":t==-1?"ambiguous":"unknown";}
int detect(Client&c,const std::vector<uint8_t>&e,std::string&w){\nbool x=cls(xgp(e))==1,n=cls(np(e))==2;\nif(x&&!n){w="EEPROM payload";\nreturn 1;}\nif(n&&!x){w="EEPROM payload";\nreturn 2;}\nif(!x&&!n){w="EEPROM payload not classified";\nreturn 0;}try{rr(c,XG_PROBE,1);w="XG register probe: readable";\nreturn 1;}\ncatch(const BusError&){w="XG register probe: not supported";\nreturn 2;}\ncatch(const Timeout&){w="XG register probe: timeout";\nreturn -1;}}
void usage(const char*a){\nstd::cerr<<"Usage: "<<a<<" <ip> [options]\n\nOptions:\n  --port N       RBCP UDP port (default: "<<DEFAULT_PORT<<")\n  --timeout SEC  RBCP timeout in seconds (default: "<<DEFAULT_TIMEOUT<<")\n  -h, --help     Show this help\n";}
}
inline int run_mpc_mpcx_reader(int ac,char**av){\ntry{\nif(ac<2||(ac==2&&(std::string(av[1])=="-h"||std::string(av[1])=="--help"))){usage(av[0]);\nreturn ac<2?2:0;}\nstd::string ip=av[1];uint16_t port=DEFAULT_PORT;double timeout=DEFAULT_TIMEOUT;\nfor(int i=2;i<ac;i++){\nstd::string a=av[i];\nif(a=="--port"&&i+1<ac){\nauto p=std::stoul(av[++i]);\nif(!p||p>65535)throw Error("invalid port");port=p;}\nelse if(a=="--timeout"&&i+1<ac){timeout=std::stod(av[++i]);\nif(timeout<=0)throw Error("timeout must be positive");}\nelse if(a=="-h"||a=="--help"){usage(av[0]);\nreturn 0;}\nelse throw Error("unknown option: "+a);}\nClient c(ip,port,timeout);\nauto e=exact(c,EEPROM_BASE,0x50);\nstd::string why;\nint t=detect(c,e,why);\nauto p=t==1?xgp(e):t==2?np(e):std::vector<uint8_t>{};field("command","read");field("target",ip+":"+std::to_string(port));field("detected type",tn(t));field("detection",why);\nif(!p.empty())field("reconstructed payload",hex(p));\nif(t==1){field("MPCX FC00..FC0F",hex(e,0,16));field("MAC",hex(e,0x12,0x18,':'));}\nelse if(t==2){field("MAC",hex(e,0x12,0x18,':'));field("MPC FC40..FC4F",hex(e,0x40,0x50));}\nfield("EEPROM IP",std::to_string(e[0x18])+"."+std::to_string(e[0x19])+"."+std::to_string(e[0x1a])+"."+std::to_string(e[0x1b]));field("status","READ OK");\nstd::cout<<"raw EEPROM FC00..FC4F:\n";\nfor(size_t o=0;o<e.size();o+=16)std::cout<<std::hex<<std::uppercase<<std::setw(8)<<std::setfill('0')<<(EEPROM_BASE+o)<<": "<<hex(e,o,std::min(o+16,e.size()))<<'\n';\nreturn 0;}\ncatch(const std::exception&e){\nstd::cerr<<"ERROR: "<<e.what()<<'\n';\nreturn 1;}}
