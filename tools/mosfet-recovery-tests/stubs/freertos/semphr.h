#pragma once
#include <mutex>
#include <atomic>
using SemaphoreHandle_t=std::recursive_mutex*;
inline bool failMutexCreate=false;
inline std::atomic<unsigned> lockContention{0};
inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(){static std::recursive_mutex value;return failMutexCreate?nullptr:&value;}
inline int xSemaphoreTakeRecursive(SemaphoreHandle_t mutex,unsigned){if(!mutex->try_lock()){++lockContention;mutex->lock();}return 1;}
inline int xSemaphoreGiveRecursive(SemaphoreHandle_t mutex){mutex->unlock();return 1;}
