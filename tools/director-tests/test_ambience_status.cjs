const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const source = fs.readFileSync(path.resolve(__dirname, '../../firmware/director-esp32-8048s050/ShowduinoDirector8048S050/page_05_audio_node.cpp'), 'utf8');
const start = source.indexOf('void page_05_audio_node_apply_ambience_line(');
const end = source.indexOf('uint8_t page_05_audio_node_ambience_volume', start);
const dir = fs.mkdtempSync(path.join(__dirname, '.ambience-test-'));
const cpp = path.join(dir, 'test.cpp');
const exe = path.join(dir, process.platform === 'win32' ? 'test.exe' : 'test');
fs.writeFileSync(cpp, `
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#define PAGE05_AMBIENCE_DEFAULT_PATH "/showduino/audio/ambience.wav"
static char s_amb_state[12] = "IDLE";
static char s_amb_path[96] = PAGE05_AMBIENCE_DEFAULT_PATH;
static uint8_t s_amb_volume = 80;
static char s_amb_feedback[48] = "";
static void paint() {}
${source.slice(start, end)}
int main() {
  page_05_audio_node_apply_ambience_line("AMBIENCE:STATUS:LOOPING:VOL=30:LOCK=0:ERROR=");
  assert(!strcmp(s_amb_state, "LOOPING") && s_amb_volume == 30);
  assert(!strcmp(s_amb_path, PAGE05_AMBIENCE_DEFAULT_PATH));
  page_05_audio_node_apply_ambience_line("AMBIENCE:STATUS:EMERGENCY:VOL=100:LOCK=1:ERROR=");
  assert(!strcmp(s_amb_state, "EMERGENCY") && s_amb_volume == 100);
  page_05_audio_node_apply_ambience_line("OK:AMBIENCE:VOLUME:0");
  assert(s_amb_volume == 0);
  page_05_audio_node_apply_ambience_line("ERR:AMBIENCE:EMERGENCY_LOCKED");
  assert(strstr(s_amb_feedback, "ERROR: EMERGENCY_LOCKED"));
  page_05_audio_node_apply_ambience_line(nullptr);
  page_05_audio_node_apply_ambience_line("");
  assert(s_amb_volume == 0);
  puts("Passed: actual Director ambience parser receives P4 status, volume and errors");
}
`);
try {
  for (const [command, args] of [['g++', ['-std=c++17', '-Wall', '-Wextra', cpp, '-o', exe]], [exe, []]]) {
    const result = spawnSync(command, args, { stdio: 'inherit' });
    if (result.error) throw result.error;
    if (result.status !== 0) process.exitCode = result.status || 1;
    if (process.exitCode) break;
  }
} finally {
  for (const file of [cpp, exe]) if (fs.existsSync(file)) fs.unlinkSync(file);
  fs.rmdirSync(dir);
}
