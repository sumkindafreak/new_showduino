#include <cstdio>
#include <cstring>

#include "../../protocol/showduino_deploy.h"

static int failures = 0;

static void expect(bool condition, const char *name) {
  std::printf("%s  %s\n", condition ? "PASS" : "FAIL", name);
  if (!condition) ++failures;
}

int main() {
  ShowduinoWebBodyHeader hdr{};
  expect(showduino_web_body_parse_header("WEB/BODY:12:POST:/api/productions/deploy/begin", &hdr) == 1,
         "parses WEB/BODY header");
  expect(hdr.length == 12, "length");
  expect(std::strcmp(hdr.method, "POST") == 0, "method");
  expect(std::strcmp(hdr.path, "/api/productions/deploy/begin") == 0, "path");
  expect(showduino_web_body_parse_header("WEB/POST/api/studio-timeline", &hdr) == 0,
         "rejects WEB/POST as WEB/BODY");

  const char *json = "{\"bytes\":48,\"crc32\":123}";
  uint32_t bytes = 0, crc = 0;
  expect(showduino_json_u32_field(json, "bytes", &bytes) == 1 && bytes == 48, "json bytes");
  expect(showduino_json_u32_field(json, "crc32", &crc) == 1 && crc == 123, "json crc32");

  unsigned char out[4];
  size_t n = 0;
  expect(showduino_hex_decode("4869", out, sizeof(out), &n) == 1 && n == 2 && out[0] == 'H' && out[1] == 'i',
         "hex decode");
  const char sample[] = "Hi";
  expect(showduino_crc32_ieee(sample, 2) != 0, "crc32 non-zero");
  return failures ? 1 : 0;
}
