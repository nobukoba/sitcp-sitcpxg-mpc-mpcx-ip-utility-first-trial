#pragma once
#include "sitcp-sitcpxg-rbcp.hpp"
#include <arpa/inet.h>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace sitcp_sitcpxg {
namespace network_config {
using rbcp::Client; using rbcp::Error; using rbcp::Timeout;
constexpr uint16_t DEFAULT_PORT = rbcp::DEFAULT_PORT;
constexpr double DEFAULT_TIMEOUT = rbcp::DEFAULT_TIMEOUT;
constexpr uint32_t CURRENT_MAC=0xFFFFFF12u, CURRENT_IP=0xFFFFFF18u;
constexpr uint32_t EEPROM_MAC=0xFFFFFC12u, EEPROM_IP=0xFFFFFC18u, EEPROM_WE=0xFFFFFCFFu;

inline std::vector<uint8_t> parse_ipv4(const std::string& value) {
    in_addr address{}; if (inet_pton(AF_INET,value.c_str(),&address)!=1) throw Error("invalid IPv4 address: "+value);
    const auto* b=reinterpret_cast<const uint8_t*>(&address.s_addr); return {b[0],b[1],b[2],b[3]};
}
inline std::string ipv4(const std::vector<uint8_t>& d) {
    if(d.size()!=4) throw Error("invalid IP read length");
    return std::to_string(d[0])+"."+std::to_string(d[1])+"."+std::to_string(d[2])+"."+std::to_string(d[3]);
}
inline std::string mac(const std::vector<uint8_t>& d) {
    if(d.size()!=6) throw Error("invalid MAC read length");
    std::ostringstream o; o<<std::hex<<std::uppercase<<std::setfill('0');
    for(size_t i=0;i<d.size();++i){if(i)o<<':';o<<std::setw(2)<<static_cast<unsigned>(d[i]);} return o.str();
}
struct Snapshot { std::vector<uint8_t> current_mac,current_ip,eeprom_mac,eeprom_ip; };
inline Snapshot read_snapshot(Client& c){return {rbcp::read_retry(c,CURRENT_MAC,6),rbcp::read_retry(c,CURRENT_IP,4),rbcp::read_retry(c,EEPROM_MAC,6),rbcp::read_retry(c,EEPROM_IP,4)};}
inline void print_snapshot(const Snapshot&s,const std::string&p=""){std::cout<<p<<"current MAC  : "<<mac(s.current_mac)<<'\n'<<p<<"current IP   : "<<ipv4(s.current_ip)<<'\n'<<p<<"EEPROM MAC   : "<<mac(s.eeprom_mac)<<'\n'<<p<<"EEPROM IP    : "<<ipv4(s.eeprom_ip)<<'\n';}
inline Snapshot show_all(const std::string&h,uint16_t p,double t,const std::string&prefix=""){Client c(h,p,t);auto s=read_snapshot(c);print_snapshot(s,prefix);return s;}
inline void write_eeprom_ip(const std::string&h,const std::string&v,uint16_t p,double t){Client c(h,p,t);auto b=parse_ipv4(v);c.write(EEPROM_WE,{0});try{c.write(EEPROM_IP,b);}catch(...){try{c.write(EEPROM_WE,{0xff});}catch(...){}throw;}c.write(EEPROM_WE,{0xff});if(rbcp::read_retry(c,EEPROM_IP,4)!=b)throw Error("EEPROM IP read-back mismatch");}
inline void write_current_ip(const std::string&h,const std::string&v,uint16_t p,double t){auto b=parse_ipv4(v);Client old(h,p,t);try{old.write(CURRENT_IP,b);}catch(const Timeout&){}std::this_thread::sleep_for(std::chrono::milliseconds(200));Client now(v,p,t);if(rbcp::read_retry(now,CURRENT_IP,4)!=b)throw Error("current IP read-back mismatch at new address "+v);}
} // network_config
} // sitcp_sitcpxg
