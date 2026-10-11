#include <assert.h>
#include <string.h>
#include <limits>
#include "../../examples/companion_radio/ui-new/QuickReplies.h"
int main() {
  char text[80]; uint32_t fix;
  assert(!strcmp(QuickReplies::gpsProblem(false,false,0), "GPS is off"));
  assert(!strcmp(QuickReplies::gpsProblem(true,false,0), "GPS: waiting for fix"));
  assert(!strcmp(QuickReplies::gpsProblem(true,true,30001), "GPS: fix is stale"));
  assert(!QuickReplies::gpsProblem(true,true,30000));
  for (unsigned i=0;i<7;i++) {
    assert(QuickReplies::build(i,0,0,false,false,0,0,0,0,text,sizeof(text),fix));
    assert(!strcmp(text,QuickReplies::label(i)) && fix==0);
  }
  assert(!strcmp(QuickReplies::label(2),"STATS"));
  assert(!QuickReplies::label(10));
  assert(!QuickReplies::build(9,0,0,false,false,0,0,0,0,text,sizeof(text),fix));
  assert(!QuickReplies::build(7,0,0,false,false,0,0,0,0,text,sizeof(text),fix));
  assert(QuickReplies::build(7,10.5,-20.25,false,false,0,0,0,0,text,sizeof(text),fix));
  assert(!strcmp(text,"Saved position: 10.500000, -20.250000") && fix==0);
  for(int mode=0;mode<4;mode++) {
    assert(!QuickReplies::build(8,10.5,-20.25,mode!=0,mode!=1,10500000,-20250000,
           mode==2?30001:mode==3?UINT32_MAX:0,50000,text,sizeof(text),fix));
  }
  assert(QuickReplies::build(8,0,0,true,true,0,0,30000,50000,text,sizeof(text),fix));
  assert(!strcmp(text,"GPS position: 0.000000, 0.000000"));
  assert(QuickReplies::previewFresh(fix,50000) && !QuickReplies::previewFresh(fix,50001));
  // Receipt and confirmation straddling millis wrap; no wall-clock dependency.
  fix=QuickReplies::fixToken(0xFFFFFF00UL,0);
  assert(QuickReplies::previewFresh(fix,0x158) && !QuickReplies::previewFresh(fix,uint32_t(0xFFFFFF00UL+30001U)));
  assert(QuickReplies::previewFresh(QuickReplies::fixToken(0,0),0));
  assert(QuickReplies::previewFresh(QuickReplies::fixToken(UINT32_MAX,0),0));
  assert(!QuickReplies::previewFresh(0,0));
  assert(!QuickReplies::coordinates(text,sizeof(text),"GPS",91,0));
  assert(!QuickReplies::coordinates(text,sizeof(text),"GPS",0,-181));
  assert(!QuickReplies::coordinates(text,sizeof(text),"GPS",std::numeric_limits<double>::quiet_NaN(),0));
  assert(!QuickReplies::coordinates(text,5,"GPS",1,2));
  puts("Quick reply tests passed: presets, GPS age, preview expiration, millis wrap, coordinate bounds");
}
