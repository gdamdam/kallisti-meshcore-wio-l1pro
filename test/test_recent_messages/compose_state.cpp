#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "send-target-under-test.h"
#include "fake_inbox_storage.h"
#include "../../examples/companion_radio/ui-new/UIComposeState.h"
#include "../../examples/companion_radio/ui-new/Outbox.h"
int main() {
  {
    FakeInboxStorage favorites,drafts,settings; UIComposeState state(&favorites,&drafts,&settings);
    assert(state.autoReadChannels()); assert(state.toggleAutoRead(1) && !state.autoReadChannels());
    UIComposeState reboot(&favorites,&drafts,&settings); assert(!reboot.autoReadChannels());
    assert(reboot.clearDraft(2) && !reboot.autoReadChannels());
  }
  FakeInboxStorage favorites, drafts, sends;
  UIComposeState state(&favorites,&drafts);
  UISendTarget target; target.key[31]=7; strcpy(target.name,"Friend");
  assert(!state.favorite(target)); assert(state.toggle(target,1)); assert(state.favorite(target));
  state.save(target,"hello there",5,10); assert(state.hasDraft() && state.draftStatus());
  state.tick(2010); assert(!state.draftStatus());
  UIComposeState reboot(&favorites,&drafts); UISendTarget restored; char text[80]; unsigned cursor;
  assert(reboot.favorite(target) && reboot.restore(restored,text,sizeof(text),cursor));
  assert(!strcmp(text,"hello there") && cursor==5 && !memcmp(restored.key,target.key,32));
  assert(reboot.toggle(target,2020) && !reboot.favorite(target));
  target.favorite=true; assert(reboot.favorite(target)); // app favorite overrides stale local unstar
  target.favorite=false;
  UISendTarget other=target; other.key[31]=8;
  assert(reboot.discard(other,2030) && reboot.hasDraft());
  assert(reboot.discard(target,2040) && !reboot.hasDraft());
  UIComposeState cleared(&favorites,&drafts); assert(!cleared.hasDraft());
  for(unsigned i=0;i<32;i++){target.key[31]=20+i; assert(cleared.toggle(target,2050+i));}
  target.key[31]=100; assert(!cleared.toggle(target,2100)); // no silent eviction at capacity
  // Removing a favorite makes room; old false records are reclaimed on load.
  target.key[31]=20; assert(cleared.toggle(target,2101));
  target.key[31]=100; assert(cleared.toggle(target,2102));
  { FakeInboxStorage f,d; UIComposeState state(&f,&d); UISendTarget t;
    for(unsigned i=0;i<40;i++){t.key[0]=i;assert(state.toggle(t,i*2));assert(state.toggle(t,i*2+1));assert(!state.favorite(t));}
    RecentMessages<2,32> legacy; InboxPersistence<2,32> disk(&f);
    for(unsigned i=0;i<32;i++){t.key[0]=i;legacy.add(0,0,0,0,nullptr,"old","");legacy.setReplyTarget(0,t.key);}
    disk.changed(0);assert(disk.flush(legacy,0)); UIComposeState migrated(&f,&d);
    t.key[0]=99;assert(migrated.toggle(t,1));assert(migrated.favorite(t));
  }
  Outbox box(&sends,0); uint8_t key[32]={9}, wrong[32]={8};
  box.add(0,key,"Friend","hello",true,42,1000,1700000000,100);
  assert(box.history().at(0)->channel_idx==Outbox::Awaiting);
  assert(!box.acknowledge(42,wrong,200)); assert(!box.acknowledge(41,key,200));
  assert(box.acknowledge(42,key,200)); assert(!box.acknowledge(42,key,210));
  assert(box.flush(300)); Outbox restarted(&sends,0);
  assert(restarted.history().at(0)->channel_idx==Outbox::Acknowledged);
  box.add(0,key,"Friend","timeout",true,43,1000,1700000001,100);
  box.tick(1099); assert(box.history().at(0)->channel_idx==Outbox::Awaiting);
  box.tick(1100); assert(box.history().at(0)->channel_idx==Outbox::Unconfirmed);
  assert(box.acknowledge(43,key,1200)); // late receipt can still confirm
  box.add(0,key,"Friend","pending reboot",true,44,1000,1700000002,2000); box.flush(2000);
  Outbox pending(&sends,0); assert(pending.history().at(0)->channel_idx==Outbox::Unconfirmed);
  pending.add(1,key,"Channel","OK",true,0,0,1700000003,0);
  assert(pending.history().at(0)->channel_idx==Outbox::ChannelSent);
  assert(pending.acknowledge(44,key,1));
  assert(pending.history().at(0)->channel_idx==Outbox::ChannelSent);
  pending.add(0,key,"Friend","failed",false,0,0,1700000004,0);
  assert(pending.history().at(0)->channel_idx==Outbox::Failed);
  for(int i=0;i<20;i++) pending.add(0,key,"Friend","bounded",false,0,0,1700000005,0);
  assert(pending.history().count()==16);
  pending.add(0,key,"Friend","wrap",true,45,1000,1700000006,0xFFFFFF00UL);
  pending.tick(0x2E7); assert(pending.history().at(0)->channel_idx==Outbox::Awaiting);
  pending.tick(0x2E8); assert(pending.history().at(0)->channel_idx==Outbox::Unconfirmed);
  puts("Compose state tests passed: favorites/draft restart, limits, recipient binding, ACK matching, timeout/reboot, bounded outbox");
}
