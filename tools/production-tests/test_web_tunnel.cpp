#include <cstdio>
#include <cstring>

#include "../../protocol/showduino_web_tunnel.h"

static int failures = 0;

static void expect(bool condition, const char *name) {
  std::printf("%s  %s\n", condition ? "PASS" : "FAIL", name);
  if (!condition) ++failures;
}

int main() {
  ShowduinoWebrHeader hdr{};

  expect(showduino_webr_parse_header("WEBR:200:12:application/json", &hdr) == 1,
         "parse WEBR with mime");
  expect(hdr.status == 200, "status 200");
  expect(hdr.bodyLen == 12, "bodyLen 12");
  expect(std::strcmp(hdr.mime, "application/json") == 0, "mime json");
  expect(hdr.truncated == 0, "not truncated");

  expect(showduino_webr_parse_header("WEBR:503:0", &hdr) == 1, "parse empty body");
  expect(hdr.status == 503 && hdr.bodyLen == 0, "503 empty");

  expect(showduino_webr_parse_header("WEBR:200:999999", &hdr) == 1, "oversized capped");
  expect(hdr.bodyLen == SHOWDUINO_WEB_TUNNEL_BODY_MAX, "cap BODY_MAX");
  expect(hdr.truncated == 1, "truncated flag");

  expect(showduino_webr_parse_header("WEB/GET/api/system", &hdr) == 0, "reject WEB/ as WEBR");
  expect(showduino_webr_parse_header("WEBR:xx:1", &hdr) == 0, "reject bad status");
  expect(showduino_webr_parse_header("WEBR:200", &hdr) == 0, "reject truncated header");
  expect(showduino_webr_parse_header("WEBR:200:4:text/plain", &hdr) == 1, "mime text/plain");
  expect(std::strcmp(hdr.mime, "text/plain") == 0, "mime value");

  char cmd[64];
  char rid[48];
  const char *json =
      "{\"cmd\":\"SHOW:START\",\"requestId\":\"req-1\",\"source\":\"studio\"}";
  expect(showduino_web_json_string_field(json, "cmd", cmd, sizeof(cmd)) == 1, "extract cmd");
  expect(std::strcmp(cmd, "SHOW:START") == 0, "cmd value");
  expect(showduino_web_json_string_field(json, "requestId", rid, sizeof(rid)) == 1,
         "extract requestId");
  expect(std::strcmp(rid, "req-1") == 0, "requestId value");
  expect(showduino_web_json_string_field(json, "missing", cmd, sizeof(cmd)) == 0,
         "missing field");

  expect(showduino_web_request_id_ok("abc-123") == 1, "rid ok");
  expect(showduino_web_request_id_ok("a:b.c_1") == 1, "rid charset");
  expect(showduino_web_request_id_ok("") == 0, "rid empty");
  expect(showduino_web_request_id_ok("bad id") == 0, "rid space rejected");
  expect(showduino_web_request_id_ok("x\"y") == 0, "rid quote rejected");

  ShowduinoWebDupRing ring;
  showduino_web_dup_init(&ring);
  const char *found = nullptr;
  int st = 0;
  expect(showduino_web_dup_find(&ring, "req-1", &found, &st) == 0, "dup miss");
  showduino_web_dup_remember(&ring, "req-1", 200,
                             "{\"ok\":true,\"lifecycle\":\"accepted\"}");
  expect(showduino_web_dup_find(&ring, "req-1", &found, &st) == 1, "dup hit");
  expect(st == 200, "dup status");
  expect(found && std::strstr(found, "accepted") != nullptr, "dup body");

  /* Second remember of same id still finds one of the remembered copies. */
  showduino_web_dup_remember(&ring, "req-1", 200,
                             "{\"ok\":true,\"lifecycle\":\"duplicate\"}");
  expect(showduino_web_dup_find(&ring, "req-1", &found, &st) == 1, "dup still hit");

  /* Fill ring beyond slots — oldest evicted. */
  char idbuf[16];
  for (unsigned i = 0; i < SHOWDUINO_WEB_DUP_SLOTS + 2; i++) {
    std::snprintf(idbuf, sizeof(idbuf), "id-%u", i);
    showduino_web_dup_remember(&ring, idbuf, 200, "{\"ok\":true}");
  }
  expect(showduino_web_dup_find(&ring, "id-0", &found, &st) == 0, "oldest evicted");
  expect(showduino_web_dup_find(&ring, "id-15", &found, &st) == 1 ||
             showduino_web_dup_find(&ring, "id-16", &found, &st) == 1,
         "recent retained");

  /* Fragmented header recovery: incomplete line is not a valid WEBR. */
  expect(showduino_webr_parse_header("WEBR:20", &hdr) == 0, "fragment rejected");
  expect(showduino_webr_parse_header("WEBR:200:3\n", &hdr) == 1, "header before body");
  expect(hdr.bodyLen == 3, "body 3 after header");

  /* Interleaved status traffic must not parse as WEBR. */
  expect(showduino_webr_parse_header("SRv1:STATUS:RUNNING", &hdr) == 0,
         "status line not WEBR");
  expect(showduino_webr_parse_header("SHOW:STATE:RUNNING", &hdr) == 0,
         "show state not WEBR");

  return failures ? 1 : 0;
}
