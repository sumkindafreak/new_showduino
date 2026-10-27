#include <cassert>
#include <cstdio>
#include "../../firmware/director-esp32-8048s050/ShowduinoDirector8048S050/DirectorAudioNodeControl.h"

int main() {
  static_assert(DIRECTOR_AUDIO_ASSETS_PER_SCREEN <= SHOWDUINO_AUDIO_INV_WIRE_MAX,
                "screen rows must fit model storage");
  DirectorAudioNodeControl m;
  director_audio_node_clear(&m);
  char names[SHOWDUINO_AUDIO_INV_WIRE_MAX][SHOWDUINO_AUDIO_INV_NAME_MAX + 1] = {};
  auto receive = [&](uint16_t page, uint16_t total, const char *name) {
    snprintf(names[0], sizeof(names[0]), "%s", name);
    return director_audio_inventory_accept(&m, page, total, names, name[0] ? 1 : 0);
  };
  assert(director_audio_inventory_begin(&m, 0) == 0);
  assert(receive(0, 7, "show/roar.wav") == 1);
  assert(receive(0, 7, "duplicate.wav") == -1); // duplicate cannot erase rows
  assert(receive(2, 7, "out-of-order.wav") == -1);
  assert(receive(1, 7, "show/music.mp3") == 2);
  assert(receive(2, 7, "show/emergency.wav") == -1);
  assert(!m.inventoryLoading && m.inventoryCount == 3 && m.inventoryTotal == 7);
  assert(!strcmp(m.inventory[0], "show/roar.wav"));
  assert(!strcmp(m.inventory[1], "show/music.mp3"));
  assert(!strcmp(m.inventory[2], "show/emergency.wav"));
  assert(receive(2, 7, "unsolicited.wav") == -1);
  assert(!strcmp(m.inventory[2], "show/emergency.wav"));
  assert(director_audio_inventory_begin(&m, 3) == 3);
  assert(m.inventoryCount == 0 && !m.inventory[0][0]);
  assert(receive(3, 7, "four.wav") == 4);
  assert(receive(4, 7, "five.mp3") == 5);
  assert(receive(5, 7, "six.wav") == -1);
  assert(director_audio_inventory_begin(&m, 6) == 6);
  assert(receive(6, 7, "last.mp3") == -1);
  assert(m.inventoryCount == 1 && !m.inventory[1][0]);
  assert(director_audio_inventory_begin(&m, 3) == 3); // previous screen
  assert(receive(3, 3, "") == -1); // SD contents shrank
  assert(!m.inventoryLoading && !m.inventoryCount);
  assert(director_audio_inventory_begin(&m, 0) == 0); // refresh/retry
  assert(receive(0, 0, "") == -1);
  assert(!m.inventoryLoading && !m.inventoryCount && !m.inventoryTotal);
  director_audio_inventory_begin(&m, 0);
  m.inventoryLoading = false; // close or select stops background requests
  assert(receive(0, 7, "late.wav") == -1);
  assert(m.inventoryCount == 0);
  puts("Director multi-asset inventory tests passed");
}
