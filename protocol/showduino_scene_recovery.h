#pragma once
#include <stdint.h>
#include <string.h>
inline const char *showduinoSceneLabel(const char *command) {
 if(!command || strncmp(command,"INTERNAL:LOG:",13))return nullptr;
 const char *p=strchr(command+13,':');
 return p && !strncmp(p+1,"SCENE ",6) && p[7] ? p+7 : nullptr;
}
inline uint32_t showduinoSceneFingerprint(uint32_t hash,uint32_t time,const char *command) {
 for(unsigned i=0;i<4;++i){hash^=(time>>(i*8))&255u;hash*=16777619u;}
 if(command)for(const unsigned char *p=(const unsigned char*)command;*p;++p){hash^=*p;hash*=16777619u;}
 hash^=0;return hash*16777619u;
}
inline bool showduinoParseSceneStart(const char *command,uint32_t *time,uint32_t *fingerprint,uint32_t *revision) {
 if(!command || !time || !fingerprint || !revision || strncmp(command,"SHOW:STARTAT:",13))return false;
 const char *p=command+13;if(!*p)return false;uint32_t value=0;unsigned digits=0;
 for(;*p && *p!=':';++p){if(*p<'0'||*p>'9')return false;unsigned digit=*p-'0';if(value>(UINT32_MAX-digit)/10u)return false;value=value*10u+digit;++digits;}
 if(!digits || *p++!=':' || strlen(p)<10 || p[8]!=':')return false;
 uint32_t hash=0;
 for(unsigned i=0;i<8;++i,++p){unsigned v;if(*p>='0'&&*p<='9')v=*p-'0';else if(*p>='a'&&*p<='f')v=*p-'a'+10;else if(*p>='A'&&*p<='F')v=*p-'A'+10;else return false;hash=(hash<<4)|v;}
 ++p;uint32_t rev=0;digits=0;
 for(;*p;++p){if(*p<'0'||*p>'9')return false;unsigned digit=*p-'0';if(rev>(UINT32_MAX-digit)/10u)return false;rev=rev*10u+digit;++digits;}
 if(!digits)return false;
 *time=value;*fingerprint=hash;*revision=rev;return true;
}
