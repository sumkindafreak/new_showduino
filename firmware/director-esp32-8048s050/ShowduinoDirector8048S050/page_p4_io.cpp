#include "page_p4_io.h"
#include "ShowduinoOsUi.h"
#include <stdio.h>
#include <string.h>
static outputs_command_fn send;
static bool linked=false, locked=true;
struct Line { bool known=false, config=false; unsigned mode=0, high=1, pull=0, debounce=30, active=0, level=0; };
static Line lines[2];
static lv_obj_t *states[2], *on[2], *off[2], *configButtons[2], *feedback;
static lv_obj_t *editor, *fields[4], *saveButton;
static unsigned editing=0, values[4];
static const char *modes[]={"DISABLED","INPUT","OUTPUT"};
static const char *pulls[]={"NONE","UP","DOWN"};
static const char *chips[]={"SX1509","MCP23017","PCA9685","TCA9548A"};
static const char *roles[]={"DIGITAL_INPUTS","DIGITAL_OUTPUTS","DIGITAL_IO","PWM_OUTPUTS","SERVO_OUTPUTS","I2C_MULTIPLEXER","NONE"};
static lv_obj_t *busState,*busDevice,*chipButton,*roleButton,*busSave,*busScan,*busNext,*busFeedback;
static unsigned busIndex=0,busTotal=0, address=0, mux=0, chip=0, role=0;
static bool busKnown=false, busDirty=false;
static lv_obj_t *text(lv_obj_t *p,int x,int y,int w,const char *s) {
 auto *o=lv_label_create(p);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);
 lv_obj_set_style_text_font(o,&lv_font_montserrat_14,0);lv_obj_set_style_text_color(o,lv_color_hex(ShowduinoPalette::Text),0);lv_label_set_text(o,s);return o;
}
static lv_obj_t *btn(lv_obj_t *p,int x,int y,int w,const char *s,lv_event_cb_t cb,unsigned data=0) {
 auto *o=lv_button_create(p);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,40);
 lv_obj_set_style_bg_color(o,lv_color_hex(ShowduinoPalette::PanelRaised),0);lv_obj_set_style_border_width(o,1,0);showduino_theme_register(o,SHOWDUINO_THEME_ROLE_BORDER);
 auto *l=text(o,0,0,w-12,s);lv_obj_center(l);lv_obj_add_event_cb(o,cb,LV_EVENT_CLICKED,(void*)(uintptr_t)data);return o;
}
static void caption(lv_obj_t *b,const char *s) {lv_label_set_text(lv_obj_get_child(b,0),s);}
static void request(const char *s) {if(linked&&send)send(s);}
static void render() {
 for(unsigned i=0;i<2;i++) {char s[100];auto &v=lines[i];
  if(!linked||!v.known)snprintf(s,sizeof(s),"Line %u / GPIO%u\nAwaiting P4 state",i+1,46+i);
  else snprintf(s,sizeof(s),"Line %u / GPIO%u: %s\n%s",i+1,46+i,modes[v.mode],v.mode?(v.active?"ACTIVE":"INACTIVE"):"High impedance");
  if(states[i])lv_label_set_text(states[i],s);
  if(on[i]) {ShowduinoOsTheme::setEnabled(on[i],linked&&v.known&&v.mode==2&&!locked);ShowduinoOsTheme::setEnabled(off[i],linked&&v.known&&v.mode==2);ShowduinoOsTheme::setEnabled(configButtons[i],linked&&v.config&&!locked);}
 }
 if(saveButton)ShowduinoOsTheme::setEnabled(saveButton,linked&&!locked&&editing&&lines[editing-1].config);
 if(busSave) {ShowduinoOsTheme::setEnabled(busSave,linked&&!locked&&busKnown&&!mux&&address!=0x18);ShowduinoOsTheme::setEnabled(busScan,linked&&!locked);ShowduinoOsTheme::setEnabled(busNext,linked&&busTotal>1);ShowduinoOsTheme::setEnabled(chipButton,linked&&!locked&&busKnown&&!mux&&address!=0x18);ShowduinoOsTheme::setEnabled(roleButton,linked&&!locked&&busKnown&&!mux&&address!=0x18);}
 if(busState&&!linked)lv_label_set_text(busState,"P4 link lost - inventory unavailable");
}
void page_p4_io_locks(bool up,bool blocked) {if(linked&&!up){for(auto &v:lines)v=Line{};busKnown=false;busDirty=false;}linked=up;locked=blocked;render();}
void page_p4_io_feedback(const char *s){if(feedback)lv_label_set_text(feedback,s);if(busFeedback)lv_label_set_text(busFeedback,s);}
static void editLabels() {char s[48];snprintf(s,sizeof(s),"Mode: %s",modes[values[0]]);caption(fields[0],s);snprintf(s,sizeof(s),"Active: %s",values[1]?"HIGH":"LOW");caption(fields[1],s);snprintf(s,sizeof(s),"Pull: %s",pulls[values[2]]);caption(fields[2],s);snprintf(s,sizeof(s),"Debounce: %u ms",values[3]);caption(fields[3],s);}
static void field(lv_event_t *e){unsigned i=(uintptr_t)lv_event_get_user_data(e);if(i==0)values[i]=(values[i]+1)%3;else if(i==1)values[i]=!values[i];else if(i==2)values[i]=(values[i]+1)%3;else {const unsigned options[]={0,30,60,100,250,500,1000,5000};unsigned j=0;while(j<8&&options[j]<=values[3])j++;values[3]=options[j%8];}editLabels();}
static void action(lv_event_t *e) {unsigned n=(uintptr_t)lv_event_get_user_data(e);unsigned i=n/3,a=n%3;if(i>=2||!linked)return;
 if(a==2) {if(locked||!lines[i].config)return;editing=i+1;values[0]=lines[i].mode;values[1]=lines[i].high;values[2]=lines[i].pull;values[3]=lines[i].debounce;editLabels();lv_obj_remove_flag(editor,LV_OBJ_FLAG_HIDDEN);render();return;}
 if(lines[i].mode!=2||!lines[i].known||(a==0&&locked))return;
 char s[40];snprintf(s,sizeof(s),"OUTPUTS:IO:%u:%s",i+1,a?"OFF":"ON");request(s);
}
void page_p4_io_create(lv_obj_t *p,outputs_command_fn fn){send=fn;
 text(p,12,98,776,"P4 LOCAL I/O | GPIO46 / GPIO47 | 3.3 V logic");
 for(unsigned i=0;i<2;i++){int y=150+i*88;states[i]=text(p,20,y,350,"Awaiting P4 state");on[i]=btn(p,390,y,108,"ACTIVE",action,i*3);off[i]=btn(p,510,y,108,"OFF",action,i*3+1);configButtons[i]=btn(p,630,y,146,"CONFIGURE",action,i*3+2);}
 btn(p,12,350,120,"REFRESH",[](lv_event_t*){request("OUTPUTS:IO:STATUS");});btn(p,144,350,132,"ALL OFF",[](lv_event_t*){request("OUTPUTS:IO:ALL:OFF");});feedback=text(p,292,350,490,"Inputs show debounced state. Outputs require ACTIVE.");
 editor=lv_obj_create(p);lv_obj_set_pos(editor,100,100);lv_obj_set_size(editor,600,294);lv_obj_set_style_bg_color(editor,lv_color_hex(ShowduinoPalette::PanelRaised),0);lv_obj_set_style_bg_opa(editor,LV_OPA_COVER,0);lv_obj_set_style_pad_all(editor,0,0);lv_obj_remove_flag(editor,LV_OBJ_FLAG_SCROLLABLE);text(editor,12,8,560,"LOCAL I/O CONFIGURATION - saves to P4 SD");
 for(unsigned i=0;i<4;i++)fields[i]=btn(editor,12+(i%2)*286,44+(i/2)*60,274,"",field,i);
 saveButton=btn(editor,12,204,274,"SAVE / LEAVE INACTIVE",[](lv_event_t*){if(!linked||locked||!editing)return;char s[96];snprintf(s,sizeof(s),"OUTPUTS:IO:%u:CONFIG:%s:%s:%s:%u",editing,modes[values[0]],values[1]?"HIGH":"LOW",pulls[values[2]],values[3]);request(s);lv_obj_add_flag(editor,LV_OBJ_FLAG_HIDDEN);});
 btn(editor,298,204,274,"CLOSE",[](lv_event_t*){lv_obj_add_flag(editor,LV_OBJ_FLAG_HIDDEN);});lv_obj_add_flag(editor,LV_OBJ_FLAG_HIDDEN);render();
}
static void busLabels(){caption(chipButton,chips[chip]);caption(roleButton,roles[role]);}
void page_p4_bus_reset(){busIndex=0;busKnown=false;busDirty=false;render();}
static void fetchBus(){char s[48];snprintf(s,sizeof(s),"OUTPUTS:PLUGIN:PAGE:%u",busIndex);request(s);}
void page_p4_bus_create(lv_obj_t *p,outputs_command_fn fn){send=fn;
 text(p,12,98,776,"P4 I2C BUS | SDA GPIO7 / SCL GPIO8 | 3.3 V");busState=text(p,12,126,776,"Awaiting P4 inventory");busDevice=text(p,12,160,776,"Select a detected root-bus device to assign a preset.");
 chipButton=btn(p,12,218,190,chips[0],[](lv_event_t*){chip=(chip+1)%4;role=chip<2?0:chip==2?3:5;busDirty=true;busLabels();});
 roleButton=btn(p,214,218,260,roles[0],[](lv_event_t*){if(chip<2)role=role<2?role+1:role==2?6:0;else if(chip==2)role=role==3?4:role==4?6:3;else role=role==5?6:5;busDirty=true;busLabels();});
 busSave=btn(p,486,218,290,"SAVE ASSIGNMENT",[](lv_event_t*){if(!linked||locked||!busKnown||mux||address==0x18)return;char s[96];snprintf(s,sizeof(s),"OUTPUTS:PLUGIN:ASSIGN:%02X:%s:%s",address,chips[chip],roles[role]);request(s);});
 text(p,12,278,776,"SX1509 / MCP23017: digital inputs, outputs or mixed I/O\nPCA9685: PWM or servos | TCA9548A: I2C multiplexer\nInternal audio at 0x18 is protected. Presets assign roles only.");
 btn(p,12,350,120,"REFRESH",[](lv_event_t*){fetchBus();});busScan=btn(p,144,350,120,"SCAN BUS",[](lv_event_t*){page_p4_bus_reset();request("OUTPUTS:PLUGIN:RESCAN");});busNext=btn(p,276,350,132,"NEXT DEVICE",[](lv_event_t*){if(busTotal){busIndex=(busIndex+1)%busTotal;busKnown=false;busDirty=false;render();fetchBus();}});busFeedback=text(p,420,350,368,"Choose a compatible preset, then SAVE.");render();
}
void page_p4_io_receive(const char *s){if(!s)return;unsigned n,m,h,p,d,a,l,index,total,addr,mx,status,configured;int end=0;
 if(sscanf(s,"STATE:IO:%u:CFG:%u,%u,%u,%u%n",&n,&m,&h,&p,&d,&end)==5&&s[end]==0&&n>=1&&n<=2&&m<=2&&h<=1&&p<=2&&d<=5000){auto &v=lines[n-1];v.config=true;v.mode=m;v.high=h;v.pull=p;v.debounce=d;render();return;}
 char mode[9]={},level[5]={};end=0;
 if(sscanf(s,"STATE:IO:%u:MODE:%8[^:]:GPIO:%u:LEVEL:%4[^:]:ACTIVE:%u%n",&n,mode,&p,level,&a,&end)==5&&s[end]==0&&n>=1&&n<=2&&p==45+n&&a<=1&&(!strcmp(mode,"INPUT")||!strcmp(mode,"OUTPUT"))&&(!strcmp(level,"HIGH")||!strcmp(level,"LOW"))){auto &v=lines[n-1];v.known=true;v.mode=!strcmp(mode,"OUTPUT")?2:1;v.active=a;v.level=!strcmp(level,"HIGH");render();return;}
 end=0;if(sscanf(s,"STATE:IO:%u:MODE:DISABLED:GPIO:%u%n",&n,&p,&end)==2&&s[end]==0&&n>=1&&n<=2&&p==45+n){lines[n-1].known=true;lines[n-1].mode=0;render();return;}
 char c[16]={},r[24]={},buf[160];end=0;
 if(sscanf(s,"STATE:BUS:%u:%u:%x:%x:%u:%15[^:]:%23[^:]:%u%n",&index,&total,&addr,&mx,&status,c,r,&configured,&end)==8&&s[end]==0&&index==busIndex&&total<=20&&addr<=0x77&&mx<=0x77&&status<=4&&configured<=1){
  bool changed=address!=addr||mux!=mx;busTotal=total;address=addr;mux=mx;busKnown=total>0;if(changed)busDirty=false;
  snprintf(buf,sizeof(buf),"Device %u / %u | %s",total?index+1:0,total,status==1?"ONLINE":status==0?"ABSENT":status==2?"OFFLINE":status==4?"AMBIGUOUS":"UNKNOWN");lv_label_set_text(busState,buf);
  snprintf(buf,sizeof(buf),"Address 0x%02X | %s | %s\n%s",addr,c,r,mx?"Behind multiplexer: assignment editing unavailable":addr==0x18?"Internal audio: protected":configured?"Configured preset":"Unconfigured device");lv_label_set_text(busDevice,buf);
  if(!busDirty){for(unsigned i=0;i<4;i++)if(!strcmp(c,chips[i]))chip=i;for(unsigned i=0;i<7;i++)if(!strcmp(r,roles[i]))role=i;busLabels();}render();return;
 }
 if(!strncmp(s,"STATE:IO:RESULT:",16)){page_p4_io_feedback(s+16);request("OUTPUTS:IO:STATUS");}
 if(!strncmp(s,"STATE:BUS:RESULT:",17)){lv_label_set_text(busFeedback,s+17);if(strstr(s+17,"Assignment saved."))busDirty=false;fetchBus();}
}
