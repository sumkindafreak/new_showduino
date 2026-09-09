#ifndef SHOWDUINO_VERSION_H
#define SHOWDUINO_VERSION_H

#include "showduino_protocol_version.h"

/*
 * Authoritative Showduino PRODUCT / PLATFORM identity.
 *
 * Component firmware versions may continue to evolve independently.
 * User-facing UI should report this platform release first. Compatibility
 * is protocol/schema based, not component patch matching.
 *
 * This tree is a release candidate until hardware commissioning passes.
 */
#define SHOWDUINO_PRODUCT_NAME "Showduino"
#define SHOWDUINO_PLATFORM_VERSION "1.0.0-rc.1"
#define SHOWDUINO_PLATFORM_VERSION_MAJOR 1
#define SHOWDUINO_PLATFORM_VERSION_MINOR 0
#define SHOWDUINO_PLATFORM_VERSION_PATCH 0
#define SHOWDUINO_PLATFORM_PRERELEASE "rc.1"
#define SHOWDUINO_RELEASE_CANDIDATE 1

#define SHOWDUINO_SHDO_SCHEMA_NAME "showduino-production-v2"
#define SHOWDUINO_SHDO_PACKAGE_FORMAT "showduino-production"
#define SHOWDUINO_SHDO_PACKAGE_VERSION 2

#define SHOWDUINO_COMPONENT_P4 "P4 Show Engine"
#define SHOWDUINO_COMPONENT_COMMS "Communications S3"
#define SHOWDUINO_COMPONENT_DIRECTOR "Director"
#define SHOWDUINO_COMPONENT_AUDIO "Audio Node"
#define SHOWDUINO_COMPONENT_LAMP "Lamp Node"
#define SHOWDUINO_COMPONENT_STUDIO "Studio V4"

#ifdef __cplusplus
extern "C" {
#endif

static inline int showduino_protocol_compatible(int peer_major) {
  return peer_major == SHOWDUINO_PROTOCOL_VERSION_MAJOR;
}

static inline int showduino_shdo_package_supported(int package_version) {
  return package_version == SHOWDUINO_SHDO_PACKAGE_VERSION;
}

static inline int showduino_platform_major_compatible(int peer_major) {
  return peer_major == SHOWDUINO_PLATFORM_VERSION_MAJOR;
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_VERSION_H */
