#include "support.h"
#include <stdlib.h> // Load the GCC C compatibility wrapper before macro hooks.
// Inject symbol-allocation failures only into the actual output implementation.
#define calloc outputCalloc
#define free outputFree
#include "../../firmware/audio-node-esp32-a1s/ShowduinoAudioNode/src/AudioPixelOutput.cpp"
#undef free
#undef calloc
