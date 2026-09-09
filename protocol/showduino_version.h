#ifndef SHOWDUINO_VERSION_H
#define SHOWDUINO_VERSION_H

#include <string.h>
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

#define SHOWDUINO_GITHUB_OWNER "sumkindafreak"
#define SHOWDUINO_GITHUB_REPO "new_showduino"
#define SHOWDUINO_GITHUB_RELEASES_API \
  "https://api.github.com/repos/sumkindafreak/new_showduino/releases?per_page=8"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ShowduinoVersionParts {
  int major;
  int minor;
  int patch;
  int preKind; /* 0 = final release, 1 = rc, 2 = other prerelease */
  int preNum;
} ShowduinoVersionParts;

static inline const char *showduino_version_skip_v(const char *s) {
  if (!s) return "";
  if (s[0] == 'v' || s[0] == 'V') return s + 1;
  return s;
}

static inline int showduino_parse_version(const char *s, ShowduinoVersionParts *out) {
  if (!out) return 0;
  memset(out, 0, sizeof(*out));
  s = showduino_version_skip_v(s);
  if (!s || *s < '0' || *s > '9') return 0;
  int major = 0, minor = 0, patch = 0;
  while (*s >= '0' && *s <= '9') {
    major = major * 10 + (*s - '0');
    if (major > 9999) return 0;
    ++s;
  }
  if (*s != '.') return 0;
  ++s;
  while (*s >= '0' && *s <= '9') {
    minor = minor * 10 + (*s - '0');
    if (minor > 9999) return 0;
    ++s;
  }
  if (*s != '.') return 0;
  ++s;
  while (*s >= '0' && *s <= '9') {
    patch = patch * 10 + (*s - '0');
    if (patch > 9999) return 0;
    ++s;
  }
  out->major = major;
  out->minor = minor;
  out->patch = patch;
  if (*s == '-' || *s == '_') {
    ++s;
    if ((s[0] == 'r' || s[0] == 'R') && (s[1] == 'c' || s[1] == 'C')) {
      out->preKind = 1;
      s += 2;
      if (*s == '.' || *s == '-') ++s;
      int n = 0;
      while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        ++s;
      }
      out->preNum = n;
    } else if (*s) {
      out->preKind = 2;
    }
  }
  return 1;
}

/* <0 if a < b, 0 if equal, >0 if a > b. Unparseable versions compare as equal-low. */
static inline int showduino_version_compare(const char *a, const char *b) {
  ShowduinoVersionParts pa, pb;
  const int oka = showduino_parse_version(a, &pa);
  const int okb = showduino_parse_version(b, &pb);
  if (!oka && !okb) return 0;
  if (!oka) return -1;
  if (!okb) return 1;
  if (pa.major != pb.major) return pa.major - pb.major;
  if (pa.minor != pb.minor) return pa.minor - pb.minor;
  if (pa.patch != pb.patch) return pa.patch - pb.patch;
  if (pa.preKind == 0 && pb.preKind == 0) return 0;
  if (pa.preKind == 0) return 1;
  if (pb.preKind == 0) return -1;
  if (pa.preKind != pb.preKind) return pb.preKind - pa.preKind;
  return pa.preNum - pb.preNum;
}

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
