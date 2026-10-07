#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
#define FILE_READ "r"
// In-memory SD card: each open handle owns an independent read cursor.
class File {
  std::shared_ptr<std::vector<uint8_t>> bytes;
  size_t cursor=0;
public:
  File()=default;
  explicit File(const std::vector<uint8_t>& data):bytes(std::make_shared<std::vector<uint8_t>>(data)){}
  explicit operator bool() const{return bool(bytes);}
  size_t read(uint8_t* out,size_t n){if(!bytes)return 0; n=std::min(n,bytes->size()-cursor); std::memcpy(out,bytes->data()+cursor,n);cursor+=n;return n;}
  size_t available() const{return bytes?bytes->size()-cursor:0;}
  size_t position() const{return cursor;}
  bool seek(size_t p){if(!bytes||p>bytes->size())return false;cursor=p;return true;}
  void close(){bytes.reset();cursor=0;}
};
namespace fs {
struct FS {
 std::map<std::string,std::vector<uint8_t>> files;
 File open(const char *p,const char *){auto it=files.find(p);return it==files.end()?File():File(it->second);}
};
}
