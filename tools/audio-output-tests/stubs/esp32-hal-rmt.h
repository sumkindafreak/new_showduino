#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <cassert>
// Field-shaped double, not the hardware bitfield ABI or a timing simulation.
struct rmt_data_t {uint32_t level0=0,duration0=0,level1=0,duration1=0;};
enum {RMT_TX_MODE=1,RMT_MEM_NUM_BLOCKS_1=1};
struct WriteAttempt {std::vector<rmt_data_t> symbols;bool success;uint32_t timeout;};
struct TestChannel {
  bool active=false,failInit=false,failWrite=false;
  unsigned inits=0,deinits=0;uint32_t hz=0;
  std::vector<WriteAttempt> writes;
};
inline TestChannel channels[40];
inline bool rmtInit(int pin,int mode,int blocks,uint32_t hz){
  assert(pin>=0&&pin<40&&mode==RMT_TX_MODE&&blocks==RMT_MEM_NUM_BLOCKS_1);
  auto& c=channels[pin];++c.inits;c.hz=hz;
  if(c.failInit)return false;
  assert(!c.active);c.active=true;return true;
}
inline bool rmtWrite(int pin,rmt_data_t* data,size_t n,uint32_t timeout){
  assert(pin>=0&&pin<40&&channels[pin].active);
  auto& c=channels[pin];c.writes.push_back({std::vector<rmt_data_t>(data,data+n),!c.failWrite,timeout});
  return !c.failWrite;
}
inline bool rmtDeinit(int pin){assert(pin>=0&&pin<40);auto& c=channels[pin];++c.deinits;c.active=false;return true;}
