#pragma once

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "Service.h"
#include "../OsTypes.h"
#include "../events/EventBus.h"

namespace Os2 {

/**
 * DeviceService — device inventory ONLY.
 *
 * Owns: Director / Stage / SUE / child node presence.
 * Does NOT own: link quality (NetworkService) or show runtime (ShowService).
 *
 *   Stage Controller · Director · SUE · Child nodes
 *
 * Populated by the communication / discovery layer. Apps read only.
 */
class DeviceService : public IService {
 public:
  enum class Role : uint8_t {
    Director = 0,
    Stage,
    Sue,
    Node,
    Unknown
  };

  struct Entry {
    Role role;
    char name[24];
    char id[16];
    StatusLevel status;
    bool present;
  };

  static constexpr int kMaxDevices = 12;

  const char *id() const override { return "device"; }

  void begin() override {
    count_ = 0;
    revision_ = 0;
    /* Director is always present — this board. */
    upsert(Role::Director, "Director", "local", StatusLevel::Healthy, true);
  }

  void setStage(bool present, StatusLevel status = StatusLevel::Healthy) {
    upsert(Role::Stage, "Stage Controller", "stage", status, present);
  }

  void setSue(bool present, StatusLevel status = StatusLevel::Healthy) {
    upsert(Role::Sue, "SUE", "sue", status, present);
  }

  void setNode(uint8_t index, bool present, StatusLevel status = StatusLevel::Healthy) {
    char name[24];
    char id[16];
    snprintf(name, sizeof(name), "Node %u", (unsigned)index);
    snprintf(id, sizeof(id), "node%u", (unsigned)index);
    upsert(Role::Node, name, id, status, present);
  }

  int count() const { return count_; }
  const Entry *at(int i) const {
    return (i >= 0 && i < count_) ? &devices_[i] : nullptr;
  }

  int presentCount() const {
    int n = 0;
    for (int i = 0; i < count_; ++i) if (devices_[i].present) ++n;
    return n;
  }

  StatusLevel status() const {
    bool stageOk = false;
    for (int i = 0; i < count_; ++i) {
      if (devices_[i].role == Role::Stage) {
        if (!devices_[i].present) return StatusLevel::Critical;
        if (devices_[i].status == StatusLevel::Critical) return StatusLevel::Critical;
        if (devices_[i].status == StatusLevel::Warning) return StatusLevel::Warning;
        stageOk = devices_[i].present;
      }
    }
    return stageOk ? StatusLevel::Healthy : StatusLevel::Warning;
  }

  const char *roleLabel(Role r) const {
    switch (r) {
      case Role::Director: return "Director";
      case Role::Stage:    return "Stage";
      case Role::Sue:      return "SUE";
      case Role::Node:     return "Node";
      default:             return "Device";
    }
  }

  uint32_t revision() const { return revision_; }

 private:
  void bump() { ++revision_; events().publish(Event::DeviceChanged); }

  void upsert(Role role, const char *name, const char *id,
              StatusLevel status, bool present) {
    for (int i = 0; i < count_; ++i) {
      if (devices_[i].role == role && strcmp(devices_[i].id, id) == 0) {
        bool changed = devices_[i].present != present ||
                       devices_[i].status != status ||
                       strcmp(devices_[i].name, name) != 0;
        devices_[i].present = present;
        devices_[i].status = status;
        strncpy(devices_[i].name, name, sizeof(devices_[i].name) - 1);
        devices_[i].name[sizeof(devices_[i].name) - 1] = '\0';
        if (changed) bump();
        return;
      }
    }
    if (count_ >= kMaxDevices) return;
    Entry &e = devices_[count_++];
    e.role = role;
    e.status = status;
    e.present = present;
    strncpy(e.name, name, sizeof(e.name) - 1);
    e.name[sizeof(e.name) - 1] = '\0';
    strncpy(e.id, id, sizeof(e.id) - 1);
    e.id[sizeof(e.id) - 1] = '\0';
    bump();
  }

  Entry devices_[kMaxDevices]{};
  int count_ = 0;
  uint32_t revision_ = 0;
};

}  // namespace Os2