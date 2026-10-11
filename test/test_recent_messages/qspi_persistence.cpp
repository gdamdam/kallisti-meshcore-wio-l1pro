#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <stdlib.h>
extern "C" void* pvPortMalloc(size_t n){return malloc(n);}
extern "C" void vPortFree(void* p){free(p);}
extern "C" {
#include "lfs.h"
}
#include "../../examples/companion_radio/ui-new/InboxPersistence.h"
struct Flash {
  std::vector<uint8_t> bytes; unsigned rejected=0;
  Flash():bytes(2*1024*1024,255){}
};
static int readFlash(const lfs_config* c,lfs_block_t b,lfs_off_t o,void* p,lfs_size_t n){
  auto* f=static_cast<Flash*>(c->context);
  if(uintptr_t(p)%4){f->rejected++;return LFS_ERR_IO;}
  memcpy(p,f->bytes.data()+b*c->block_size+o,n);return 0;
}
static int writeFlash(const lfs_config* c,lfs_block_t b,lfs_off_t o,const void* p,lfs_size_t n){
  auto* f=static_cast<Flash*>(c->context);
  if(uintptr_t(p)%4){f->rejected++;return LFS_ERR_IO;}
  const auto* src=static_cast<const uint8_t*>(p);
  for(size_t i=0;i<n;i++)f->bytes[b*c->block_size+o+i]&=src[i];return 0;
}
static int eraseFlash(const lfs_config* c,lfs_block_t b){auto* f=static_cast<Flash*>(c->context);memset(f->bytes.data()+b*c->block_size,255,c->block_size);return 0;}
static int syncFlash(const lfs_config*){return 0;}
#define FILE_O_WRITE (LFS_O_WRONLY|LFS_O_CREAT)
#define FILE_O_READ LFS_O_RDONLY
struct FS {
  Flash flash;lfs_t lfs={};lfs_config cfg={};
  FS(){cfg.context=&flash;cfg.read=readFlash;cfg.prog=writeFlash;cfg.erase=eraseFlash;cfg.sync=syncFlash;cfg.read_size=cfg.prog_size=256;cfg.block_size=4096;cfg.block_count=512;cfg.lookahead=32;assert(!lfs_format(&lfs,&cfg));assert(!lfs_mount(&lfs,&cfg));}
  ~FS(){lfs_unmount(&lfs);}
  bool exists(const char* p){lfs_info info;return lfs_stat(&lfs,p,&info)==0;}
  bool remove(const char* p){return lfs_remove(&lfs,p)==0;}
};
using FILESYSTEM=FS;
struct File {
  FS& fs;lfs_file_t f={};bool opened=false;
  explicit File(FS& s):fs(s){}
  bool open(const char* p,int mode){opened=lfs_file_open(&fs.lfs,&f,p,mode)==0;return opened;}
  void close(){if(opened)lfs_file_close(&fs.lfs,&f);opened=false;}
  int read(uint8_t* p,size_t n){return lfs_file_read(&fs.lfs,&f,p,n);}
  size_t write(const uint8_t* p,size_t n){auto r=lfs_file_write(&fs.lfs,&f,p,n);return r>0?size_t(r):0;}
};
#include "wio-storage-under-test.h"
// The former unchunked adapter, used to reproduce the hardware alignment fault.
class OldStorage:public InboxStorage {
 FS& fs;File f;
 const char* path(unsigned i){return i?"/old1":"/old0";}
public:
 explicit OldStorage(FS& s):fs(s),f(s){}
 bool exists(unsigned i)override{return fs.exists(path(i));}
 bool open(unsigned i,bool write)override{f.close();if(write&&exists(i))fs.remove(path(i));return f.open(path(i),write?FILE_O_WRITE:FILE_O_READ);}
 size_t read(uint8_t* p,size_t n)override{int r=f.read(p,n);return r>0?size_t(r):0;}
 size_t write(const uint8_t* p,size_t n)override{return f.write(p,n);}
 void close()override{f.close();}
 bool remove(unsigned i)override{return !exists(i)||fs.remove(path(i));}
};
int main(){
 using History=RecentMessages<185>;using Disk=InboxPersistence<185>;
 {FS fs;OldStorage adapter(fs);History h;Disk disk(&adapter);assert(disk.load(h));bool failed=false;
  for(int i=0;i<32;i++){h.add(1,0,1700000000,1700000001,nullptr,"channel","message");disk.changed(i);if(!disk.flush(h,i)){failed=true;printf("Old adapter failed at %d messages; rejected DMA buffers: %u\n",i+1,fs.flash.rejected);break;}}
  assert(failed&&fs.flash.rejected);
 }
 {using History=RecentMessages<185,72>;using Disk=InboxPersistence<185,72>;FS fs;WioInboxStorage adapter(fs);History h;Disk disk(&adapter);assert(disk.load(h));
  for(int i=0;i<64;i++){char text[185];memset(text,'x',184);text[184]=0;h.add(i%3,0,1700000000,1700000001,nullptr,"channel",text);disk.changed(i);assert(disk.flush(h,i));}
  History restored;Disk reboot(&adapter);assert(reboot.load(restored)&&restored.count()==64&&!fs.flash.rejected);
  assert(disk.clear(h,81));puts("Chunked Wio adapter: 64 saves, 64-message reboot and cleanup passed with QSPI alignment enforced");
 }
}
