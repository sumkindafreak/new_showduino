#ifndef SHOWDUINO_UPDATE_GITHUB_H
#define SHOWDUINO_UPDATE_GITHUB_H

/*
 * GitHub Release + showduino-release-v1 helpers for Comms discovery.
 * Host-testable. Do not add a second OTA stack.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "showduino_update_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHOWDUINO_GITHUB_ASSET_MAX 8
#define SHOWDUINO_GITHUB_ASSET_NAME_MAX 63
#define SHOWDUINO_GITHUB_ASSET_URL_MAX 191

typedef struct ShowduinoGithubReleaseMeta {
  char tag[32];
  char tag_norm[32];
  char name[48];
  char html_url[128];
} ShowduinoGithubReleaseMeta;

typedef struct ShowduinoGithubAsset {
  char name[SHOWDUINO_GITHUB_ASSET_NAME_MAX + 1];
  char url[SHOWDUINO_GITHUB_ASSET_URL_MAX + 1];
} ShowduinoGithubAsset;

typedef struct ShowduinoGithubAssetList {
  uint8_t count;
  ShowduinoGithubAsset assets[SHOWDUINO_GITHUB_ASSET_MAX];
} ShowduinoGithubAssetList;

static inline const char *showduino_json_skip_ws(const char *s) {
  while (s && (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t')) s++;
  return s;
}

static inline const char *showduino_json_match_end(const char *start, char open_ch, char close_ch) {
  int depth = 0;
  int in_str = 0;
  const char *s;
  if (!start || *start != open_ch) return NULL;
  for (s = start; *s; s++) {
    if (in_str) {
      if (*s == '\\' && s[1]) {
        s++;
        continue;
      }
      if (*s == '"') in_str = 0;
      continue;
    }
    if (*s == '"') {
      in_str = 1;
      continue;
    }
    if (*s == open_ch) depth++;
    else if (*s == close_ch) {
      depth--;
      if (depth == 0) return s;
    }
  }
  return NULL;
}

static inline int showduino_json_copy_unescaped(const char *src, int len, char *dst, size_t cap) {
  int i = 0;
  size_t used = 0;
  if (!dst || cap == 0) return 0;
  dst[0] = 0;
  if (!src || len < 0) return 0;
  while (i < len && used + 1 < cap) {
    char c = src[i++];
    if (c == '\\' && i < len) {
      char n = src[i++];
      if (n == 'n' || n == 'r' || n == 't') c = ' ';
      else c = n;
    }
    if ((unsigned char)c < 0x20) c = ' ';
    dst[used++] = c;
  }
  dst[used] = 0;
  return 1;
}

static inline const char *showduino_json_find_key_range(const char *json, const char *end,
                                                        const char *key) {
  char pat[48];
  size_t n;
  const char *p;
  if (!json || !key || !key[0]) return NULL;
  n = strlen(key);
  if (n + 3 >= sizeof(pat)) return NULL;
  pat[0] = '"';
  memcpy(pat + 1, key, n);
  pat[n + 1] = '"';
  pat[n + 2] = 0;
  p = json;
  while (p && *p && (!end || p < end)) {
    const char *hit = strstr(p, pat);
    if (!hit || (end && hit >= end)) return NULL;
    return hit + (int)n + 2;
  }
  return NULL;
}

static inline int showduino_json_string_value(const char *after_key, const char *end,
                                              char *out, size_t cap) {
  const char *p = showduino_json_skip_ws(after_key);
  const char *q1;
  const char *q2;
  if (!p || *p != ':') return 0;
  p = showduino_json_skip_ws(p + 1);
  if (!p || *p != '"') return 0;
  q1 = p + 1;
  q2 = q1;
  while (*q2 && (!end || q2 < end)) {
    if (*q2 == '\\' && q2[1]) {
      q2 += 2;
      continue;
    }
    if (*q2 == '"') break;
    q2++;
  }
  if (!*q2 || (end && q2 >= end)) return 0;
  return showduino_json_copy_unescaped(q1, (int)(q2 - q1), out, cap);
}

static inline int showduino_json_bool_value(const char *after_key, const char *end) {
  const char *p = showduino_json_skip_ws(after_key);
  if (!p || *p != ':') return -1;
  p = showduino_json_skip_ws(p + 1);
  if (!p || (end && p >= end)) return -1;
  if (strncmp(p, "true", 4) == 0) return 1;
  if (strncmp(p, "false", 5) == 0) return 0;
  return -1;
}

static inline uint32_t showduino_json_u32_value(const char *after_key, const char *end) {
  const char *p = showduino_json_skip_ws(after_key);
  uint32_t v = 0;
  if (!p || *p != ':') return 0;
  p = showduino_json_skip_ws(p + 1);
  if (!p || (end && p >= end)) return 0;
  if (*p == '"') {
    p++;
    while (*p && *p != '"' && (!end || p < end)) {
      if (*p >= '0' && *p <= '9') v = v * 10u + (uint32_t)(*p - '0');
      else break;
      p++;
    }
    return v;
  }
  while (*p && (!end || p < end) && *p >= '0' && *p <= '9') {
    v = v * 10u + (uint32_t)(*p - '0');
    p++;
  }
  return v;
}

static inline int showduino_json_get_string(const char *json, const char *end,
                                            const char *key, char *out, size_t cap) {
  const char *k = showduino_json_find_key_range(json, end, key);
  if (!k) {
    if (out && cap) out[0] = 0;
    return 0;
  }
  return showduino_json_string_value(k, end, out, cap);
}

static inline int showduino_json_get_bool(const char *json, const char *end, const char *key) {
  const char *k = showduino_json_find_key_range(json, end, key);
  if (!k) return -1;
  return showduino_json_bool_value(k, end);
}

static inline uint32_t showduino_json_get_u32(const char *json, const char *end, const char *key) {
  const char *k = showduino_json_find_key_range(json, end, key);
  if (!k) return 0;
  return showduino_json_u32_value(k, end);
}

static inline int showduino_github_first_release(const char *json,
                                                 ShowduinoGithubReleaseMeta *out) {
  const char *p;
  if (!json || !out) return 0;
  memset(out, 0, sizeof(*out));
  p = json;
  while (1) {
    const char *tag_key = strstr(p, "\"tag_name\"");
    const char *obj;
    const char *obj_end;
    char tag[32];
    int draft;
    if (!tag_key) return 0;
    obj = tag_key;
    while (obj > json && *obj != '{') obj--;
    obj_end = showduino_json_match_end(obj, '{', '}');
    if (!obj_end) obj_end = tag_key + 2400;
    draft = showduino_json_get_bool(obj, obj_end, "draft");
    if (draft == 1) {
      p = tag_key + 10;
      continue;
    }
    if (!showduino_json_get_string(obj, obj_end, "tag_name", tag, sizeof(tag))) {
      p = tag_key + 10;
      continue;
    }
    showduino_update_copy(out->tag, sizeof(out->tag), tag);
    showduino_update_copy(out->tag_norm, sizeof(out->tag_norm), showduino_version_skip_v(tag));
    if (!showduino_json_get_string(obj, obj_end, "name", out->name, sizeof(out->name))) {
      showduino_update_copy(out->name, sizeof(out->name), tag);
    }
    showduino_json_get_string(obj, obj_end, "html_url", out->html_url, sizeof(out->html_url));
    return 1;
  }
}

static inline int showduino_github_parse_assets(const char *json, ShowduinoGithubAssetList *out) {
  const char *arr_key;
  const char *arr;
  const char *arr_end;
  const char *p;
  if (!json || !out) return 0;
  memset(out, 0, sizeof(*out));
  arr_key = strstr(json, "\"assets\"");
  if (!arr_key) return 1;
  arr = strchr(arr_key, '[');
  if (!arr) return 1;
  arr_end = showduino_json_match_end(arr, '[', ']');
  if (!arr_end) return 0;
  p = arr + 1;
  while (out->count < SHOWDUINO_GITHUB_ASSET_MAX) {
    const char *obj;
    const char *obj_end;
    ShowduinoGithubAsset *a;
    while (*p && p < arr_end && *p != '{') p++;
    if (!*p || p >= arr_end) break;
    obj = p;
    obj_end = showduino_json_match_end(obj, '{', '}');
    if (!obj_end || obj_end > arr_end) break;
    a = &out->assets[out->count];
    showduino_json_get_string(obj, obj_end, "name", a->name, sizeof(a->name));
    showduino_json_get_string(obj, obj_end, "browser_download_url", a->url, sizeof(a->url));
    if (a->name[0] && a->url[0]) out->count++;
    p = obj_end + 1;
  }
  return 1;
}

static inline const char *showduino_github_find_asset(const ShowduinoGithubAssetList *list,
                                                      const char *filename) {
  uint8_t i;
  if (!list || !filename || !filename[0]) return NULL;
  for (i = 0; i < list->count; i++) {
    if (strcmp(list->assets[i].name, filename) == 0) return list->assets[i].url;
  }
  return NULL;
}

static inline const char *showduino_github_find_manifest_asset(const ShowduinoGithubAssetList *list,
                                                               const char **name_out) {
  uint8_t i;
  if (name_out) *name_out = NULL;
  if (!list) return NULL;
  for (i = 0; i < list->count; i++) {
    const char *n = list->assets[i].name;
    size_t len = strlen(n);
    if (len > 14 && strcmp(n + len - 14, ".manifest.json") == 0) {
      if (name_out) *name_out = n;
      return list->assets[i].url;
    }
  }
  return NULL;
}

static inline int showduino_github_download_url(const char *tag, const char *filename,
                                                char *out, size_t cap) {
  if (!out || cap < 48 || !tag || !tag[0] || !filename || !filename[0]) return 0;
  if (snprintf(out, cap, "%s%s/%s", SHOWDUINO_GITHUB_DOWNLOAD_PREFIX, tag, filename) < 0) {
    out[0] = 0;
    return 0;
  }
  out[cap - 1] = 0;
  return out[0] != 0;
}

static inline int showduino_github_tag_api_url(const char *tag, char *out, size_t cap) {
  if (!out || cap < 48 || !tag || !tag[0]) return 0;
  if (snprintf(out, cap, "%s%s", SHOWDUINO_GITHUB_RELEASE_TAG_API_PREFIX, tag) < 0) {
    out[0] = 0;
    return 0;
  }
  out[cap - 1] = 0;
  return 1;
}

static inline int showduino_release_parse_component_obj(const char *obj, const char *obj_end,
                                                        ShowduinoReleaseComponent *c) {
  char tmp[24];
  int capable;
  if (!obj || !c) return 0;
  memset(c, 0, sizeof(*c));
  if (!showduino_json_get_string(obj, obj_end, "role", c->role, sizeof(c->role))) return 0;
  showduino_json_get_string(obj, obj_end, "id", c->id, sizeof(c->id));
  showduino_json_get_string(obj, obj_end, "firmware", c->firmware, sizeof(c->firmware));
  showduino_json_get_string(obj, obj_end, "hardwareId", c->hardware_id, sizeof(c->hardware_id));
  if (!c->hardware_id[0]) {
    showduino_json_get_string(obj, obj_end, "hardware_id", c->hardware_id, sizeof(c->hardware_id));
  }
  showduino_json_get_string(obj, obj_end, "filename", c->filename, sizeof(c->filename));
  showduino_json_get_string(obj, obj_end, "sha256", c->sha256, sizeof(c->sha256));
  c->size = showduino_json_get_u32(obj, obj_end, "size");
  capable = showduino_json_get_bool(obj, obj_end, "otaCapable");
  if (capable < 0) capable = showduino_json_get_bool(obj, obj_end, "ota_capable");
  c->ota_capable = capable == 1 ? 1 : 0;
  if (showduino_json_get_string(obj, obj_end, "updatePolicy", tmp, sizeof(tmp))) {
    c->one_at_a_time = (strcmp(tmp, SHOWDUINO_UPDATE_POLICY_ONE_AT_TIME) == 0) ? 1 : 0;
  }
  return showduino_update_role_ok(c->role);
}

static inline int showduino_release_manifest_parse_json(const char *json,
                                                        ShowduinoReleaseManifest *m) {
  const char *arr;
  const char *arr_end;
  const char *p;
  char tmp[24];
  int schema_ver;
  int ota_install;
  if (!json || !m) return 0;
  showduino_release_manifest_clear(m);
  if (!showduino_json_get_string(json, NULL, "schema", m->schema, sizeof(m->schema))) return 0;
  schema_ver = (int)showduino_json_get_u32(json, NULL, "schemaVersion");
  if (!schema_ver) schema_ver = (int)showduino_json_get_u32(json, NULL, "schema_version");
  m->schema_version = schema_ver;
  showduino_json_get_string(json, NULL, "product", m->product, sizeof(m->product));
  if (!showduino_json_get_string(json, NULL, "version", m->product_version, sizeof(m->product_version))) {
    showduino_json_get_string(json, NULL, "product_version", m->product_version,
                              sizeof(m->product_version));
  }
  showduino_json_get_string(json, NULL, "protocol", m->protocol, sizeof(m->protocol));
  m->shdo = (int)showduino_json_get_u32(json, NULL, "shdo");
  ota_install = showduino_json_get_bool(json, NULL, "otaInstall");
  if (ota_install < 0) ota_install = showduino_json_get_bool(json, NULL, "ota_install");
  m->ota_install = ota_install == 1 ? 1 : 0;
  (void)tmp;
  arr = strstr(json, "\"components\"");
  if (!arr) return showduino_release_manifest_valid(m);
  arr = strchr(arr, '[');
  if (!arr) return 0;
  arr_end = showduino_json_match_end(arr, '[', ']');
  if (!arr_end) return 0;
  p = arr + 1;
  while (m->component_count < SHOWDUINO_UPDATE_MAX_COMPONENTS) {
    const char *obj;
    const char *obj_end;
    ShowduinoReleaseComponent c;
    while (*p && p < arr_end && *p != '{') p++;
    if (!*p || p >= arr_end) break;
    obj = p;
    obj_end = showduino_json_match_end(obj, '{', '}');
    if (!obj_end || obj_end > arr_end) break;
    if (showduino_release_parse_component_obj(obj, obj_end, &c)) {
      m->components[m->component_count++] = c;
    }
    p = obj_end + 1;
  }
  return showduino_release_manifest_valid(m);
}

static inline const ShowduinoReleaseComponent *showduino_release_find_comms(
    const ShowduinoReleaseManifest *m) {
  uint8_t i;
  if (!m) return NULL;
  for (i = 0; i < m->component_count; i++) {
    if (showduino_update_is_comms_role(m->components[i].role)) return &m->components[i];
  }
  return NULL;
}

static inline int showduino_comms_candidate_from_component(const ShowduinoReleaseComponent *comp,
                                                           ShowduinoOtaCandidate *c) {
  if (!comp || !c) return 0;
  memset(c, 0, sizeof(*c));
  showduino_update_copy(c->role, sizeof(c->role), comp->role);
  showduino_update_copy(c->hardware_id, sizeof(c->hardware_id), comp->hardware_id);
  showduino_update_copy(c->firmware, sizeof(c->firmware), comp->firmware);
  showduino_update_copy(c->filename, sizeof(c->filename),
                        comp->filename[0] ? comp->filename : SHOWDUINO_COMMS_BIN_FILENAME);
  showduino_update_copy(c->sha256, sizeof(c->sha256), comp->sha256);
  c->size = comp->size;
  c->ota_capable = comp->ota_capable;
  return 1;
}

static inline int showduino_comms_resolve_bin_url(const ShowduinoOtaCandidate *c,
                                                  const ShowduinoGithubAssetList *assets,
                                                  const char *tag,
                                                  char *url, size_t cap) {
  const char *found;
  const char *filename;
  if (!c || !url || cap == 0) return 0;
  url[0] = 0;
  filename = c->filename[0] ? c->filename : SHOWDUINO_COMMS_BIN_FILENAME;
  found = showduino_github_find_asset(assets, filename);
  if (found && found[0]) {
    showduino_update_copy(url, cap, found);
    return 1;
  }
  if (!tag || !tag[0]) return 0;
  return showduino_github_download_url(tag, filename, url, cap);
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_UPDATE_GITHUB_H */
