#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include <algorithm>
using gpio_num_t=int;
using i2s_chan_handle_t=int*;
inline constexpr int ESP_OK=0,ESP_ERR_TIMEOUT=0x107,I2S_NUM_AUTO=0,I2S_ROLE_MASTER=0,I2S_GPIO_UNUSED=-1,I2S_DATA_BIT_WIDTH_16BIT=16,I2S_SLOT_MODE_STEREO=2;
struct i2s_chan_config_t {int port;int role;};
struct i2s_std_config_t {
 struct Clock{uint32_t rate;} clk_cfg;
 struct Slot{int bits,mode;} slot_cfg;
 struct Gpio{int mclk,bclk,ws,dout,din;struct{bool mclk_inv,bclk_inv,ws_inv;}invert_flags;}gpio_cfg;
};
#define I2S_CHANNEL_DEFAULT_CONFIG(p,r) i2s_chan_config_t{p,r}
#define I2S_STD_CLK_DEFAULT_CONFIG(r) i2s_std_config_t::Clock{r}
#define I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(b,m) i2s_std_config_t::Slot{b,m}
inline int allocations=0;
inline bool failI2s=false;
inline size_t maxWriteBytes=SIZE_MAX;
inline int writeResult=ESP_OK;
inline std::vector<int16_t> sentSamples;
inline int i2s_new_channel(i2s_chan_config_t*,i2s_chan_handle_t* out,void*){if(failI2s)return -1;*out=new int(1);++allocations;return ESP_OK;}
inline int i2s_channel_disable(i2s_chan_handle_t){return ESP_OK;}
inline int i2s_del_channel(i2s_chan_handle_t p){delete p;return ESP_OK;}
inline int i2s_channel_init_std_mode(i2s_chan_handle_t,i2s_std_config_t*){return ESP_OK;}
inline int i2s_channel_enable(i2s_chan_handle_t){return ESP_OK;}
inline int i2s_channel_write(i2s_chan_handle_t,const void* b,size_t n,size_t* written,int){if(writeResult!=ESP_OK){*written=0;return writeResult;}n=std::min(n,maxWriteBytes);auto p=static_cast<const int16_t*>(b);sentSamples.insert(sentSamples.end(),p,p+n/2);*written=n;return ESP_OK;}
