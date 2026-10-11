#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <deque>
#define HIGH 1
#define LOW 0
#define OUTPUT 1
extern uint32_t test_now;
inline uint32_t millis(){return test_now;}
inline void delay(int){}
inline void pinMode(int,int){}
inline void digitalWrite(int,int){}
inline int digitalRead(int){return HIGH;}
class Stream { public: std::deque<char> bytes; int available(){return bytes.size();} int read(){char c=bytes.front();bytes.pop_front();return c;} void print(const char*){} void print(char){} };
