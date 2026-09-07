#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "showduino_storage.h"

static int gFail = 0;

static void expect(int cond, const char *name) {
  if (cond) {
    printf("PASS %s\n", name);
  } else {
    printf("FAIL %s\n", name);
    gFail++;
  }
}

static void expectPath(ShowduinoPathStatus got, ShowduinoPathStatus want, const char *name) {
  if (got == want) {
    printf("PASS %s\n", name);
  } else {
    printf("FAIL %s got=%s want=%s\n", name,
           showduino_storage_path_name(got), showduino_storage_path_name(want));
    gFail++;
  }
}

static void expectCfg(ShowduinoConfigStatus got, ShowduinoConfigStatus want, const char *name) {
  if (got == want) {
    printf("PASS %s\n", name);
  } else {
    printf("FAIL %s got=%s want=%s\n", name,
           showduino_storage_cfg_name(got), showduino_storage_cfg_name(want));
    gFail++;
  }
}

int main() {
  expectPath(showduino_storage_path_check("/showduino/config/system.json"),
             SHOWDUINO_PATH_OK, "valid config path");
  expectPath(showduino_storage_path_check("/showduino"),
             SHOWDUINO_PATH_OK, "root path");
  expectPath(showduino_storage_path_check(NULL), SHOWDUINO_PATH_NULL, "null path");
  expectPath(showduino_storage_path_check(""), SHOWDUINO_PATH_EMPTY, "empty path");
  expectPath(showduino_storage_path_check("showduino/config/system.json"),
             SHOWDUINO_PATH_NOT_ABSOLUTE, "relative path");
  expectPath(showduino_storage_path_check("/showduino/config/../system.json"),
             SHOWDUINO_PATH_DOTDOT, "dotdot rejected");
  expectPath(showduino_storage_path_check("/showduino/config\\system.json"),
             SHOWDUINO_PATH_BACKSLASH, "backslash rejected");
  expectPath(showduino_storage_path_check("/etc/passwd"),
             SHOWDUINO_PATH_OUTSIDE_ROOT, "outside root");
  expectPath(showduino_storage_path_check("/showduinoX/config.json"),
             SHOWDUINO_PATH_OUTSIDE_ROOT, "prefix collision");
  expectPath(showduino_storage_path_check("/showduino/config//system.json"),
             SHOWDUINO_PATH_DOUBLE_SLASH, "double slash");

  char longPath[256];
  memset(longPath, 'a', sizeof(longPath));
  longPath[0] = '/';
  memcpy(longPath + 1, "showduino/", 10);
  longPath[255] = '\0';
  expectPath(showduino_storage_path_check(longPath), SHOWDUINO_PATH_TOO_LONG, "oversized path");

  char joined[192];
  expectPath(showduino_storage_path_join("/showduino/config", "system.json", joined, sizeof(joined)),
             SHOWDUINO_PATH_OK, "join ok");
  expect(strcmp(joined, "/showduino/config/system.json") == 0, "join result");
  expectPath(showduino_storage_path_join("/showduino/config", "../etc", joined, sizeof(joined)),
             SHOWDUINO_PATH_BAD_NAME, "join traversal");
  expectPath(showduino_storage_path_join("/tmp", "x.json", joined, sizeof(joined)),
             SHOWDUINO_PATH_OUTSIDE_ROOT, "join outside parent");
  expectPath(showduino_storage_path_join("/showduino/config", "a/b.json", joined, sizeof(joined)),
             SHOWDUINO_PATH_BAD_NAME, "join nested name");

  char tmp[192];
  expectPath(showduino_storage_temp_path("/showduino/config/system.json", tmp, sizeof(tmp)),
             SHOWDUINO_PATH_OK, "temp path");
  expect(strcmp(tmp, "/showduino/config/system.json.tmp") == 0, "temp suffix");
  expect(strcmp(showduino_storage_basename("/showduino/config/system.json"), "system.json") == 0,
         "basename");

  const char *okJson = "{ \"formatVersion\": 1, \"systemName\": \"Showduino\" }";
  expectCfg(showduino_storage_config_check(okJson, strlen(okJson), 4096, 1),
            SHOWDUINO_CFG_OK, "valid config");
  expectCfg(showduino_storage_config_check(NULL, 0, 4096, 1),
            SHOWDUINO_CFG_EMPTY, "empty config");
  expectCfg(showduino_storage_config_check("[]", 2, 4096, 1),
            SHOWDUINO_CFG_NOT_OBJECT, "array rejected");
  const char *noVer = "{ \"systemName\": \"Showduino\" }";
  expectCfg(showduino_storage_config_check(noVer, strlen(noVer), 4096, 1),
            SHOWDUINO_CFG_MISSING_VERSION, "missing version");
  const char *dup = "{ \"formatVersion\": 1, \"formatVersion\": 1 }";
  expectCfg(showduino_storage_config_check(dup, strlen(dup), 4096, 1),
            SHOWDUINO_CFG_DUPLICATE_VERSION, "duplicate version");
  const char *v99 = "{ \"formatVersion\": 99 }";
  expectCfg(showduino_storage_config_check(v99, strlen(v99), 4096, 1),
            SHOWDUINO_CFG_UNSUPPORTED_VERSION, "unsupported version");
  const char *v0 = "{ \"formatVersion\": 0 }";
  expectCfg(showduino_storage_config_check(v0, strlen(v0), 4096, 1),
            SHOWDUINO_CFG_BAD_VERSION, "zero version");
  char huge[9000];
  memset(huge, 'x', sizeof(huge));
  huge[0] = '{';
  huge[sizeof(huge) - 1] = '\0';
  expectCfg(showduino_storage_config_check(huge, sizeof(huge) - 1, 4096, 1),
            SHOWDUINO_CFG_TOO_LARGE, "oversized json");

  expect(showduino_storage_boot_behaviour_ok("idle") != 0, "boot idle allowed");
  expect(showduino_storage_boot_behaviour_ok("running") == 0, "boot running rejected");
  expect(showduino_storage_boot_behaviour_ok("resume") == 0, "boot resume rejected");
  expect(showduino_storage_production_id_ok("") != 0, "empty production id");
  expect(showduino_storage_production_id_ok("chamber") != 0, "valid production id");
  expect(showduino_storage_production_id_ok("../x") == 0, "bad production id");

  ShowduinoAtomicStep st = SHOWDUINO_ATOMIC_IDLE;
  st = showduino_storage_atomic_next(st, SHOWDUINO_ATOMIC_EV_START);
  expect(st == SHOWDUINO_ATOMIC_VALIDATE_PATH, "atomic start");
  st = showduino_storage_atomic_next(st, SHOWDUINO_ATOMIC_EV_PATH_OK);
  expect(st == SHOWDUINO_ATOMIC_PRESERVE, "atomic path");
  st = showduino_storage_atomic_next(st, SHOWDUINO_ATOMIC_EV_PRESERVE_FAIL);
  expect(st == SHOWDUINO_ATOMIC_WRITE_TEMP, "atomic preserve fail continues");
  st = showduino_storage_atomic_next(st, SHOWDUINO_ATOMIC_EV_WRITE_OK);
  st = showduino_storage_atomic_next(st, SHOWDUINO_ATOMIC_EV_FLUSH_OK);
  st = showduino_storage_atomic_next(st, SHOWDUINO_ATOMIC_EV_VALIDATE_OK);
  st = showduino_storage_atomic_next(st, SHOWDUINO_ATOMIC_EV_REPLACE_OK);
  expect(st == SHOWDUINO_ATOMIC_DONE, "atomic happy path");

  st = SHOWDUINO_ATOMIC_IDLE;
  st = showduino_storage_atomic_next(st, SHOWDUINO_ATOMIC_EV_START);
  st = showduino_storage_atomic_next(st, SHOWDUINO_ATOMIC_EV_PATH_BAD);
  expect(st == SHOWDUINO_ATOMIC_FAIL, "atomic bad path");
  st = showduino_storage_atomic_next(st, SHOWDUINO_ATOMIC_EV_WRITE_OK);
  expect(st == SHOWDUINO_ATOMIC_FAIL, "atomic fail sticky");

  st = SHOWDUINO_ATOMIC_WRITE_TEMP;
  st = showduino_storage_atomic_next(st, SHOWDUINO_ATOMIC_EV_WRITE_FAIL);
  expect(st == SHOWDUINO_ATOMIC_FAIL, "atomic write fail");
  st = SHOWDUINO_ATOMIC_VALIDATE_TEMP;
  st = showduino_storage_atomic_next(st, SHOWDUINO_ATOMIC_EV_VALIDATE_FAIL);
  expect(st == SHOWDUINO_ATOMIC_FAIL, "atomic validate fail");
  st = SHOWDUINO_ATOMIC_REPLACE;
  st = showduino_storage_atomic_next(st, SHOWDUINO_ATOMIC_EV_REPLACE_FAIL);
  expect(st == SHOWDUINO_ATOMIC_FAIL, "atomic replace fail");

  expect(strcmp(showduino_storage_state_name(SHOWDUINO_STORAGE_ONLINE), "ONLINE") == 0, "state online");
  expect(strcmp(showduino_storage_state_name(SHOWDUINO_STORAGE_DEGRADED), "DEGRADED") == 0, "state degraded");
  expect(strcmp(showduino_storage_state_name(SHOWDUINO_STORAGE_READ_ONLY), "READ_ONLY") == 0, "state readonly");
  expect(strcmp(showduino_storage_state_name(SHOWDUINO_STORAGE_OFFLINE), "OFFLINE") == 0, "state offline");
  expect(strcmp(showduino_storage_state_name(SHOWDUINO_STORAGE_FAULT), "FAULT") == 0, "state fault");

  char rot[32];
  expect(showduino_storage_log_rotated_name("system", 1, rot, sizeof(rot)) != 0, "rotate name");
  expect(strcmp(rot, "system-001.log") == 0, "rotate 001");
  expect(showduino_storage_log_rotated_name("system", 12, rot, sizeof(rot)) != 0, "rotate 012");
  expect(strcmp(rot, "system-012.log") == 0, "rotate 012 value");
  expect(showduino_storage_log_rotated_name("../x", 1, rot, sizeof(rot)) == 0, "rotate bad channel");

  if (gFail) {
    printf("%d FAIL\n", gFail);
    return 1;
  }
  printf("ALL PASS\n");
  return 0;
}
