#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../../protocol/showduino_scene_recovery.h"
inline bool directorCueSummary(const char *command, char *out, size_t size) {
  if (!command || !out || !size) return false;
  const char *p = nullptr;
  const char *label = nullptr;
  if (!strncmp(command,"INTERNAL:LOG:",13)) {
    p=strchr(command+13,':'); if(!p || !p[1]) return false;
    snprintf(out,size,"%s",p+1);return true;
  }
  if (!strncmp(command,"AUDIO:NODE:PLAY:",16)) {p=command+16;label="Audio play";}
  else if (!strncmp(command,"AUDIO:NODE:LOOP:",16)) {p=command+16;label="Audio loop";}
  else if (!strncmp(command,"AMBIENCE:PLAY:",14)) {p=command+14;label="P4 ambience play";}
  else if (!strncmp(command,"AMBIENCE:LOOP:",14)) {p=command+14;label="P4 ambience loop";}
  if (label) { const char *leaf=strrchr(p,'/');snprintf(out,size,"%s: %s",label,leaf?leaf+1:p);return true; }
  if (!strcmp(command,"AUDIO:NODE:STOP")) {snprintf(out,size,"Audio stop");return true;}
  if (!strcmp(command,"AMBIENCE:STOP")) {snprintf(out,size,"P4 ambience stop");return true;}
  if (!strncmp(command,"LAMP:",5) || !strncmp(command,"MOSFET:NODE:",12)) {
    snprintf(out,size,"%s",command);return true;
  }
  if (!strncmp(command,"PIXEL:",6) || !strncmp(command,"AUDIO:NODE:PIXEL:",17) || !strncmp(command,"ESTOP:NODE:PIXEL:",17)) {
    if (!strstr(command,":START") && !strstr(command,":STOP")) return false;
    snprintf(out,size,"%s",command);return true;
  }
  return false; // Hide setup packets such as range, colour and volume.
}
inline uint32_t directorCueCountdown(uint32_t time, uint32_t elapsed) {
 return time > elapsed ? time-elapsed : 0;
}
