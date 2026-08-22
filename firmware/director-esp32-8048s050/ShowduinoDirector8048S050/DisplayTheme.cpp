#include "DisplayTheme.h"

#include <SD.h>
#include <string.h>

static char s_themeName[32] = "default";
static int s_themeMajor = 0;
static char s_themeLabel[40] = "Default";
static bool s_themeMetaOk = false;

static bool jsonStringValue(const String &body, const char *key, char *out, size_t outLen) {
  if (!key || !out || outLen == 0) return false;
  out[0] = '\0';
  String token = String("\"") + key + "\"";
  const char *start = strstr(body.c_str(), token.c_str());
  if (!start) return false;
  const char *colon = strchr(start + token.length(), ':');
  if (!colon) return false;
  const char *open = strchr(colon + 1, '"');
  if (!open) return false;
  const char *close = strchr(open + 1, '"');
  if (!close) return false;
  size_t n = (size_t)(close - open - 1);
  if (n >= outLen) n = outLen - 1;
  memcpy(out, open + 1, n);
  out[n] = '\0';
  return true;
}

void DisplayTheme::begin() {
  if (!s_themeName[0]) strncpy(s_themeName, "default", sizeof(s_themeName) - 1);
  s_themeMetaOk = validateCurrentTheme();
}

const char *DisplayTheme::currentTheme() { return s_themeName; }

void DisplayTheme::setTheme(const char *name) {
  if (!name || !name[0]) return;
  strncpy(s_themeName, name, sizeof(s_themeName) - 1);
  s_themeName[sizeof(s_themeName) - 1] = '\0';
  s_themeMetaOk = validateCurrentTheme();
  Serial.printf("[Theme] set → %s meta_ok=%u\n", s_themeName, (unsigned)s_themeMetaOk);
}

int DisplayTheme::themeMajor() { return s_themeMajor; }
const char *DisplayTheme::themeDisplayName() { return s_themeLabel; }
bool DisplayTheme::isValid() { return s_themeMetaOk; }

void DisplayTheme::buildThemeRoot(char *out, size_t outLen) {
  snprintf(out, outLen, "/showduino/ui/themes/%s", s_themeName);
}

bool DisplayTheme::parseThemeJson(const char *path) {
  if (!path || !SD.exists(path)) {
    Serial.printf("[Theme] missing %s\n", path ? path : "(null)");
    return false;
  }
  File f = SD.open(path, FILE_READ);
  if (!f) {
    Serial.println("[Theme] open theme.json failed");
    return false;
  }
  String body;
  body.reserve((size_t)f.size() + 1);
  while (f.available()) body += (char)f.read();
  f.close();

  /* Minimal parse: "version": N and "resolution":"WxH" and optional "name".
     Unknown fields ignored (forward compatible). */
  int version = 0;
  const char *v = strstr(body.c_str(), "\"version\"");
  if (v) {
    v = strchr(v, ':');
    if (v) version = atoi(v + 1);
  }
  s_themeMajor = version;

  if (version != DISPLAY_THEME_MAJOR) {
    Serial.printf("[Theme] reject major version=%d (need %d)\n", version, DISPLAY_THEME_MAJOR);
    return false;
  }

  char wantRes[24];
  snprintf(wantRes, sizeof(wantRes), "%ux%u", (unsigned)DISPLAY_WIDTH, (unsigned)DISPLAY_HEIGHT);
  char gotRes[24];
  if (!jsonStringValue(body, "resolution", gotRes, sizeof(gotRes))) {
    Serial.println("[Theme] reject — resolution field missing");
    return false;
  }
  if (strcmp(gotRes, wantRes) != 0) {
    Serial.printf("[Theme] reject resolution=%s (need %s)\n", gotRes, wantRes);
    return false;
  }

  char label[sizeof(s_themeLabel)];
  if (jsonStringValue(body, "name", label, sizeof(label))) {
    strncpy(s_themeLabel, label, sizeof(s_themeLabel) - 1);
    s_themeLabel[sizeof(s_themeLabel) - 1] = '\0';
  }

  Serial.printf("[Theme] ok name=%s version=%d res=%s\n", s_themeLabel, s_themeMajor, wantRes);
  return true;
}

bool DisplayTheme::validateCurrentTheme() {
  char path[96];
  char root[72];
  buildThemeRoot(root, sizeof(root));
  snprintf(path, sizeof(path), "%s/theme.json", root);
  return parseThemeJson(path);
}

bool DisplayTheme::resolvePageImage(DisplayPageId /*page*/, const char * /*imageBasename*/,
                                    char *out, size_t outLen) {
  if (out && outLen) out[0] = '\0';
  return false;
}
