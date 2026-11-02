#include "support.h"
// Inject symbol-allocation failures only into the actual output implementation.
#define calloc outputCalloc
#define free outputFree
#include "../../firmware/audio-node-esp32-a1s/ShowduinoAudioNode/src/AudioPixelOutput.cpp"
#undef free
#undef calloc
