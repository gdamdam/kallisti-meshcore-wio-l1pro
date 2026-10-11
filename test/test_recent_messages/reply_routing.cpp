#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define PUB_KEY_SIZE 32
#define UI_RECENT_INBOX 1
#define PUSH_CODE_SEND_CONFIRMED 82
#define MAX_TEXT_LEN 160
#define MSG_SEND_FAILED 0
#define EXPECTED_ACK_TABLE_SIZE 2
#define MAX_GROUP_CHANNELS 4
#define MAX_ANON_CONTACTS 8
#include "send-target-under-test.h"
struct UIMessageMetadata { enum {Contact,Channel,Signed}; };
struct ContactInfo { uint8_t flags = 0; struct { uint8_t pub_key[32] = {}; } id; char name[32] = "Contact"; };
struct ChannelDetails { struct { uint8_t secret[32]; } channel; char name[32]; };
namespace mesh { struct Utils {
  // Deterministic stand-in for the unchanged upstream SHA-256 implementation.
  static void sha256(uint8_t* out,size_t n,const uint8_t* in,int len) {
    for(size_t i=0;i<n;i++) out[i]=in[i%len]^0xA5;
  }
}; }
class MyMesh {
public:
  ContactInfo contact;
  uint8_t out_frame[16] = {};
  struct Serial { void writeFrame(const uint8_t*,int) {} } serial;
  Serial* _serial=&serial;
  struct UI { unsigned calls=0; uint32_t ack=0; uint8_t key[32]={};
    void onUIMessageAck(uint32_t value,const uint8_t* k){calls++;ack=value;memcpy(key,k,32);}
  } ui;
  UI* _ui=&ui;
  ContactInfo* checkConnectionsAck(const uint8_t*) { return nullptr; }
  ContactInfo* processAck(const uint8_t*);
  ChannelDetails channel = {};
  bool contact_exists=true,channel_exists=true,success=true;
  unsigned sent=0,full_key_lookups=0;
  char text[200] = {};
  struct { char node_name[32] = "Me"; } _prefs;
  struct Clock { uint32_t getCurrentTimeUnique() {return 1700000001;} uint32_t getMillis(){return 123;} } clock;
  Clock* _ms=&clock;
  struct Ack {uint32_t msg_sent=0,ack=0;ContactInfo* contact=nullptr;} expected_ack_table[2];
  unsigned next_ack_idx=0;
  Clock* getRTCClock() { return &clock; }
  ContactInfo* lookupContactByPubKey(const uint8_t* key,int n) {
    assert(n==32);full_key_lookups++;return contact_exists && !memcmp(key,contact.id.pub_key,32)?&contact:nullptr;
  }
  unsigned getNumContacts() const { return contact_exists ? 1 : 0; }
  bool getContactByIdx(unsigned i,ContactInfo& c) { assert(i >= MAX_ANON_CONTACTS); if (i != MAX_ANON_CONTACTS || !contact_exists) return false; c=contact; return true; }
  bool getChannel(int i,ChannelDetails& c) {if(!channel_exists||i!=3)return false;c=channel;return true;}
  int sendMessage(const ContactInfo&,uint32_t timestamp,uint8_t attempt,const char* s,uint32_t& ack,uint32_t& timeout) {
    assert(timestamp==1700000001 && attempt==0);sent++;strcpy(text,s);ack=42;timeout=100;return success?1:0;
  }
  bool sendGroupMessage(uint32_t timestamp,decltype(ChannelDetails::channel)&,const char* sender,const char* s,int n) {
    assert(timestamp==1700000001 && !strcmp(sender,"Me") && n==int(strlen(s)));sent++;strcpy(text,s);return success;
  }
  bool inboxReplyTarget(uint8_t,uint8_t,const uint8_t*,char*,size_t);
  bool sendInboxReply(uint8_t,uint8_t,const uint8_t*,const char*,uint32_t* = nullptr,uint32_t* = nullptr);
  bool getUISendTarget(bool,unsigned,UISendTarget&);
  const char* joinUIChannel(const char*);
  bool removeUIChannel(const UISendTarget&);
  unsigned channel_saves=0;
  struct Store { bool success=true; unsigned calls=0; bool saveUIChannels(MyMesh* m) { calls++; if(!success)return false; m->channel_saves++;return true;} } store;
  Store* _store=&store;
  bool setChannel(int i,const ChannelDetails& c) { if(i!=3)return false; channel=c;return true; }
  void saveChannels() { ++channel_saves; }
};
#include "reply-routing-under-test.h"
int main(){
  MyMesh m;char name[40];uint8_t key[32]={};key[31]=9;m.contact.id.pub_key[31]=9;
  assert(m.inboxReplyTarget(0,255,key,name,sizeof(name)));
  assert(m.sendInboxReply(0,255,key,"STATS") && !strcmp(m.text,"STATS"));
  assert(m.expected_ack_table[0].ack==42 && m.expected_ack_table[0].contact==&m.contact);
  uint32_t ack=42;
  assert(m.processAck(reinterpret_cast<uint8_t*>(&ack))==&m.contact);
  assert(m.ui.calls==1 && m.ui.ack==42 && !memcmp(m.ui.key,key,32));
  assert(m.processAck(reinterpret_cast<uint8_t*>(&ack))==nullptr && m.ui.calls==1);
  UISendTarget target;m.contact.flags=1;
  assert(m.getUISendTarget(false,0,target) && target.favorite && !memcmp(target.key,key,32));
  assert(!m.getUISendTarget(false,1,target));
  strcpy(m.channel.name,"Public");
  assert(m.joinUIChannel("Public") && m.channel_saves==0); // same name with another key never overwritten
  m.channel.name[0]=0;
  m.store.success=false; assert(m.joinUIChannel("Public") && !m.channel.name[0]); m.store.success=true;
  assert(!m.joinUIChannel("Public") && m.channel_saves==1);
  assert(!m.joinUIChannel("Public") && m.channel_saves==1); // idempotent
  assert(m.getUISendTarget(true,0,target));
  UISendTarget stale=target; stale.key[0]^=1; assert(!m.removeUIChannel(stale));
  assert(m.removeUIChannel(target) && !m.channel.name[0]);
  assert(!m.joinUIChannel("#field")); assert(!strcmp(m.channel.name,"#field"));
  assert(m.joinUIChannel("#Bad name")); assert(m.joinUIChannel("#"));
  m.channel.name[0]=0;
  key[31]=8;assert(!m.sendInboxReply(0,255,key,"OK") && m.sent==1);key[31]=9;
  m.contact_exists=false;assert(!m.sendInboxReply(0,255,key,"OK"));m.contact_exists=true;
  assert(!m.sendInboxReply(2,255,key,"OK"));assert(!m.sendInboxReply(0,255,nullptr,"OK"));
  assert(!m.sendInboxReply(0,255,key,""));
  char long_text[162];memset(long_text,'X',161);long_text[161]=0;
  assert(!m.sendInboxReply(0,255,key,long_text));
  strcpy(m.channel.name,"Field");m.channel.channel.secret[0]=1;
  mesh::Utils::sha256(key,32,m.channel.channel.secret,32);
  assert(m.getUISendTarget(true,0,target) && target.channel_idx==3 && !target.favorite && !memcmp(target.key,key,32));
  assert(!m.getUISendTarget(true,1,target));
  assert(m.sendInboxReply(1,3,key,"OK") && m.sent==2);
  m.channel.channel.secret[31]=1;assert(!m.sendInboxReply(1,3,key,"NO") && m.sent==2);
  m.channel.channel.secret[31]=0;m.channel.name[0]=0;assert(!m.sendInboxReply(1,3,key,"NO"));
  strcpy(m.channel.name,"Renamed");assert(m.inboxReplyTarget(1,3,key,name,sizeof(name)) && strstr(name,"Renamed"));
  m.success=false;assert(!m.sendInboxReply(1,3,key,"NO"));
  assert(!m.sendInboxReply(1,4,key,"NO"));
  puts("Actual MyMesh reply methods passed: full keys, removed/changed destinations, signed-message refusal, transport failures and ACK registration");
}
