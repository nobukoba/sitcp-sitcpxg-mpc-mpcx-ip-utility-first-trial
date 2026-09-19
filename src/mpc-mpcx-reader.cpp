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
constexpr double DEFAULT_TIMEOUT=3.0; constexpr uint16_t DEFAULT_PORT=4660; constexpr uint32_t EEPROM_BASE=0xFFFFFC00u; constexpr uint32_t XG_PROBE=0xFFFFFF50u; constexpr int FW=20;
struct Error:std::runtime_error{using std::runtime_error::runtime_error;}; struct Timeout:Error{using Error::Error;}; struct BusError:Error{using Error::Error;};
class Client{std::string h_;uint16_t p_;double t_;uint8_t id_=0;public:Client(std::string h,uint16_t p,double t):h_(std::move(h)),p_(p),t_(t){} std::vector<uint8_t> read(uint32_t a,size_t n){uint8_t id=id_++;std::vector<uint8_t> q={0xff,0xc0,id,(uint8_t)n,(uint8_t)(a>>24),(uint8_t)(a>>16),(uint8_t)(a>>8),(uint8_t)a};addrinfo hi{};hi.ai_family=AF_INET;hi.ai_socktype=SOCK_DGRAM;addrinfo*r=nullptr;auto ps=std::to_string(p_);int g=getaddrinfo(h_.c_str(),ps.c_str(),&hi,&r);if(g)throw Error(gai_strerror(g));int fd=socket(r->ai_family,r->ai_socktype,r->ai_protocol);if(fd<0){freeaddrinfo(r);throw Error(strerror(errno));}if(sendto(fd,q.data(),q.size(),0,r->ai_addr,r->ai_addrlen)!=(ssize_t)q.size()){freeaddrinfo(r);close(fd);throw Error(strerror(errno));}freeaddrinfo(r);fd_set f;FD_ZERO(&f);FD_SET(fd,&f);timeval tv{(long)t_,(long)((t_-(long)t_)*1000000)};int rv=select(fd+1,&f,nullptr,nullptr,&tv);if(rv==0){close(fd);throw Timeout("RBCP timeout");}if(rv<0){close(fd);throw Error(strerror(errno));}uint8_t b[263];ssize_t z=recvfrom(fd,b,sizeof(b),0,nullptr,nullptr);close(fd);if(z<8||b[0]!=0xff||b[2]!=id)throw Error("invalid RBCP reply");if(b[1]&1)throw BusError("RBCP bus error");return {b+8,b+z};}};
std::vector<uint8_t> rr(Client&c,uint32_t a,size_t n){for(int i=0;i<3;i++)try{return c.read(a,n);}catch(const Timeout&){if(i==2)throw;}throw Timeout("timeout");}
std::vector<uint8_t> exact(Client&c,uint32_t a,size_t n){std::vector<uint8_t>o;for(size_t x=0;x<n;x+=8){size_t m=std::min<size_t>(8,n-x);auto b=rr(c,a+x,m);if(b.size()!=m)throw Error("short read");o.insert(o.end(),b.begin(),b.end());}return o;}
bool tag(std::vector<uint8_t>b){if(b.size()!=7)return false;for(auto x:b){if(x==0||x==' '||x=='-'||(x>='0'&&x<='9'))continue;x&=0xdf;if(x<'A'||x>'Z')return false;}return true;}
int cls(const std::vector<uint8_t>&d){if(d.size()!=22)return 0;std::vector<uint8_t>a,b;for(int i=6;i<13;i++)a.push_back(d[i]?d[i]-0x34:0);for(int i=0;i<7;i++)b.push_back(d[i]?d[i]-0x2c:0);if(tag(a))return 2;if(tag(b))return 1;return 0;}
std::vector<uint8_t>xgp(const std::vector<uint8_t>&e){std::vector<uint8_t>p(e.begin(),e.begin()+16);p.insert(p.end(),e.begin()+18,e.begin()+24);return p;} std::vector<uint8_t>np(const std::vector<uint8_t>&e){std::vector<uint8_t>p(e.begin()+0x12,e.begin()+0x18);p.insert(p.end(),e.begin()+0x40,e.begin()+0x50);return p;}
std::string hex(const std::vector<uint8_t>&d,size_t a=0,size_t z=SIZE_MAX,char s=' '){z=std::min(z,d.size());std::ostringstream o;o<<std::hex<<std::setfill('0');for(size_t i=a;i<z;i++){if(i>a)o<<s;o<<std::setw(2)<<(unsigned)d[i];}return o.str();} void field(std::string k,std::string v){std::cout<<std::left<<std::setw(FW)<<k<<": "<<v<<'\n';}
std::string tn(int t){return t==1?"MPCX (SiTCP-XG)":t==2?"MPC (normal SiTCP)":t==-1?"ambiguous":"unknown";}
int detect(Client&c,const std::vector<uint8_t>&e,std::string&w){bool x=cls(xgp(e))==1,n=cls(np(e))==2;if(x&&!n){w="EEPROM payload";return 1;}if(n&&!x){w="EEPROM payload";return 2;}if(!x&&!n){w="EEPROM payload not classified";return 0;}try{rr(c,XG_PROBE,1);w="XG register probe: readable";return 1;}catch(const BusError&){w="XG register probe: bus error";return 2;}catch(const Timeout&){w="XG register probe: timeout";return -1;}}
void usage(const char*a){std::cerr<<"Usage: "<<a<<" <ip> [options]\n\nOptions:\n  --port N       RBCP UDP port (default: "<<DEFAULT_PORT<<")\n  --timeout SEC  RBCP timeout in seconds (default: "<<DEFAULT_TIMEOUT<<")\n  -h, --help     Show this help\n";}
}
int main(int ac,char**av){try{if(ac<2||(ac==2&&(std::string(av[1])=="-h"||std::string(av[1])=="--help"))){usage(av[0]);return ac<2?2:0;}std::string ip=av[1];uint16_t port=DEFAULT_PORT;double timeout=DEFAULT_TIMEOUT;for(int i=2;i<ac;i++){std::string a=av[i];if(a=="--port"&&i+1<ac){auto p=std::stoul(av[++i]);if(!p||p>65535)throw Error("invalid port");port=p;}else if(a=="--timeout"&&i+1<ac){timeout=std::stod(av[++i]);if(timeout<=0)throw Error("timeout must be positive");}else if(a=="-h"||a=="--help"){usage(av[0]);return 0;}else throw Error("unknown option: "+a);}Client c(ip,port,timeout);auto e=exact(c,EEPROM_BASE,0x50);std::string why;int t=detect(c,e,why);auto p=t==1?xgp(e):t==2?np(e):std::vector<uint8_t>{};field("command","read");field("target",ip+":"+std::to_string(port));field("detected type",tn(t));field("detection",why);if(!p.empty())field("reconstructed payload",hex(p));if(t==1){field("MPCX FC00..FC0F",hex(e,0,16));field("MAC",hex(e,0x12,0x18,':'));}else if(t==2){field("MAC",hex(e,0x12,0x18,':'));field("MPC FC40..FC4F",hex(e,0x40,0x50));}field("EEPROM IP",std::to_string(e[0x18])+"."+std::to_string(e[0x19])+"."+std::to_string(e[0x1a])+"."+std::to_string(e[0x1b]));field("status","READ OK");std::cout<<"raw EEPROM FC00..FC4F:\n";for(size_t o=0;o<e.size();o+=16)std::cout<<std::hex<<std::uppercase<<std::setw(8)<<std::setfill('0')<<(EEPROM_BASE+o)<<": "<<hex(e,o,std::min(o+16,e.size()))<<'\n';return 0;}catch(const std::exception&e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}}
