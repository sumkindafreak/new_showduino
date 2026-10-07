#pragma once
#include <string.h>
#include <initializer_list>
inline bool showduinoIoCommandAllowed(const char *c){
  if(!c)return false;
  if(!strcmp(c,"IO:STATUS")||!strcmp(c,"IO:ALL:OFF")||!strcmp(c,"IO:SAVE"))return true;
  if(strncmp(c,"IO:",3)||(c[3]!='1'&&c[3]!='2')||c[4]!=':')return false;
  const char *p=c+5;
  for(const char *action:{"STATUS","OFF","ON","MODE:DISABLED","MODE:INPUT","MODE:OUTPUT","ACTIVE:HIGH","ACTIVE:LOW","PULL:NONE","PULL:UP","PULL:DOWN"})if(!strcmp(p,action))return true;
  unsigned max=0,min=0;
  if(!strncmp(p,"DEBOUNCE:",9)){p+=9;max=5000;}
  else if(!strncmp(p,"PULSE:",6)){p+=6;min=1;max=3600000;}
  else return false;
  unsigned value=0,n=0;
  while(*p){if(*p<'0'||*p>'9'||++n>7)return false;value=value*10+(*p++-'0');}
  return n&&value>=min&&value<=max;
}
