#pragma once

#include <stdint.h>
#include <string.h>

namespace Os2 {

/**
 * Layer 3 — Services.
 * Apps never talk to hardware, transport, or storage directly.
 * They ask a service. Exactly like Android / Windows / macOS.
 */
class IService {
 public:
  virtual ~IService() = default;
  virtual const char *id() const = 0;
  virtual void begin() = 0;
  virtual void tick(uint32_t /*nowMs*/) {}
};

class ServiceRegistry {
 public:
  static constexpr int kMax = 16;

  static ServiceRegistry &instance() {
    static ServiceRegistry r;
    return r;
  }

  bool add(IService *svc) {
    if (!svc || count_ >= kMax) return false;
    for (int i = 0; i < count_; ++i) {
      if (services_[i] == svc) return true;
    }
    services_[count_++] = svc;
    return true;
  }

  IService *find(const char *id) const {
    if (!id) return nullptr;
    for (int i = 0; i < count_; ++i) {
      if (services_[i] && services_[i]->id() &&
          strcmp(services_[i]->id(), id) == 0) {
        return services_[i];
      }
    }
    return nullptr;
  }

  template <typename T>
  T *get(const char *id) const {
    return static_cast<T *>(find(id));
  }

  void beginAll() {
    for (int i = 0; i < count_; ++i) {
      if (services_[i]) services_[i]->begin();
    }
  }

  void tickAll(uint32_t nowMs) {
    for (int i = 0; i < count_; ++i) {
      if (services_[i]) services_[i]->tick(nowMs);
    }
  }

 private:
  ServiceRegistry() = default;
  IService *services_[kMax]{};
  int count_ = 0;
};

inline ServiceRegistry &services() { return ServiceRegistry::instance(); }

}  // namespace Os2