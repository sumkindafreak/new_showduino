#pragma once
#include <string.h>
#include <stddef.h>
#include "showduino_protocol_version.h"
// Operator playback stays within the dedicated SD ambience directory.
inline bool showduinoAmbienceCommandAllowed(const char *c) {
  if(!c||strlen(c)>=SHOWDUINO_DESK_COMMAND_MAX)return false;
  if(!strcmp(c,"AMBIENCE:STATUS")||!strcmp(c,"AMBIENCE:STOP"))return true;
  if(!strncmp(c,"AMBIENCE:FILE:",14)){
    const char *p=c+14;unsigned n=0;while(*p){if(*p<'0'||*p>'9'||++n>3)return false;++p;}return n>0;
  }
  if(!strncmp(c,"AMBIENCE:VOLUME:",16)){
    const char *p=c+16;unsigned v=0,n=0;
    while(*p){if(*p<'0'||*p>'9'||++n>3)return false;v=v*10+(*p++-'0');}
    return n&&v<=100;
  }
  if(strncmp(c,"AMBIENCE:PLAY:",14)&&strncmp(c,"AMBIENCE:LOOP:",14))return false;
  const char *p=c+14;
  const char *base="/showduino/audio/ambience/";
  if(strncmp(p,base,strlen(base))||strlen(p)>=96)return false;
  p+=strlen(base);size_t n=strlen(p);
  if(n<5||strcmp(p+n-4,".wav")||strstr(p,".."))return false;
  for(;*p;++p)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='_'||*p=='-'||*p=='.'||*p==' '))return false;
  return true;
}
