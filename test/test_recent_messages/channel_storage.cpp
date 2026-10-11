#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>
#include <algorithm>
#define FILE_O_WRITE 1
#define FILE_O_READ 0
struct ChannelDetails { char name[32]={}; struct { uint8_t secret[32]={}; } channel; };
struct DataStoreHost {
  ChannelDetails channels[3];
  bool getChannelForSave(unsigned i,ChannelDetails& c) { if(i>=3)return false;c=channels[i];return true; }
};
struct FS;
struct File {
  FS* fs;std::string path;size_t pos=0;bool opened=false,writing=false;
  File(FS* f, const char* p, bool w):fs(f),path(p),writing(w){}
  operator bool() const{return opened;}
  size_t write(const uint8_t*,size_t);
  int read(void*,uint16_t);
  void close();
};
struct FS {
  std::map<std::string,std::vector<uint8_t>> files;
  int limit=-1;bool refuse_open=false,refuse_rename=false,corrupt=false;
  bool exists(const char* p){return files.count(p);}
  bool remove(const char* p){files.erase(p);return true;}
  File open(const char* p,int mode){File f{this,p,mode==FILE_O_WRITE};if(refuse_open)return f;if(mode==FILE_O_WRITE)files[p].clear();f.opened=exists(p);return f;}
  bool rename(const char* from,const char* to){if(refuse_rename)return false;files[to]=files[from];files.erase(from);return true;}
};
size_t File::write(const uint8_t* p,size_t n){if(fs->limit>=0){n=std::min(n,size_t(fs->limit));fs->limit-=n;}auto& b=fs->files[path];b.insert(b.end(),p,p+n);return n;}
int File::read(void* p,uint16_t n){auto& b=fs->files[path];size_t len=std::min(size_t(n),b.size()-pos);if(len)memcpy(p,b.data()+pos,len);pos+=len;return len;}
void File::close(){if(writing&&fs->corrupt&&!fs->files[path].empty())fs->files[path][5]^=1;opened=false;}
struct DataStore {FS fs;FS* _getContactsChannelsFS(){return &fs;}bool saveUIChannels(DataStoreHost*);};
#include "channel-storage-under-test.h"
int main(){
  DataStoreHost host;strcpy(host.channels[0].name,"Public");host.channels[0].channel.secret[0]=9;
  for(int cut=0;cut<204;cut++){
    DataStore s;s.fs.files["/channels2"]={1,2,3};s.fs.limit=cut;
    assert(!s.saveUIChannels(&host));assert(s.fs.files["/channels2"]==std::vector<uint8_t>({1,2,3}));
  }
  for(int failure=0;failure<3;failure++){
    DataStore s;s.fs.files["/channels2"]={1,2,3};s.fs.refuse_open=failure==0;s.fs.refuse_rename=failure==1;s.fs.corrupt=failure==2;
    assert(!s.saveUIChannels(&host));assert(s.fs.files["/channels2"]==std::vector<uint8_t>({1,2,3}));
  }
  DataStore s;s.fs.files["/channels2"]={1,2,3};assert(s.saveUIChannels(&host));assert(s.fs.files["/channels2"].size()==204);
  assert(!s.fs.exists("/ui_channels_tmp"));assert(!memcmp(s.fs.files["/channels2"].data()+4,"Public",6));
  puts("Channel storage passed: full disk, short writes, readback corruption and rename failures retain original channels");
}
