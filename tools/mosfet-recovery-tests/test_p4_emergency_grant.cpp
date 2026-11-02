#define SHOWDUINO_LOG_H
#include <cassert>
#include "../../firmware/stage-engine-p4/ShowduinoStageEngineP4/src/nodes/MosfetNodeLink.cpp"
bool emergencyLocked=false;
static std::vector<std::string> routed;
bool sendToDirector(const String&){return true;}
void stageCommsSendLine(const char* s){routed.emplace_back(s);}
static void expectRoute(const char* command){bool seen=false;for(const auto& s:routed){
 assert(s.find("MOSFET:OWN:GRANT")==std::string::npos||!emergencyLocked);
 if(s.find(command)!=std::string::npos)seen=true;
 }assert(seen);}
int main(){
 mosfetNodeLinkBegin();emergencyLocked=true;
 char announce[180],report[210];
 showduino_mosfet_format_announce(announce,sizeof(announce),"00:11:22:33:44:55","0.1.4-rc1","SEARCHING","MOSFET-01","TEST");
 std::snprintf(report,sizeof(report),"NODE:MOSFET:%s",announce);
 assert(mosfetNodeLinkHandleReport(report));expectRoute("EMERGENCY:STOP");
 auto& node=sNodes[0];assert(node.used&&node.online);
 routed.clear();testNow+=2000;mosfetNodeLinkLoop();expectRoute("EMERGENCY:STOP");
 routed.clear();emergencyLocked=false;node.lastRxMs=testNow;testNow+=2000;mosfetNodeLinkLoop();expectRoute("MOSFET:OWN:GRANT");
 puts("PASS actual P4 MOSFET routing: Emergency asserts instead of granting automatic restoration");
}
