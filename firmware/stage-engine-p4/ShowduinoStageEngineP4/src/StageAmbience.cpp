#include "StageAmbience.h"
#include "../../../protocol/showduino_ambience_command.h"
#include "../BoardConfig.h"
#include "StageStorage.h"
#include <FS.h>
#include <string.h>
#include <stdlib.h>
#include "driver/i2s_std.h"

static StageAmbienceStatus s;
static i2s_chan_handle_t tx=nullptr;
static File f;
static uint32_t dataStart=0,dataSize=0,dataPos=0;
static uint16_t channels=2,bits=16;
static int16_t buf[512];
static size_t pendingBytes=0, pendingOffset=0;

static uint16_t rd16(File &x){uint8_t b[2]; if(x.read(b,2)!=2)return 0; return b[0]|(b[1]<<8);}
static uint32_t rd32(File &x){uint8_t b[4]; if(x.read(b,4)!=4)return 0; return (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);}
static bool wavOpen(const char *p){
  if(!stageStorageIsReady()){strncpy(s.error,"NO_SD",sizeof(s.error)-1);return false;}
  f=stageStorageFs().open(p,FILE_READ); if(!f){strncpy(s.error,"FILE_NOT_FOUND",sizeof(s.error)-1);return false;}
  char id[5]={}; f.read((uint8_t*)id,4); if(strcmp(id,"RIFF")){f.close();strncpy(s.error,"NOT_RIFF",sizeof(s.error)-1);return false;}
  rd32(f); f.read((uint8_t*)id,4); if(strcmp(id,"WAVE")){f.close();strncpy(s.error,"NOT_WAVE",sizeof(s.error)-1);return false;}
  bool fmt=false,dat=false; uint16_t format=0;
  while(f.available()>=8){
    f.read((uint8_t*)id,4); uint32_t n=rd32(f); if(n>(uint32_t)f.available()){f.close();strncpy(s.error,"TRUNCATED_WAV",sizeof(s.error)-1);return false;}
    uint32_t next=f.position()+n+(n&1);
    if(!strcmp(id,"fmt ")){if(n<16){f.close();strncpy(s.error,"UNSUPPORTED_WAV",sizeof(s.error)-1);return false;}format=rd16(f);channels=rd16(f);s.sampleRate=rd32(f);rd32(f);rd16(f);bits=rd16(f);fmt=true;}
    else if(!strcmp(id,"data")){dataStart=f.position();dataSize=n;dat=true;break;}
    f.seek(next);
  }
  if(!fmt||!dat||format!=1||bits!=16||(channels!=1&&channels!=2)||!s.sampleRate||!dataSize||dataSize%(channels*2)){f.close();strncpy(s.error,"UNSUPPORTED_WAV",sizeof(s.error)-1);return false;}
  f.seek(dataStart);dataPos=0;return true;
}
static void inventory(unsigned index,char *r,size_t z){
  if(!stageStorageIsReady()){snprintf(r,z,"ERR:AMBIENCE:NO_SD");return;}
  File dir=stageStorageFs().open(PATH_AUDIO_AMBIENCE,FILE_READ);
  if(!dir||!dir.isDirectory()){snprintf(r,z,"AMBIENCE:FILE:END");return;}
  unsigned current=0;
  for(File entry=dir.openNextFile();entry;entry=dir.openNextFile()){
    const char *name=entry.name();const char *slash=strrchr(name,'/');if(slash)name=slash+1;
    char path[96],command[128];snprintf(path,sizeof(path),"%s/%s",PATH_AUDIO_AMBIENCE,name);
    snprintf(command,sizeof(command),"AMBIENCE:PLAY:%s",path);
    bool valid=!entry.isDirectory()&&strlen(PATH_AUDIO_AMBIENCE)+1+strlen(name)<sizeof(path)&&showduinoAmbienceCommandAllowed(command);
    entry.close();
    if(valid&&current++==index){snprintf(r,z,"AMBIENCE:FILE:%u:%s",index,path+strlen(PATH_AUDIO_AMBIENCE)+1);return;}
  }
  snprintf(r,z,"AMBIENCE:FILE:END");
}
static bool hw(uint32_t rate){
  s.i2sReady=false;
  if(tx){i2s_channel_disable(tx);i2s_del_channel(tx);tx=nullptr;}
  i2s_chan_config_t cc=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO,I2S_ROLE_MASTER);
  if(i2s_new_channel(&cc,&tx,nullptr)!=ESP_OK){strncpy(s.error,"I2S_ALLOC",sizeof(s.error)-1);return false;}
  i2s_std_config_t cfg={
    .clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(rate),
    .slot_cfg=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_STEREO),
    .gpio_cfg={.mclk=I2S_GPIO_UNUSED,.bclk=(gpio_num_t)P4_AMBIENCE_I2S_BCLK,.ws=(gpio_num_t)P4_AMBIENCE_I2S_WS,.dout=(gpio_num_t)P4_AMBIENCE_I2S_DOUT,.din=I2S_GPIO_UNUSED,.invert_flags={.mclk_inv=false,.bclk_inv=false,.ws_inv=false}}
  };
  if(i2s_channel_init_std_mode(tx,&cfg)!=ESP_OK||i2s_channel_enable(tx)!=ESP_OK){i2s_del_channel(tx);tx=nullptr;strncpy(s.error,"I2S_INIT",sizeof(s.error)-1);return false;}
  s.i2sReady=true; return true;
}
bool stageAmbienceBegin(){s=StageAmbienceStatus{};strncpy(s.error,"READY",sizeof(s.error)-1);Serial.printf("[AMBIENCE] PCM5102A WS=%d BCLK=%d DOUT=%d SCK=NC\n",P4_AMBIENCE_I2S_WS,P4_AMBIENCE_I2S_BCLK,P4_AMBIENCE_I2S_DOUT);return true;}
void stageAmbienceStop(){pendingBytes=pendingOffset=0;s.playing=false;s.looping=false;s.emergencyPlayback=false;if(f)f.close();dataPos=0;if(tx)i2s_channel_disable(tx);}
// Only the internal emergency transition can bypass the playback lock.
static bool play(const char *p,bool loop,bool emergencyPlayback=false){
  if(s.emergencyBlocked&&!emergencyPlayback){strncpy(s.error,"EMERGENCY",sizeof(s.error)-1);return false;}
  stageAmbienceStop(); if(!wavOpen(p))return false; if(!hw(s.sampleRate)){f.close();return false;}
  strncpy(s.path,p,sizeof(s.path)-1);s.path[sizeof(s.path)-1]=0;s.looping=loop;s.playing=true;s.emergencyPlayback=emergencyPlayback;strncpy(s.error,"OK",sizeof(s.error)-1);return true;
}
void stageAmbienceOnEmergency(bool active){
  // Repeated assertions must not restart an already playing announcement.
  if(active==s.emergencyBlocked)return;
  s.emergencyBlocked=active;
  stageAmbienceStop();
  if(!active){
    Serial.println("[AMBIENCE] Emergency cleared; output idle, no ambience restore");
    return;
  }
  // Use the same canonical emergency asset/fallback as P4 system sounds.
  // Failure leaves the normal soundtrack stopped and Emergency still locked.
  if(play(PATH_SYSTEM_EMERGENCY_WAV,true,true)||
     play(PATH_AUDIO_SYSTEM_LIBRARY "/emergency.wav",true,true)){
    Serial.printf("[AMBIENCE] Emergency WAV looping: %s\n",s.path);
  }else{
    Serial.printf("[AMBIENCE] Emergency WAV unavailable: %s; output stopped\n",s.error);
  }
}
void stageAmbienceLoop(){
  if(!s.playing||!tx||!f)return;
  // Keep each output block until DMA accepts every byte. Never advance the
  // source or loop while a nonblocking write still has an unwritten tail.
  if(!pendingBytes){
    size_t want=channels==1?sizeof(buf)/2:sizeof(buf);
    uint32_t left=dataSize-dataPos;if(left<want)want=left;
    size_t n=f.read((uint8_t*)buf,want);
    if(n!=want){strncpy(s.error,"TRUNCATED_WAV",sizeof(s.error)-1);stageAmbienceStop();return;}
    dataPos+=n;
    if(channels==1){
      for(size_t i=n/2;i>0;--i){int16_t sample=buf[i-1];buf[2*i-2]=sample;buf[2*i-1]=sample;}
      n*=2;
    }
    const int32_t vol=s.emergencyPlayback?100:s.volume;
    for(size_t i=0;i<n/2;i++)buf[i]=(int16_t)(((int32_t)buf[i]*vol)/100);
    pendingBytes=n;pendingOffset=0;
  }
  size_t written=0;
  const auto result=i2s_channel_write(tx,(uint8_t*)buf+pendingOffset,pendingBytes,&written,0);
  if(result!=ESP_OK&&result!=ESP_ERR_TIMEOUT){strncpy(s.error,"I2S_WRITE",sizeof(s.error)-1);stageAmbienceStop();return;}
  if(written>pendingBytes){strncpy(s.error,"I2S_WRITE",sizeof(s.error)-1);stageAmbienceStop();return;}
  pendingOffset+=written;pendingBytes-=written;
  if(!pendingBytes&&dataPos>=dataSize){
    if(s.looping){f.seek(dataStart);dataPos=0;}else stageAmbienceStop();
  }
}
const StageAmbienceStatus &stageAmbienceStatus(){return s;}
bool stageAmbienceHandleCommand(const char *c,char *r,size_t z){
  if(!c||strncmp(c,"AMBIENCE:",9))return false;
  if(!strcmp(c,"AMBIENCE:STATUS")){snprintf(r,z,"AMBIENCE:STATUS:%s:VOL=%u:LOCK=%u:ERROR=%s",s.emergencyPlayback?"EMERGENCY":(s.playing?(s.looping?"LOOPING":"PLAYING"):"IDLE"),(unsigned)(s.emergencyPlayback?100:s.volume),(unsigned)s.emergencyBlocked,s.error);return true;}
  if(!strncmp(c,"AMBIENCE:FILE:",14)&&showduinoAmbienceCommandAllowed(c)){inventory((unsigned)atoi(c+14),r,z);return true;}
  // Operator commands cannot replace, stop or mute the emergency announcement.
  if(s.emergencyBlocked){snprintf(r,z,"ERR:AMBIENCE:EMERGENCY");return true;}
  if(!showduinoAmbienceCommandAllowed(c)){snprintf(r,z,"ERR:AMBIENCE:INVALID_COMMAND");return true;}
  if(!strcmp(c,"AMBIENCE:STOP")){stageAmbienceStop();snprintf(r,z,"OK:AMBIENCE:STOP");return true;}
  if(!strncmp(c,"AMBIENCE:VOLUME:",16)){char *end=nullptr;long v=strtol(c+16,&end,10);if(!c[16]||*end||v<0||v>100){snprintf(r,z,"ERR:AMBIENCE:VOLUME");return true;}s.volume=v;snprintf(r,z,"OK:AMBIENCE:VOLUME:%ld",v);return true;}
  if(!strncmp(c,"AMBIENCE:PLAY:",14)){bool ok=play(c+14,false);snprintf(r,z,ok?"OK:AMBIENCE:PLAY":"ERR:AMBIENCE:%s",ok?"":s.error);return true;}
  if(!strncmp(c,"AMBIENCE:LOOP:",14)){bool ok=play(c+14,true);snprintf(r,z,ok?"OK:AMBIENCE:LOOP":"ERR:AMBIENCE:%s",ok?"":s.error);return true;}
  snprintf(r,z,"ERR:AMBIENCE:UNKNOWN");return true;
}
