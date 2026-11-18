#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
struct DirectorCueLibraryState {
  uint32_t elapsedMs, fired, total;
  bool matched, online, emergency;
  char name[64], state[24];
};
typedef void (*DirectorCueStateFn)(DirectorCueLibraryState *);
typedef bool (*DirectorCueReadFn)(uint32_t index, uint32_t *timeMs, char *command, size_t size);
typedef bool (*DirectorCueStartFn)(uint32_t timeMs);
void directorCueLibrarySetSource(DirectorCueStateFn state, DirectorCueReadFn read, DirectorCueStartFn start);
void directorCueLibraryOpen();
void directorCueLibraryClose();

inline bool directorCueCanStart(const DirectorCueLibraryState &state) {
 return state.matched && state.online && !state.emergency &&
  (!strcmp(state.state,"PAUSED") || !strcmp(state.state,"SHOW_LOADED") ||
   !strcmp(state.state,"IDLE") || !strcmp(state.state,"FINISHED"));
}
