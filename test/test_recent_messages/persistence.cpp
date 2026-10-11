#include <assert.h>
#include <stdio.h>
#include "fake_inbox_storage.h"
using History = RecentMessages<185>;
using Persistence = InboxPersistence<185>;
static void add(History& h, const char* text) {
  const uint8_t prefix[] = {1, 2, 3, 4};
  h.add(2, 9, 1700000000, 1700000001, prefix, "test origin", text);
}
int main() {
  // Explicit cleanup can recover a full filesystem without writing a snapshot.
  {
    FakeInboxStorage full; History h; Persistence p(&full);
    add(h,"one"); p.changed(1); assert(p.flush(h,1));
    add(h,"two"); p.changed(2); assert(p.flush(h,2));
    full.refuse=true;
    add(h,"three"); p.changed(3); assert(!p.flush(h,3));
    assert(p.clear(h,4) && !p.failed() && !p.pending());
    assert(!full.present[0] && !full.present[1]);
    History restored; Persistence reboot(&full); assert(reboot.load(restored) && !restored.count());
    full.refuse=false; add(h,"saved again"); p.changed(5); assert(p.flush(h,5));
    full.refuse_remove=true; assert(!p.clear(h,6) && p.failed());
  }
  FakeInboxStorage fs;
  History h;
  Persistence p(&fs);
  assert(p.load(h) && h.count() == 0);
  for (int i = 0; i < 40; i++) {
    char text[185]; snprintf(text, sizeof(text), "Message %d \xF0\x9F\x8C\x80\nnext line", i);
    add(h, text);
  }
  h.setFlags(2, true, true);
  uint8_t target[32] = {0xAA, 0xBB}; h.setReplyTarget(2, target);
  p.changed(100); p.tick(h, 2099); assert(fs.writes == 0);
  p.tick(h, 2100); assert(fs.writes == 1 && !p.pending());
  History restored; Persistence reboot(&fs);
  assert(reboot.load(restored) && restored.count() == 32 && restored.selected() == 0);
  for (size_t i = 0; i < 32; i++) {
    const auto *a=h.at(i), *b=restored.at(i);
    assert(!strcmp(a->text,b->text) && !strcmp(a->origin,b->origin));
    assert(a->sent_at==b->sent_at && a->received_at==b->received_at);
    assert(a->read==b->read && a->truncated==b->truncated && a->kind==b->kind && a->channel_idx==b->channel_idx);
    assert(!memcmp(a->sender_prefix,b->sender_prefix,4));
    assert(a->has_reply_target==b->has_reply_target && !memcmp(a->reply_target,b->reply_target,32));
  }
  // Erase oldest, middle, newest and empty; preserve chronological order.
  assert(h.erase(31)); assert(h.erase(4)); assert(h.erase(0)); assert(h.count()==29);
  assert(!h.erase(29));
  h.select(10);
  char selected[185]; strcpy(selected,h.at(10)->text);
  assert(h.erase(3) && h.selected()==9 && !strcmp(h.at(h.selected())->text,selected));
  assert(h.erase(27));
  assert(h.count()==27);
  p.changed(3000); assert(p.flush(h,3000));
  Persistence afterDelete(&fs); assert(afterDelete.load(restored) && restored.count()==27);
  assert(!strcmp(restored.at(0)->text,h.at(0)->text));
  h.clear(); p.changed(4000); assert(p.flush(h,4000));
  Persistence afterClear(&fs); assert(afterClear.load(restored) && restored.count()==0);
  // Read an actual WIB1-layout snapshot from the preceding firmware, then
  // upgrade without inventing a reply address for legacy messages.
  {
    FakeInboxStorage legacy_fs; History legacy_history; add(legacy_history,"legacy");
    legacy_history.markRead(0); Persistence old_writer(&legacy_fs);
    old_writer.load(legacy_history); old_writer.changed(1); assert(old_writer.flush(legacy_history,1));
    auto& bytes=legacy_fs.files[0]; bytes.resize(20+232); bytes[3]='1';
    bytes[12]=232; bytes[13]=bytes[14]=bytes[15]=0;
    uint32_t crc=0xFFFFFFFFUL;
    for(size_t i=0;i<bytes.size();i++) {
      if(i>=16 && i<20) continue;
      crc^=bytes[i]; for(int b=0;b<8;b++) crc=(crc>>1)^((crc&1)?0xEDB88320UL:0);
    }
    crc^=0xFFFFFFFFUL; for(int i=0;i<4;i++) bytes[16+i]=crc>>(i*8);
    Persistence reader(&legacy_fs); History upgraded;
    assert(reader.load(upgraded) && upgraded.count()==1 && upgraded.at(0)->read);
    assert(!strcmp(upgraded.at(0)->text,"legacy") && !upgraded.at(0)->has_reply_target);
    add(upgraded,"new"); uint8_t key[32]={9}; upgraded.setReplyTarget(0,key);
    reader.changed(2); assert(reader.flush(upgraded,2));
    Persistence restart(&legacy_fs); History final;
    assert(restart.load(final) && final.count()==2 && final.at(0)->has_reply_target);
    assert(!final.at(1)->has_reply_target && final.at(1)->read);
  }
  // Every possible incomplete write of a two-message snapshot preserves old data.
  FakeInboxStorage base; History old; add(old,"saved"); Persistence initial(&base);
  initial.load(old); initial.changed(1); assert(initial.flush(old,1));
  for (int cut=0; cut<20+2*(47+185+33); cut++) {
    FakeInboxStorage broken=base; History current; Persistence writer(&broken);
    assert(writer.load(current)); add(current,"new"); writer.changed(2); broken.limit=cut;
    assert(!writer.flush(current,2) && writer.pending() && writer.failed());
    Persistence recovery(&broken); History recovered;
    assert(recovery.load(recovered) && recovered.count()==1 && !strcmp(recovered.at(0)->text,"saved"));
  }
  // Corrupt newest data/header, unsupported format, trailing bytes: use old slot.
  FakeInboxStorage two=base; History current; Persistence writer(&two);
  assert(writer.load(current)); add(current,"new"); writer.changed(2); assert(writer.flush(current,2));
  for (size_t byte=0; byte<two.files[1].size(); byte++) {
    FakeInboxStorage damaged=two; damaged.files[1][byte]^=1;
    History recovered; Persistence recovery(&damaged);
    assert(recovery.load(recovered) && recovered.count()==1);
  }
  two.files[1].push_back(1); History recovered; Persistence trailing(&two);
  assert(trailing.load(recovered) && recovered.count()==1);
  // Both damaged slots report failure without inventing messages or formatting.
  two.files[0][0]=0; Persistence corrupt(&two);
  History empty; assert(!corrupt.load(empty) && corrupt.failed() && empty.count()==0);
  // A full/unavailable filesystem leaves RAM and last snapshot intact; retry.
  FakeInboxStorage full=base; Persistence retry(&full); History pending;
  assert(retry.load(pending)); add(pending,"pending"); full.refuse=true; retry.changed(10);
  retry.tick(pending,2010); assert(retry.failed() && retry.pending());
  full.refuse=false; retry.tick(pending,32009); assert(retry.pending());
  retry.tick(pending,32010); assert(!retry.pending() && !retry.failed());
  // Continuous traffic is saved by five seconds, and millis rollover is safe.
  FakeInboxStorage timed; History stream; Persistence batch(&timed); batch.load(stream);
  for (uint32_t t=1000;t<=6000;t+=1000) { add(stream,"traffic"); batch.changed(t); batch.tick(stream,t); }
  assert(timed.writes==1);
  batch.changed(0xFFFFFF00UL); batch.tick(stream,0x000006CFUL); assert(batch.pending());
  batch.tick(stream,0x000006D0UL); assert(!batch.pending());
  puts("Persistence tests passed: reboot, delete, clear, torn writes, corruption, full FS, batching, rollover");
}
