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
  std::string filename;
  std::shared_ptr<std::map<std::string,std::vector<uint8_t>>> entries;
  size_t nextEntry=0;
public:
  File()=default;
  explicit File(const std::vector<uint8_t>& data):bytes(std::make_shared<std::vector<uint8_t>>(data)){}
  explicit File(const std::map<std::string,std::vector<uint8_t>>& data):entries(std::make_shared<std::map<std::string,std::vector<uint8_t>>>(data)){}
  explicit operator bool() const{return bool(bytes)||bool(entries);}
  bool isDirectory()const{return bool(entries);}
  const char *name()const{return filename.c_str();}
  File openNextFile(){if(!entries||nextEntry>=entries->size())return File();auto it=entries->begin();std::advance(it,nextEntry++);File result(it->second);result.filename=it->first;return result;}
  size_t read(uint8_t* out,size_t n){if(!bytes)return 0; n=std::min(n,bytes->size()-cursor); std::memcpy(out,bytes->data()+cursor,n);cursor+=n;return n;}
  size_t available() const{return bytes?bytes->size()-cursor:0;}
  size_t position() const{return cursor;}
  bool seek(size_t p){if(!bytes||p>bytes->size())return false;cursor=p;return true;}
  void close(){bytes.reset();entries.reset();cursor=0;}
};
namespace fs {
struct FS {
 std::map<std::string,std::vector<uint8_t>> files;
 File open(const char *p,const char *){auto it=files.find(p);if(it!=files.end())return File(it->second);
 std::map<std::string,std::vector<uint8_t>> directory;std::string prefix=std::string(p)+"/";
 for(auto &entry:files)if(entry.first.rfind(prefix,0)==0&&entry.first.find('/',prefix.size())==std::string::npos)directory.insert(entry);
 return directory.empty()?File():File(directory);
 }
};
}
