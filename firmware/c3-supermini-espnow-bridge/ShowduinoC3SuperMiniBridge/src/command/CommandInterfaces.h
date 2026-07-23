#ifndef SHOWDUINO_COMMAND_INTERFACES_H
#define SHOWDUINO_COMMAND_INTERFACES_H

#include "ShowCommand.h"

/**
 * Integration hooks for CommandDispatcher.
 * Stage 7: IDeviceRouter / ICapabilityManager implemented by
 * DeviceRouter and CapabilityManager.
 * IStageRuntimeBridge remains a null stub until Stage 8.
 */
class IDeviceRouter {
 public:
  virtual ~IDeviceRouter() {}
  /** Resolve target device. Must not execute hardware. */
  virtual bool route(const ShowCommand &cmd) = 0;
};

class ICapabilityManager {
 public:
  virtual ~ICapabilityManager() {}
  /** Return true if the command is capability-plausible. */
  virtual bool supports(const ShowCommand &cmd) = 0;
};

class IStageRuntimeBridge {
 public:
  virtual ~IStageRuntimeBridge() {}
  /** Returns false — Stage Runtime not connected until Stage 8. */
  virtual bool submit(const ShowCommand &cmd) = 0;
};

class NullDeviceRouter : public IDeviceRouter {
 public:
  bool route(const ShowCommand &cmd) override { (void)cmd; return false; }
};

class NullCapabilityManager : public ICapabilityManager {
 public:
  bool supports(const ShowCommand &cmd) override { (void)cmd; return true; }
};

class NullStageRuntimeBridge : public IStageRuntimeBridge {
 public:
  bool submit(const ShowCommand &cmd) override { (void)cmd; return false; }
};

#endif