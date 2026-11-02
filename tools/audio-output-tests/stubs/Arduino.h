#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#define PROGMEM
#define IRAM_ATTR
#define ESP_IDF_VERSION_VAL(a,b,c) (((a)<<16)|((b)<<8)|(c))
#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL(5,0,0)
inline uint8_t pgm_read_byte(const uint8_t* p){return *p;}
inline uint32_t testNow=1;
inline uint32_t millis(){return testNow;}
inline uint32_t micros(){static uint32_t us=0;return us+=100;}
constexpr int OUTPUT=1, INPUT=0, LOW=0, HIGH=1;
inline void pinMode(int,int){}
inline void digitalWrite(int,int){}
inline void noInterrupts(){}
inline void interrupts(){}
template<class T,class L,class H> inline T constrain(T v,L lo,H hi){return v<lo?static_cast<T>(lo):v>hi?static_cast<T>(hi):v;}
inline bool isDigit(char c){return std::isdigit(static_cast<unsigned char>(c))!=0;}
inline long random(long lo,long hi){static uint32_t seed=12345;seed=seed*1664525U+1013904223U;return hi>lo?lo+seed%static_cast<uint32_t>(hi-lo):lo;}
inline std::vector<std::string> serialLines;
struct TestSerial {
  void printf(const char* fmt,...){char buf[512];va_list args;va_start(args,fmt);vsnprintf(buf,sizeof(buf),fmt,args);va_end(args);serialLines.emplace_back(buf);}
  void println(const char* s){serialLines.emplace_back(s);}
};
inline TestSerial Serial;
class String {
 public:
  String()=default;
  String(const char* s):s_(s?s:""){}
  String(std::string s):s_(s){}
  unsigned length()const{return static_cast<unsigned>(s_.size());}
  const char* c_str()const{return s_.c_str();}
  char operator[](unsigned i)const{return s_[i];}
  long toInt()const{return std::strtol(s_.c_str(),nullptr,10);}
  int indexOf(char c,unsigned from)const{auto n=s_.find(c,from);return n==std::string::npos?-1:static_cast<int>(n);}
  String substring(unsigned from,unsigned to)const{return s_.substr(from,to-from);}
  void toUpperCase(){for(auto& c:s_)c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));}
  bool operator==(const char* s)const{return s_==s;}
  bool operator!=(const char* s)const{return !(*this==s);}
 private:std::string s_;
};
