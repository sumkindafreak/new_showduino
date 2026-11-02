#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <string>
#include <vector>
#include <map>
#define ESP_ARDUINO_VERSION_MAJOR 3
template<class... T> inline void testLog(T...){}
#define SD_LOGI(...) testLog(__VA_ARGS__)
#define SD_LOGW(...) testLog(__VA_ARGS__)
#define SD_LOGT(...) testLog(__VA_ARGS__)
inline uint32_t testNow=1;
inline uint32_t millis(){return testNow;}
enum {OUTPUT=1,LOW=0,HIGH=1};
inline unsigned duty[40]{};
inline void pinMode(int,int){}
inline void digitalWrite(int pin,int value){duty[pin]=value;}
inline bool ledcAttach(int,unsigned,unsigned){return true;}
inline bool ledcWrite(int pin,unsigned value){duty[pin]=value;return true;}
inline std::vector<std::string> logLines;
struct SerialDouble {
 void printf(const char* fmt,...){char text[256];va_list args;va_start(args,fmt);vsnprintf(text,sizeof(text),fmt,args);va_end(args);logLines.emplace_back(text);}
 void println(const char* text){logLines.emplace_back(text);}
};
inline SerialDouble Serial;
class String {
 public:
 String()=default;String(const char* s):s_(s?s:""){}
 String(int v):s_(std::to_string(v)){}String(unsigned v):s_(std::to_string(v)){}String(unsigned long v):s_(std::to_string(v)){}
 const char* c_str()const{return s_.c_str();}
 String& operator+=(const char* s){s_+=s;return *this;}
 String& operator+=(const String& s){s_+=s.s_;return *this;}
 private:std::string s_;
};
