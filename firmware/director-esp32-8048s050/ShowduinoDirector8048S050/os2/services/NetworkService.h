#pragma once

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "Service.h"
#include "../OsTypes.h"
#include "../events/EventBus.h"

namespace Os2 {

/**
 * NetworkService — fabric / link health for OS apps.
 *
 *   Link status · Connected nodes · Latency · Heartbeats · Health
 *
 * Apps never open sockets or ESP-NOW. Communication layer pushes snapshots here.
 */
class NetworkService : public IService {
 public:
  enum class Link : uint8_t {
    Searching = 0,
    Ready = 1,
    Disconnected = 2
  };

  const char *id() const override { return "network"; }

  void begin() override {
    link_ = Link::Searching;
    nodesOnline_ = 0;
    nodesExpected_ = 1;
    latencyMs_ = 0;
    lastHeartbeatMs_ = 0;
    heartbeatAgeMs_ = 0;
    espNowReady_ = false;
    txCount_ = 0;
    rxCount_ = 0;
    revision_ = 0;
    bump();
  }

  void applyLink(Link link) {
    if (link_ == link) return;
    link_ = link;
    bump();
    events().publish(Event::LinkChanged, (uint32_t)link_);
    events().publish(Event::NetworkHealth);
  }

  void applyNodes(uint8_t online, uint8_t expected) {
    if (nodesOnline_ == online && nodesExpected_ == expected) return;
    const uint8_t prev = nodesOnline_;
    nodesOnline_ = online;
    nodesExpected_ = expected ? expected : 1;
    bump();
    if (online > prev) events().publish(Event::NodeJoined, online, nodesExpected_);
    else if (online < prev) events().publish(Event::NodeLost, online, nodesExpected_);
    events().publish(Event::NetworkHealth);
  }

  void applyHeartbeat(uint32_t lastReplyMs, uint32_t nowMs) {
    lastHeartbeatMs_ = lastReplyMs;
    uint32_t age = (lastReplyMs == 0 || nowMs < lastReplyMs) ? 0 : (nowMs - lastReplyMs);
    if (heartbeatAgeMs_ == age && latencyMs_ != 0) {
      /* age changes often — bump only on meaningful steps */
    }
    uint32_t prevAge = heartbeatAgeMs_;
    heartbeatAgeMs_ = age;
    if (age / 500 != prevAge / 500) bump();
  }

  void applyLatency(uint32_t rttMs) {
    if (latencyMs_ == rttMs) return;
    latencyMs_ = rttMs;
    bump();
  }

  void applyTransport(bool espNowReady) {
    if (espNowReady_ == espNowReady) return;
    espNowReady_ = espNowReady;
    bump();
  }

  void applyTraffic(uint32_t tx, uint32_t rx) {
    if (txCount_ == tx && rxCount_ == rx) return;
    txCount_ = tx;
    rxCount_ = rx;
    /* traffic is noisy — don't bump revision every packet */
  }

  void tick(uint32_t nowMs) override {
    if (lastHeartbeatMs_ == 0) return;
    uint32_t age = (nowMs < lastHeartbeatMs_) ? 0 : (nowMs - lastHeartbeatMs_);
    uint32_t prev = heartbeatAgeMs_;
    heartbeatAgeMs_ = age;
    if (age / 1000 != prev / 1000) bump();
  }

  /* ---- Read API --------------------------------------------------------- */

  Link link() const { return link_; }
  bool online() const { return link_ == Link::Ready; }

  const char *linkLabel() const {
    switch (link_) {
      case Link::Ready:        return "READY";
      case Link::Searching:    return "SEARCHING";
      case Link::Disconnected: return "LOST";
      default:                 return "SEARCHING";
    }
  }

  uint8_t nodesOnline() const { return nodesOnline_; }
  uint8_t nodesExpected() const { return nodesExpected_; }
  uint32_t latencyMs() const { return latencyMs_; }
  uint32_t heartbeatAgeMs() const { return heartbeatAgeMs_; }
  bool espNowReady() const { return espNowReady_; }
  uint32_t txCount() const { return txCount_; }
  uint32_t rxCount() const { return rxCount_; }

  StatusLevel status() const {
    if (link_ == Link::Disconnected) return StatusLevel::Critical;
    if (link_ == Link::Searching) return StatusLevel::Working;
    if (nodesExpected_ > 0 && nodesOnline_ < nodesExpected_) return StatusLevel::Warning;
    if (heartbeatAgeMs_ > 5000) return StatusLevel::Warning;
    return StatusLevel::Healthy;
  }

  void formatNodes(char *buf, size_t n) const {
    if (!buf || n == 0) return;
    snprintf(buf, n, "%u / %u", (unsigned)nodesOnline_, (unsigned)nodesExpected_);
  }

  void formatHealth(char *buf, size_t n) const {
    if (!buf || n == 0) return;
    if (link_ == Link::Ready) {
      if (latencyMs_ > 0) snprintf(buf, n, "%s · %lums", linkLabel(), (unsigned long)latencyMs_);
      else snprintf(buf, n, "%s", linkLabel());
    } else {
      snprintf(buf, n, "%s", linkLabel());
    }
  }

  uint32_t revision() const { return revision_; }

 private:
  void bump() { ++revision_; }

  Link link_ = Link::Searching;
  uint8_t nodesOnline_ = 0;
  uint8_t nodesExpected_ = 1;
  uint32_t latencyMs_ = 0;
  uint32_t lastHeartbeatMs_ = 0;
  uint32_t heartbeatAgeMs_ = 0;
  bool espNowReady_ = false;
  uint32_t txCount_ = 0;
  uint32_t rxCount_ = 0;
  uint32_t revision_ = 0;
};

}  // namespace Os2