#pragma once
#include <Arduino.h>
#include <mutex>
#include <condition_variable>
inline std::map<std::string,std::vector<uint8_t>> flashRecords;
inline bool failStoreBegin=false,failStoreWrite=false;
inline unsigned storeWrites=0;
inline std::mutex flashTestMutex;
inline std::condition_variable flashTestCv;
inline bool pauseFlashWrite=false,flashWriteEntered=false,releaseFlashWrite=false;
class Preferences {
 public:
 bool begin(const char* ns,bool){ns_=ns;return !failStoreBegin;}
 void end(){}
 size_t getBytesLength(const char* key){return flashRecords[ns_+":"+key].size();}
 size_t getBytes(const char* key,void* out,size_t cap){const auto& v=flashRecords[ns_+":"+key];if(v.size()>cap)return 0;std::memcpy(out,v.data(),v.size());return v.size();}
 size_t putBytes(const char* key,const void* in,size_t size){
  {std::unique_lock<std::mutex> lock(flashTestMutex);if(pauseFlashWrite){pauseFlashWrite=false;flashWriteEntered=true;flashTestCv.notify_all();flashTestCv.wait(lock,[]{return releaseFlashWrite;});}}
  ++storeWrites;if(failStoreWrite)return 0;const auto* p=static_cast<const uint8_t*>(in);flashRecords[ns_+":"+key]=std::vector<uint8_t>(p,p+size);return size;
 }
 private:std::string ns_;
};
