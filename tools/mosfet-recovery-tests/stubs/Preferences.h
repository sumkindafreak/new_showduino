#pragma once
#include <Arduino.h>
inline std::map<std::string,std::vector<uint8_t>> flashRecords;
inline bool failStoreBegin=false,failStoreWrite=false;
inline unsigned storeWrites=0;
class Preferences {
 public:
 bool begin(const char* ns,bool){ns_=ns;return !failStoreBegin;}
 void end(){}
 size_t getBytesLength(const char* key){return flashRecords[ns_+":"+key].size();}
 size_t getBytes(const char* key,void* out,size_t cap){const auto& v=flashRecords[ns_+":"+key];if(v.size()>cap)return 0;std::memcpy(out,v.data(),v.size());return v.size();}
 size_t putBytes(const char* key,const void* in,size_t size){++storeWrites;if(failStoreWrite)return 0;const auto* p=static_cast<const uint8_t*>(in);flashRecords[ns_+":"+key]=std::vector<uint8_t>(p,p+size);return size;}
 private:std::string ns_;
};
