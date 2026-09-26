// Host-side check: Audio Node pixel timeline commands route via AUDIO:NODE:PIXEL:
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Mirror of the P4 strip logic used by AudioNodeLink. */
static int build_forward(const char *command, char *out, size_t outLen) {
  if (!command || strncmp(command, "AUDIO:NODE:PIXEL:", 17) != 0) return 0;
  const char *pixelCmd = command + 17;
  if (!pixelCmd[0]) return -1;
  if (!strncmp(pixelCmd, "PIXEL:", 6)) {
    snprintf(out, outLen, "%s", pixelCmd);
  } else {
    snprintf(out, outLen, "PIXEL:%s", pixelCmd);
  }
  return 1;
}

int main(void) {
  char buf[96];
  assert(build_forward("AUDIO:NODE:PIXEL:SEGMENT:0:FX:FIRE", buf, sizeof(buf)) == 1);
  assert(strcmp(buf, "PIXEL:SEGMENT:0:FX:FIRE") == 0);
  assert(build_forward("AUDIO:NODE:PIXEL:PIXEL:STATUS", buf, sizeof(buf)) == 1);
  assert(strcmp(buf, "PIXEL:STATUS") == 0);
  assert(build_forward("AUDIO:NODE:PLAY:x.wav", buf, sizeof(buf)) == 0);
  printf("audio_pixel_route_ok\n");
  return 0;
}
