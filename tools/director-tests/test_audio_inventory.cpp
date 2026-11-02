#include <cassert>
#include <cstdio>
#include <initializer_list>
#include "../../firmware/director-esp32-8048s050/ShowduinoDirector8048S050/DirectorAudioNodeControl.h"
int main() {
 DirectorAudioNodeControl m;
 director_audio_node_clear(&m);
 char names[SHOWDUINO_AUDIO_INV_WIRE_MAX][SHOWDUINO_AUDIO_INV_NAME_MAX+1] = {};
 auto receive=[&](unsigned page,unsigned total,const char *name){
  snprintf(names[0],sizeof(names[0]),"%s",name);
  return director_audio_inventory_accept(&m,page,total,names,name[0]?1:0);
 };
 for(unsigned total : {7u,48u}) {
  assert(director_audio_inventory_begin(&m,0)==0);
  for(unsigned i=0;i<total;++i){
   char name[32];snprintf(name,sizeof(name),"folder/file%u.wav",i);
   assert(receive(i,total,name)==(i+1<total?(int)i+1:-1));
   assert(!strcmp(m.inventory[i],name));
   assert(receive(i,total,"duplicate.wav")==-1);
  }
  assert(!m.inventoryLoading && m.inventoryCount==total);
 }
 director_audio_inventory_begin(&m,0);
 assert(receive(1,48,"out-of-order.wav")==-1);
 assert(!m.inventoryCount);
 assert(receive(0,0,"")==-1);
 assert(!m.inventoryLoading && !m.inventoryCount);
 director_audio_inventory_begin(&m,0);
 for(unsigned i=0;i<48;++i) receive(i,60,"bounded.wav");
 assert(!m.inventoryLoading && m.inventoryCount==48);
 assert(receive(48,60,"overflow.wav")==-1);
 director_audio_inventory_begin(&m,0);
 m.inventoryLoading=false;
 assert(receive(0,7,"late.wav")==-1 && !m.inventoryCount);
 puts("Director full-library inventory tests passed");
}
