#include "support.h"
#include <new>
void* operator new[](size_t size,const std::nothrow_t&) noexcept {
  if(failNextRenderAllocation){failNextRenderAllocation=false;return nullptr;}
  try{return ::operator new[](size);}catch(...){return nullptr;}
}
void operator delete[](void* p,const std::nothrow_t&) noexcept {::operator delete[](p);}
