#include <assert.h>
#include <stdio.h>
#include <helpers/ui/UIScreen.h>
#include "../../examples/companion_radio/ui-new/JoystickTextEditor.h"
int main() {
  JoystickTextEditor e; char text[80];
  assert(!e.copyForSend(text,sizeof(text)));
  e.input(KEY_DOWN); assert(!strcmp(e.text(),"a"));
  e.input(KEY_RIGHT);e.input(KEY_UP); assert(!strcmp(e.text(),"a)"));
  e.input(KEY_LEFT);e.remove();assert(!strcmp(e.text(),")"));e.insert();assert(!strcmp(e.text()," )"));
  e.restore("HELLO @-./",99);assert(!strcmp(e.text(),"hello @-./") && e.cursor()==strlen(e.text()));
  e.open(); for(int i=0;i<100;i++){e.input(KEY_UP);e.input(KEY_RIGHT);} assert(strlen(e.text())==79 && e.cursor()==78);
  e.insert(); assert(strlen(e.text())==79); e.remove(); assert(strlen(e.text())==78);
  e.restore("draft ",0); e.input(KEY_ENTER);e.input(KEY_DOWN);assert(e.input(KEY_ENTER)==JoystickTextEditor::Preview);
  assert(e.copyForSend(text,sizeof(text)) && !strcmp(text,"draft"));
  e.open();e.input(KEY_ENTER);e.input(KEY_DOWN);assert(e.input(KEY_ENTER)==JoystickTextEditor::Empty);
  e.input(KEY_DOWN);e.input(KEY_DOWN);e.input(KEY_DOWN);assert(e.input(KEY_ENTER)==JoystickTextEditor::Discard);
  EditorDirectionInput input;
  auto v=input.update(true,true,false,true,100); assert(v.direction==1 && v.suppress_up);
  assert(!input.update(true,true,false,true,699).direction);
  assert(input.update(true,true,false,true,700).direction==1);
  assert(!input.update(true,true,false,true,849).direction);
  assert(input.update(true,true,false,true,850).direction==1);
  assert(input.update(true,true,false,true,1900).direction==1);
  assert(input.update(true,true,false,true,1960).direction==1);
  assert(!input.update(false,true,false,true,2000).direction);
  assert(input.update(false,false,false,true,2100).suppress_up);
  assert(!input.update(false,false,false,true,2200).suppress_up);
  assert(input.update(true,false,true,false,2300).direction==-1); // wake event consumed by UI
  assert(!input.update(true,false,true,true,5000).direction); // same hold cannot edit after wake
  input.update(true,false,false,true,5100);
  assert(input.update(true,true,false,true,0xFFFFFF00UL).direction==1);
  assert(input.update(true,true,false,true,0x158).direction==1);
  puts("Editor tests passed: lowercase charset, bounds, cursor/insert/delete, preview/discard, accelerated hold, release suppression, wake, rollover");
}
