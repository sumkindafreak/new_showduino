#include "DirectorCueLibrary.h"
#include "DirectorCueSummary.h"
#include "ShowduinoOsUi.h"
#include <lvgl.h>
#include <string.h>
#include <stdio.h>

static DirectorCueStateFn stateFn=nullptr;
static DirectorCueReadFn readFn=nullptr;
static DirectorCueStartFn startFn=nullptr;
static lv_obj_t *root=nullptr,*heading=nullptr,*status=nullptr,*current=nullptr,*next=nullptr;
static lv_obj_t *rows[8]={},*rowBoxes[8]={},*startButton=nullptr,*startLabel=nullptr,*modeLabel=nullptr,*cueList=nullptr;
static uint32_t rowIndex[8]={},selectedTime=UINT32_MAX,pendingSince=0;
static char selectedName[104]={},sourceName[64]={},feedback[104]={};
static uint32_t sourceCount=0;
static bool confirmStart=false,pending=false,scenesOnly=true;
static lv_timer_t *timer=nullptr;
static uint32_t firstRow=0;
static bool follow=true;
static void render();
void directorCueLibrarySetSource(DirectorCueStateFn state,DirectorCueReadFn read,DirectorCueStartFn start){stateFn=state;readFn=read;startFn=start;}
void directorCueLibraryClose(){if(root)lv_obj_delete(root);}
static void deleted(lv_event_t *){
 if(timer){lv_timer_delete(timer);timer=nullptr;}
 root=heading=status=current=next=startButton=startLabel=modeLabel=cueList=nullptr;memset(rows,0,sizeof(rows));memset(rowBoxes,0,sizeof(rowBoxes));
}
static void selectRow(lv_event_t *e){
 const unsigned slot=(unsigned)(uintptr_t)lv_event_get_user_data(e);
 if(slot>=8 || !readFn)return;
 char command[96];uint32_t time=0;
 if(!readFn(rowIndex[slot],&time,command,sizeof(command)))return;
 const char *name=showduinoSceneLabel(command);if(!name)return;
 selectedTime=time;snprintf(selectedName,sizeof(selectedName),"%s",name);
 feedback[0]=0;confirmStart=pending=false;follow=false;render();
}
static void action(lv_event_t *e){
 const intptr_t id=(intptr_t)lv_event_get_user_data(e);
 if(id==5){directorCueLibraryClose();return;}
 if(id==4){
  DirectorCueLibraryState state{};if(stateFn)stateFn(&state);
  if(!directorCueCanStart(state) || selectedTime==UINT32_MAX || pending){confirmStart=false;render();return;}
  if(!confirmStart){confirmStart=true;render();return;}
  confirmStart=false;
  pending=startFn && startFn(selectedTime);pendingSince=lv_tick_get();
  snprintf(feedback,sizeof(feedback),"%s",pending?"Restart requested - waiting for playback confirmation":"Restart request could not be sent");render();return;
 }
 confirmStart=false;
 if(id==0){follow=true;firstRow=0;if(cueList)lv_obj_scroll_to_y(cueList,0,LV_ANIM_OFF);}
 else if(id==3){scenesOnly=!scenesOnly;firstRow=0;follow=true;}
 else {follow=false;if(id==1)firstRow=firstRow>=8?firstRow-8:0;else firstRow+=8;}
 render();
}
static void tick(lv_timer_t *){render();}
static void text(lv_obj_t *label,const char *value){ShowduinoOsTheme::setTextIfChanged(label,value);}
static void render(){
 if(!root)return;
 DirectorCueLibraryState state{};if(stateFn)stateFn(&state);
 if(state.emergency){directorCueLibraryClose();return;}
 if(strcmp(sourceName,state.name) || sourceCount!=state.total){
  snprintf(sourceName,sizeof(sourceName),"%s",state.name);sourceCount=state.total;
  selectedTime=UINT32_MAX;confirmStart=pending=false;firstRow=0;feedback[0]=0;
 }
 char buffer[160];snprintf(buffer,sizeof(buffer),"CUE LIBRARY - %s",state.name[0]?state.name:"No production");text(heading,buffer);
 if(!state.matched || !readFn){
  text(status,"Load a production on this Director to browse its verified timeline.");
  text(current,"No matching local cue list");text(next,"");
  confirmStart=false;ShowduinoOsTheme::setEnabled(startButton,false);
  for(unsigned i=0;i<8;++i){text(rows[i],"");lv_obj_clear_flag(rowBoxes[i],LV_OBJ_FLAG_CLICKABLE);}return;
 }
 snprintf(buffer,sizeof(buffer),"%s | %lu / %lu timeline | %s%s",state.state,(unsigned long)state.fired,(unsigned long)state.total,follow?"FOLLOW":"BROWSE",state.online?"":" | PLAYBACK UPDATE STALE - last report");text(status,buffer);
 uint32_t visible=0,nextRow=0,nextTime=0;bool foundNext=false;
 char latest[104]="None yet",upcoming[104]="End of cue list";
 for(uint32_t i=0;i<state.total;++i){
  uint32_t time;char cmd[96],summary[104];
  if(!readFn(i,&time,cmd,sizeof(cmd)) || !directorCueSummary(cmd,summary,sizeof(summary)) || (scenesOnly && !showduinoSceneLabel(cmd)))continue;
  if(i<state.fired)snprintf(latest,sizeof(latest),"%s",summary);
  else if(!foundNext){foundNext=true;nextRow=visible;nextTime=time;snprintf(upcoming,sizeof(upcoming),"%s",summary);}
  ++visible;
 }
 snprintf(buffer,sizeof(buffer),"CURRENT / PAST: %s",latest);text(current,buffer);
 if(foundNext)snprintf(buffer,sizeof(buffer),"NEXT%s +%lus: %s",state.online?"":" (last report)",(unsigned long)((directorCueCountdown(nextTime,state.elapsedMs)+999)/1000),upcoming);
 else snprintf(buffer,sizeof(buffer),"NEXT: End of cue list");text(next,buffer);
 if(pending && !strcmp(state.state,"RUNNING")){pending=false;selectedTime=UINT32_MAX;follow=true;feedback[0]=0;}
 if(pending && lv_tick_get()-pendingSince>5000){pending=false;snprintf(feedback,sizeof(feedback),"Scene start not confirmed. Check the system status before retrying.");}
 if(!directorCueCanStart(state))confirmStart=false;
 ShowduinoOsTheme::setEnabled(startButton,directorCueCanStart(state) && selectedTime!=UINT32_MAX && !pending);
 if(scenesOnly && !visible)text(status,"No restart scenes authored. Tap ALL CUES to browse this timeline.");
 text(startLabel,pending?"REQUESTED":(confirmStart?"CONFIRM":"START SCENE"));
 text(modeLabel,scenesOnly?"ALL CUES":"SCENES");
 if(selectedTime!=UINT32_MAX){
  snprintf(buffer,sizeof(buffer),"%s %02lu:%02lu %s%s",confirmStart?"CONFIRM START:":"SELECTED:",(unsigned long)(selectedTime/60000),(unsigned long)((selectedTime/1000)%60),selectedName,directorCueCanStart(state)?"":" | Pause / clear emergency first");text(status,buffer);
 }
 if(feedback[0] && !confirmStart)text(status,feedback);
 const uint32_t previousFirst=firstRow;
 if(follow)firstRow=foundNext?(nextRow>0?nextRow-1:0):(visible>8?visible-8:0);
 else if(firstRow>=visible)firstRow=visible>8?((visible-1)/8)*8:0;
 if(follow && firstRow!=previousFirst && cueList)lv_obj_scroll_to_y(cueList,0,LV_ANIM_OFF);
 uint32_t ordinal=0,drawn=0;
 for(uint32_t i=0;i<state.total && drawn<8;++i){
  uint32_t time;char cmd[96],summary[104];
  if(!readFn(i,&time,cmd,sizeof(cmd)) || !directorCueSummary(cmd,summary,sizeof(summary)) || (scenesOnly && !showduinoSceneLabel(cmd)))continue;
  if(ordinal++<firstRow)continue;
  snprintf(buffer,sizeof(buffer),"%s %02lu:%02lu.%01lu  %s",i<state.fired?"PAST":(foundNext&&ordinal-1==nextRow?"NEXT":"    "),(unsigned long)(time/60000),(unsigned long)((time/1000)%60),(unsigned long)((time%1000)/100),summary);
  rowIndex[drawn]=i;
  if(showduinoSceneLabel(cmd)){lv_obj_add_flag(rowBoxes[drawn],LV_OBJ_FLAG_CLICKABLE);}
  else lv_obj_clear_flag(rowBoxes[drawn],LV_OBJ_FLAG_CLICKABLE);
  text(rows[drawn],buffer);
  lv_obj_set_style_text_color(rows[drawn],lv_color_hex(i<state.fired?ShowduinoPalette::Muted:(ordinal-1==nextRow?ShowduinoPalette::Accent:ShowduinoPalette::Text)),0);
  ++drawn;
 }
 while(drawn<8){lv_obj_clear_flag(rowBoxes[drawn],LV_OBJ_FLAG_CLICKABLE);text(rows[drawn++],"");}
}
void directorCueLibraryOpen(){
 if(root){render();return;}
 firstRow=0;follow=true;scenesOnly=true;selectedTime=UINT32_MAX;confirmStart=pending=false;feedback[0]=0;
 root=lv_obj_create(lv_screen_active());lv_obj_set_pos(root,16,32);lv_obj_set_size(root,768,360);
 lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE);ShowduinoOsTheme::styleRaisedCard(root,true);
 lv_obj_set_style_pad_all(root,12,0);lv_obj_add_event_cb(root,deleted,LV_EVENT_DELETE,nullptr);
 auto label=[](lv_obj_t *parent,int y,int width){auto obj=lv_label_create(parent);lv_obj_set_pos(obj,0,y);lv_obj_set_width(obj,width);lv_label_set_long_mode(obj,LV_LABEL_LONG_CLIP);lv_obj_set_style_text_color(obj,lv_color_hex(ShowduinoPalette::Text),0);return obj;};
 heading=label(root,0,736);status=label(root,25,736);current=label(root,50,736);next=label(root,75,736);
 auto list=lv_obj_create(root);cueList=list;lv_obj_set_pos(list,0,102);lv_obj_set_size(list,736,185);lv_obj_set_style_pad_all(list,4,0);lv_obj_set_scroll_dir(list,LV_DIR_VER);
 for(unsigned i=0;i<8;++i){
  rowBoxes[i]=lv_obj_create(list);lv_obj_remove_style_all(rowBoxes[i]);
  lv_obj_set_pos(rowBoxes[i],0,i*30);lv_obj_set_size(rowBoxes[i],704,30);
  lv_obj_clear_flag(rowBoxes[i],LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(rowBoxes[i],selectRow,LV_EVENT_CLICKED,(void*)(uintptr_t)i);
  rows[i]=label(rowBoxes[i],3,704);
 }
 const char *names[]={"FOLLOW","PREVIOUS","NEXT","ALL CUES","START SCENE","CLOSE"};
 for(intptr_t i=0;i<6;++i){auto btn=lv_button_create(root);lv_obj_set_pos(btn,i*123,296);lv_obj_set_size(btn,114,38);auto lab=lv_label_create(btn);lv_label_set_text(lab,names[i]);lv_obj_set_style_text_font(lab,&lv_font_montserrat_14,0);lv_obj_center(lab);lv_obj_add_event_cb(btn,action,LV_EVENT_CLICKED,(void*)i);if(i==4){startButton=btn;startLabel=lab;}if(i==3)modeLabel=lab;}
 timer=lv_timer_create(tick,250,nullptr);render();
}
