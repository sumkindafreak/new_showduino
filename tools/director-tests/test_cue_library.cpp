#include <cassert>
#include <cstdio>
#include <cstring>
#include "../../firmware/director-esp32-8048s050/ShowduinoDirector8048S050/DirectorCueSummary.h"
#include "../../firmware/director-esp32-8048s050/ShowduinoDirector8048S050/DirectorCueLibrary.h"
int main(){
 char text[104];
 assert(directorCueSummary("AUDIO:NODE:PLAY:dinosaur.wav",text,sizeof(text))&&!strcmp(text,"Audio play: dinosaur.wav"));
 assert(directorCueSummary("AMBIENCE:LOOP:/showduino/audio/ambience/machine.wav",text,sizeof(text))&&!strcmp(text,"P4 ambience loop: machine.wav"));
 assert(directorCueSummary("INTERNAL:LOG:scene:SCENE Dinosaurs",text,sizeof(text)));
 assert(!directorCueSummary("PIXEL:SEGMENT:0:COLOR:0:255:0",text,sizeof(text)));
 assert(directorCueSummary("PIXEL:SEGMENT:0:START",text,sizeof(text)));
 assert(directorCueCountdown(1000,300)==700&&directorCueCountdown(1000,1100)==0);
 DirectorCueLibraryState state{};state.matched=state.online=true;strcpy(state.state,"RUNNING");assert(!directorCueCanStart(state));
 strcpy(state.state,"PAUSED");assert(directorCueCanStart(state));state.emergency=true;assert(!directorCueCanStart(state));state.emergency=false;
 state.online=false;assert(!directorCueCanStart(state));state.online=true;state.matched=false;assert(!directorCueCanStart(state));state.matched=true;
 strcpy(state.state,"EMERGENCY_STOP");assert(!directorCueCanStart(state));strcpy(state.state,"ERROR");assert(!directorCueCanStart(state));
 puts("Director cue summaries, countdown and scene recovery gating tests passed");
}
