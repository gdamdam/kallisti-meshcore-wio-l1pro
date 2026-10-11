#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <MicroNMEA.h>
uint32_t test_now=1000;
namespace mesh { class RTCClock { public: uint32_t value=0; unsigned writes=0; void setCurrentTime(uint32_t t){value=t;++writes;} }; }
class RefCountedDigitalPin {public:void claim(){} void release(){} };
// Calendar adapter only; parser, provider and freshness logic are production code.
class DateTime {
  uint32_t value;
public:
  DateTime(int y,int m,int d,int h,int mi,int s) {
    struct tm t={};t.tm_year=y-1900;t.tm_mon=m-1;t.tm_mday=d;t.tm_hour=h;t.tm_min=mi;t.tm_sec=s;value=timegm(&t);
  }
  uint32_t unixtime(){return value;}
};
#include "location-under-test.h"
void LocationProvider::sendSentence(const char*){}
#include "provider-under-test.h"
void feed(Stream& s,const char* sentence,bool valid_checksum=true) {
  char sum[3]={};MicroNMEA::generateChecksum(sentence,sum);
  for(const char* p=sentence;*p;++p)s.bytes.push_back(*p);
  s.bytes.push_back('*');s.bytes.push_back(valid_checksum?sum[0]:'x');s.bytes.push_back(sum[1]);
  s.bytes.push_back('\r');s.bytes.push_back('\n');
}
void rmc(Stream& s,MicroNMEALocationProvider& gps,unsigned sec) {
  char sentence[100];snprintf(sentence,sizeof(sentence),"$GPRMC,1200%02u.00,A,1030.0000,N,02015.0000,W,0.0,0.0,190926,,,A",sec);
  feed(s,sentence);gps.loop();
}
int main() {
  Stream s;mesh::RTCClock clock;MicroNMEALocationProvider gps(s,&clock);gps.begin();
  rmc(s,gps,0);assert(gps.isValid() && !gps.hasAltitude() && clock.writes==0);
  test_now+=1000;rmc(s,gps,1);test_now+=1000;rmc(s,gps,2);assert(clock.writes==1);
  assert(gps.getLatitude()==10500000 && gps.getLongitude()==-20250000);
  uint32_t timestamp=clock.value;test_now+=1800001;gps.loop();
  assert(!gps.isValid() && gps.hasFix() && clock.writes==1 && clock.value==timestamp);
  feed(s,"$GPRMC,120003.00,A,1030.0000,N,02015.0000,W,0.0,0.0,190926,,,A",false);gps.loop();assert(!gps.isValid());
  feed(s,"$GPTXT,01,01,02,receiver running");gps.loop();assert(!gps.isValid());
  rmc(s,gps,4);assert(gps.isValid() && clock.writes==2);
  clock.value+=86400;assert(gps.isValid() && gps.getFixAgeMillis()==0); // wall-clock edits cannot age a fix
  feed(s,"$GPGGA,120004.00,1030.0000,N,02015.0000,W,1,12,0.8,123.0,M,0.0,M,,");gps.loop();
  assert(gps.hasAltitude() && gps.getAltitude()==123000 && gps.satellitesCount()==12);
  gps.stop();assert(!gps.hasFix() && !gps.hasAltitude());gps.begin();gps.reset();assert(!gps.isValid() && gps.satellitesCount()==0);
  // Old UART bytes cannot masquerade as a post-restart fix.
  feed(s,"$GPRMC,120005.00,A,1030.0000,N,02015.0000,W,0.0,0.0,190926,,,A");gps.reset();gps.loop();assert(!gps.hasFix());
  unsigned writes=clock.writes;
  feed(s,"$GPGGA,120005.00,1030.0000,N,02015.0000,W,1,12,0.8,,M,0.0,M,,");gps.loop();
  assert(gps.isValid() && !gps.hasAltitude() && gps.getTimestamp()==0 && clock.writes==writes);
  for(int i=0;i<4;i++){feed(s,"$GPRMC,120005.00,A,1030.0000,N,02015.0000,W,0.0,0.0,310226,,,A");gps.loop();}
  assert(gps.getTimestamp()==0 && clock.writes==writes); // invalid date cannot set the RTC
  feed(s,"$GPRMC,120005.00,V,1030.0000,N,02015.0000,W,0.0,0.0,190926,,,A");gps.loop();assert(!gps.hasFix());
  gps.syncTime();test_now=UINT32_MAX-1000;rmc(s,gps,6);test_now=1000;assert(gps.isValid() && gps.getFixAgeMillis()==2001);
  test_now=40000;assert(!gps.isValid());
  puts("GPS provider passed: serial silence, checksum/unknown frames, RTC updates, restart invalidation, UART drain, altitude validity, invalid dates, clock changes, millis wrap");
}
