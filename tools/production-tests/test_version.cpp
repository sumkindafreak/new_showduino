#include <cstdio>
#include <cstring>

#include "../../protocol/showduino_version.h"

static int failures = 0;

static void expect(bool condition, const char *name) {
  std::printf("%s  %s\n", condition ? "PASS" : "FAIL", name);
  if (!condition) ++failures;
}

int main() {
  expect(showduino_version_compare("1.0.0-rc.1", "1.0.0-rc.1") == 0, "rc.1 equals rc.1");
  expect(showduino_version_compare("1.0.0-rc.1", "1.0.0-rc.2") < 0, "rc.1 older than rc.2");
  expect(showduino_version_compare("1.0.0-rc.2", "1.0.0-rc.1") > 0, "rc.2 newer than rc.1");
  expect(showduino_version_compare("1.0.0-rc.1", "1.0.0") < 0, "rc older than final");
  expect(showduino_version_compare("1.0.0", "1.0.0-rc.1") > 0, "final newer than rc");
  expect(showduino_version_compare("v1.0.0", "1.0.0") == 0, "v prefix ignored");
  expect(showduino_version_compare("0.9.1", "1.0.0-rc.1") < 0, "0.9.1 older than 1.0.0-rc.1");
  expect(std::strcmp(SHOWDUINO_PLATFORM_VERSION, "1.0.0-rc.1") == 0, "product version is 1.0.0-rc.1");
  return failures ? 1 : 0;
}
