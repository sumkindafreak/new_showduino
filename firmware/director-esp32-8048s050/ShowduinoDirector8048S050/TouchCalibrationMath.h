#ifndef SHOWDUINO_TOUCH_CALIBRATION_MATH_H
#define SHOWDUINO_TOUCH_CALIBRATION_MATH_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Director-local 5-point affine touch calibration.
 * Pure C, no LVGL / Arduino / NVS. Host-testable.
 *
 * screenX = a*rawX + b*rawY + c
 * screenY = d*rawX + e*rawY + f
 */

#ifndef TOUCH_CAL_X_LEFT
#define TOUCH_CAL_X_LEFT  790
#endif
#ifndef TOUCH_CAL_X_RIGHT
#define TOUCH_CAL_X_RIGHT 18
#endif
#ifndef TOUCH_CAL_Y_TOP
#define TOUCH_CAL_Y_TOP   465
#endif
#ifndef TOUCH_CAL_Y_BOT
#define TOUCH_CAL_Y_BOT   25
#endif

#ifndef SHOWDUINO_TOUCH_CAL_WIDTH
#define SHOWDUINO_TOUCH_CAL_WIDTH  800
#endif
#ifndef SHOWDUINO_TOUCH_CAL_HEIGHT
#define SHOWDUINO_TOUCH_CAL_HEIGHT 480
#endif

#define SHOWDUINO_TOUCH_CAL_MAGIC      0x53445443u /* 'SDTC' */
#define SHOWDUINO_TOUCH_CAL_VERSION    1u
#define SHOWDUINO_TOUCH_CAL_POINT_N    5
#define SHOWDUINO_TOUCH_CAL_SAMPLES    9

#define SHOWDUINO_TOUCH_CAL_INSET_X    60
#define SHOWDUINO_TOUCH_CAL_INSET_Y    60

#define SHOWDUINO_TOUCH_CAL_MAX_RESIDUAL_PX   36.0
#define SHOWDUINO_TOUCH_CAL_MAX_RMS_PX        22.0
#define SHOWDUINO_TOUCH_CAL_MAX_CENTRE_PX     28.0
#define SHOWDUINO_TOUCH_CAL_MIN_RAW_SPAN      80.0
#define SHOWDUINO_TOUCH_CAL_MIN_ABS_DET       1.0e-4
#define SHOWDUINO_TOUCH_CAL_MIN_AXIS_SCALE    0.15
#define SHOWDUINO_TOUCH_CAL_MAX_AXIS_SCALE    8.0

typedef enum ShowduinoTouchCalMode {
  SHOWDUINO_TOUCH_CAL_MODE_FACTORY = 0,
  SHOWDUINO_TOUCH_CAL_MODE_NVS = 1
} ShowduinoTouchCalMode;

typedef enum ShowduinoTouchCalFail {
  SHOWDUINO_TOUCH_CAL_OK = 0,
  SHOWDUINO_TOUCH_CAL_FAIL_POINTS,
  SHOWDUINO_TOUCH_CAL_FAIL_SPAN,
  SHOWDUINO_TOUCH_CAL_FAIL_SINGULAR,
  SHOWDUINO_TOUCH_CAL_FAIL_RESIDUAL,
  SHOWDUINO_TOUCH_CAL_FAIL_SCALE,
  SHOWDUINO_TOUCH_CAL_FAIL_NONFINITE,
  SHOWDUINO_TOUCH_CAL_FAIL_MAGIC,
  SHOWDUINO_TOUCH_CAL_FAIL_VERSION,
  SHOWDUINO_TOUCH_CAL_FAIL_SIZE,
  SHOWDUINO_TOUCH_CAL_FAIL_GEOMETRY,
  SHOWDUINO_TOUCH_CAL_FAIL_CHECKSUM
} ShowduinoTouchCalFail;

#pragma pack(push, 1)
typedef struct ShowduinoTouchCalibrationRecord {
  uint32_t magic;
  uint16_t version;
  uint16_t width;
  uint16_t height;
  uint16_t flags;
  float a;
  float b;
  float c;
  float d;
  float e;
  float f;
  uint32_t checksum;
} ShowduinoTouchCalibrationRecord;
#pragma pack(pop)

typedef struct ShowduinoTouchCalPoint {
  float rawX;
  float rawY;
  float screenX;
  float screenY;
} ShowduinoTouchCalPoint;

typedef struct ShowduinoTouchCalSampleBuf {
  int32_t x[SHOWDUINO_TOUCH_CAL_SAMPLES];
  int32_t y[SHOWDUINO_TOUCH_CAL_SAMPLES];
  uint8_t n;
} ShowduinoTouchCalSampleBuf;

static inline int32_t showduino_touch_cal_map_axis(int32_t v, int32_t inA, int32_t inB,
                                                    int32_t outMax) {
  if (inA == inB) return 0;
  int32_t mapped = (v - inA) * outMax / (inB - inA);
  if (mapped < 0) mapped = 0;
  if (mapped > outMax) mapped = outMax;
  return mapped;
}

static inline void showduino_touch_cal_factory_map(int32_t rawX, int32_t rawY,
                                                   int32_t width, int32_t height,
                                                   int32_t *screenX, int32_t *screenY) {
  if (width < 1) width = 1;
  if (height < 1) height = 1;
  if (screenX) {
    *screenX = showduino_touch_cal_map_axis(rawX, TOUCH_CAL_X_LEFT, TOUCH_CAL_X_RIGHT,
                                            width - 1);
  }
  if (screenY) {
    *screenY = showduino_touch_cal_map_axis(rawY, TOUCH_CAL_Y_TOP, TOUCH_CAL_Y_BOT,
                                            height - 1);
  }
}

static inline void showduino_touch_cal_target(uint8_t index, uint16_t width, uint16_t height,
                                              float *sx, float *sy) {
  const float insetX = (float)SHOWDUINO_TOUCH_CAL_INSET_X;
  const float insetY = (float)SHOWDUINO_TOUCH_CAL_INSET_Y;
  const float right = (float)width - 1.0f - insetX;
  const float bot = (float)height - 1.0f - insetY;
  const float cx = ((float)width - 1.0f) * 0.5f;
  const float cy = ((float)height - 1.0f) * 0.5f;
  float x = cx, y = cy;
  switch (index) {
    case 0: x = insetX; y = insetY; break;
    case 1: x = right;  y = insetY; break;
    case 2: x = right;  y = bot;    break;
    case 3: x = insetX; y = bot;    break;
    default: x = cx; y = cy; break;
  }
  if (sx) *sx = x;
  if (sy) *sy = y;
}

static inline uint32_t showduino_touch_cal_fnv1a(const uint8_t *data, size_t len) {
  uint32_t h = 2166136261u;
  size_t i;
  for (i = 0; i < len; i++) {
    h ^= (uint32_t)data[i];
    h *= 16777619u;
  }
  return h;
}

static inline uint32_t showduino_touch_cal_checksum(const ShowduinoTouchCalibrationRecord *rec) {
  if (!rec) return 0;
  return showduino_touch_cal_fnv1a((const uint8_t *)rec,
                                   offsetof(ShowduinoTouchCalibrationRecord, checksum));
}

static inline void showduino_touch_cal_seal(ShowduinoTouchCalibrationRecord *rec) {
  if (!rec) return;
  rec->magic = SHOWDUINO_TOUCH_CAL_MAGIC;
  rec->version = (uint16_t)SHOWDUINO_TOUCH_CAL_VERSION;
  rec->checksum = showduino_touch_cal_checksum(rec);
}

static inline int showduino_touch_cal_finite6(float a, float b, float c,
                                              float d, float e, float f) {
  return isfinite((double)a) && isfinite((double)b) && isfinite((double)c) &&
         isfinite((double)d) && isfinite((double)e) && isfinite((double)f);
}

static inline void showduino_touch_cal_apply(const ShowduinoTouchCalibrationRecord *rec,
                                             float rawX, float rawY,
                                             float *screenX, float *screenY) {
  float sx = 0.0f, sy = 0.0f;
  if (rec) {
    sx = rec->a * rawX + rec->b * rawY + rec->c;
    sy = rec->d * rawX + rec->e * rawY + rec->f;
  }
  if (screenX) *screenX = sx;
  if (screenY) *screenY = sy;
}

static inline void showduino_touch_cal_apply_i(const ShowduinoTouchCalibrationRecord *rec,
                                               int32_t rawX, int32_t rawY,
                                               int32_t width, int32_t height,
                                               int32_t *screenX, int32_t *screenY) {
  float sx = 0.0f, sy = 0.0f;
  int32_t ix, iy;
  showduino_touch_cal_apply(rec, (float)rawX, (float)rawY, &sx, &sy);
  ix = (int32_t)lroundf(sx);
  iy = (int32_t)lroundf(sy);
  if (width < 1) width = 1;
  if (height < 1) height = 1;
  if (ix < 0) ix = 0;
  if (iy < 0) iy = 0;
  if (ix > width - 1) ix = width - 1;
  if (iy > height - 1) iy = height - 1;
  if (screenX) *screenX = ix;
  if (screenY) *screenY = iy;
}

static inline int showduino_touch_cal_solve3(double A[3][3], const double b[3], double x[3]) {
  double M[3][4];
  int i, j, k, piv;
  double maxv, tmp, fac;
  for (i = 0; i < 3; i++) {
    M[i][0] = A[i][0];
    M[i][1] = A[i][1];
    M[i][2] = A[i][2];
    M[i][3] = b[i];
  }
  for (i = 0; i < 3; i++) {
    piv = i;
    maxv = fabs(M[i][i]);
    for (k = i + 1; k < 3; k++) {
      if (fabs(M[k][i]) > maxv) {
        maxv = fabs(M[k][i]);
        piv = k;
      }
    }
    if (maxv < 1.0e-12) return 0;
    if (piv != i) {
      for (j = 0; j < 4; j++) {
        tmp = M[i][j];
        M[i][j] = M[piv][j];
        M[piv][j] = tmp;
      }
    }
    for (k = i + 1; k < 3; k++) {
      fac = M[k][i] / M[i][i];
      for (j = i; j < 4; j++) M[k][j] -= fac * M[i][j];
    }
  }
  for (i = 2; i >= 0; i--) {
    tmp = M[i][3];
    for (j = i + 1; j < 3; j++) tmp -= M[i][j] * x[j];
    if (fabs(M[i][i]) < 1.0e-12) return 0;
    x[i] = tmp / M[i][i];
  }
  return 1;
}

static inline int showduino_touch_cal_ls_axis(const ShowduinoTouchCalPoint *pts, int n,
                                              int useY, double coeff[3]) {
  double ATA[3][3];
  double ATb[3];
  int i, r, c;
  memset(ATA, 0, sizeof(ATA));
  memset(ATb, 0, sizeof(ATb));
  if (n < 3) return 0;
  for (i = 0; i < n; i++) {
    const double xi = (double)pts[i].rawX;
    const double yi = (double)pts[i].rawY;
    const double zi = useY ? (double)pts[i].screenY : (double)pts[i].screenX;
    const double row[3] = { xi, yi, 1.0 };
    for (r = 0; r < 3; r++) {
      ATb[r] += row[r] * zi;
      for (c = 0; c < 3; c++) ATA[r][c] += row[r] * row[c];
    }
  }
  return showduino_touch_cal_solve3(ATA, ATb, coeff);
}

static inline int showduino_touch_cal_solve(const ShowduinoTouchCalPoint *pts, int n,
                                            uint16_t width, uint16_t height,
                                            ShowduinoTouchCalibrationRecord *out) {
  double cx[3], cy[3];
  if (!pts || !out || n != SHOWDUINO_TOUCH_CAL_POINT_N) return SHOWDUINO_TOUCH_CAL_FAIL_POINTS;
  memset(out, 0, sizeof(*out));
  out->magic = SHOWDUINO_TOUCH_CAL_MAGIC;
  out->version = (uint16_t)SHOWDUINO_TOUCH_CAL_VERSION;
  out->width = width;
  out->height = height;
  out->flags = 0;
  if (!showduino_touch_cal_ls_axis(pts, n, 0, cx)) return SHOWDUINO_TOUCH_CAL_FAIL_SINGULAR;
  if (!showduino_touch_cal_ls_axis(pts, n, 1, cy)) return SHOWDUINO_TOUCH_CAL_FAIL_SINGULAR;
  out->a = (float)cx[0];
  out->b = (float)cx[1];
  out->c = (float)cx[2];
  out->d = (float)cy[0];
  out->e = (float)cy[1];
  out->f = (float)cy[2];
  if (!showduino_touch_cal_finite6(out->a, out->b, out->c, out->d, out->e, out->f)) {
    return SHOWDUINO_TOUCH_CAL_FAIL_NONFINITE;
  }
  showduino_touch_cal_seal(out);
  return SHOWDUINO_TOUCH_CAL_OK;
}

static inline int showduino_touch_cal_validate_geometry(const ShowduinoTouchCalPoint *pts, int n) {
  int i, j;
  double minX, maxX, minY, maxY, bestPair;
  if (!pts || n != SHOWDUINO_TOUCH_CAL_POINT_N) return SHOWDUINO_TOUCH_CAL_FAIL_POINTS;
  minX = maxX = pts[0].rawX;
  minY = maxY = pts[0].rawY;
  bestPair = 0.0;
  for (i = 0; i < n; i++) {
    if (!isfinite((double)pts[i].rawX) || !isfinite((double)pts[i].rawY) ||
        !isfinite((double)pts[i].screenX) || !isfinite((double)pts[i].screenY)) {
      return SHOWDUINO_TOUCH_CAL_FAIL_NONFINITE;
    }
    if (pts[i].rawX < minX) minX = pts[i].rawX;
    if (pts[i].rawX > maxX) maxX = pts[i].rawX;
    if (pts[i].rawY < minY) minY = pts[i].rawY;
    if (pts[i].rawY > maxY) maxY = pts[i].rawY;
    for (j = i + 1; j < n; j++) {
      const double dx = (double)pts[i].rawX - (double)pts[j].rawX;
      const double dy = (double)pts[i].rawY - (double)pts[j].rawY;
      const double d = sqrt(dx * dx + dy * dy);
      if (d > bestPair) bestPair = d;
    }
  }
  if ((maxX - minX) < SHOWDUINO_TOUCH_CAL_MIN_RAW_SPAN ||
      (maxY - minY) < SHOWDUINO_TOUCH_CAL_MIN_RAW_SPAN ||
      bestPair < SHOWDUINO_TOUCH_CAL_MIN_RAW_SPAN) {
    return SHOWDUINO_TOUCH_CAL_FAIL_SPAN;
  }
  return SHOWDUINO_TOUCH_CAL_OK;
}

static inline int showduino_touch_cal_validate_solution(const ShowduinoTouchCalibrationRecord *rec,
                                                        const ShowduinoTouchCalPoint *pts, int n) {
  int i;
  double sum2 = 0.0;
  double det, scaleX, scaleY;
  if (!rec || !pts || n != SHOWDUINO_TOUCH_CAL_POINT_N) return SHOWDUINO_TOUCH_CAL_FAIL_POINTS;
  if (!showduino_touch_cal_finite6(rec->a, rec->b, rec->c, rec->d, rec->e, rec->f)) {
    return SHOWDUINO_TOUCH_CAL_FAIL_NONFINITE;
  }
  det = (double)rec->a * (double)rec->e - (double)rec->b * (double)rec->d;
  if (fabs(det) < SHOWDUINO_TOUCH_CAL_MIN_ABS_DET) return SHOWDUINO_TOUCH_CAL_FAIL_SINGULAR;
  scaleX = sqrt((double)rec->a * (double)rec->a + (double)rec->d * (double)rec->d);
  scaleY = sqrt((double)rec->b * (double)rec->b + (double)rec->e * (double)rec->e);
  if (scaleX < SHOWDUINO_TOUCH_CAL_MIN_AXIS_SCALE || scaleX > SHOWDUINO_TOUCH_CAL_MAX_AXIS_SCALE ||
      scaleY < SHOWDUINO_TOUCH_CAL_MIN_AXIS_SCALE || scaleY > SHOWDUINO_TOUCH_CAL_MAX_AXIS_SCALE) {
    return SHOWDUINO_TOUCH_CAL_FAIL_SCALE;
  }
  for (i = 0; i < n; i++) {
    float mx, my;
    double dx, dy, r;
    showduino_touch_cal_apply(rec, pts[i].rawX, pts[i].rawY, &mx, &my);
    dx = (double)mx - (double)pts[i].screenX;
    dy = (double)my - (double)pts[i].screenY;
    r = sqrt(dx * dx + dy * dy);
    if (r > SHOWDUINO_TOUCH_CAL_MAX_RESIDUAL_PX) return SHOWDUINO_TOUCH_CAL_FAIL_RESIDUAL;
    if (i == 4 && r > SHOWDUINO_TOUCH_CAL_MAX_CENTRE_PX) return SHOWDUINO_TOUCH_CAL_FAIL_RESIDUAL;
    sum2 += r * r;
  }
  if (sqrt(sum2 / (double)n) > SHOWDUINO_TOUCH_CAL_MAX_RMS_PX) {
    return SHOWDUINO_TOUCH_CAL_FAIL_RESIDUAL;
  }
  return SHOWDUINO_TOUCH_CAL_OK;
}

static inline int showduino_touch_cal_fit(const ShowduinoTouchCalPoint *pts, int n,
                                          uint16_t width, uint16_t height,
                                          ShowduinoTouchCalibrationRecord *out) {
  int rc = showduino_touch_cal_validate_geometry(pts, n);
  if (rc != SHOWDUINO_TOUCH_CAL_OK) return rc;
  rc = showduino_touch_cal_solve(pts, n, width, height, out);
  if (rc != SHOWDUINO_TOUCH_CAL_OK) return rc;
  return showduino_touch_cal_validate_solution(out, pts, n);
}

static inline int showduino_touch_cal_record_valid(const ShowduinoTouchCalibrationRecord *rec,
                                                   size_t size, uint16_t width, uint16_t height) {
  if (!rec) return SHOWDUINO_TOUCH_CAL_FAIL_SIZE;
  if (size != sizeof(ShowduinoTouchCalibrationRecord)) return SHOWDUINO_TOUCH_CAL_FAIL_SIZE;
  if (rec->magic != SHOWDUINO_TOUCH_CAL_MAGIC) return SHOWDUINO_TOUCH_CAL_FAIL_MAGIC;
  if (rec->version != SHOWDUINO_TOUCH_CAL_VERSION) return SHOWDUINO_TOUCH_CAL_FAIL_VERSION;
  if (rec->width != width || rec->height != height) return SHOWDUINO_TOUCH_CAL_FAIL_GEOMETRY;
  if (rec->checksum != showduino_touch_cal_checksum(rec)) return SHOWDUINO_TOUCH_CAL_FAIL_CHECKSUM;
  if (!showduino_touch_cal_finite6(rec->a, rec->b, rec->c, rec->d, rec->e, rec->f)) {
    return SHOWDUINO_TOUCH_CAL_FAIL_NONFINITE;
  }
  {
    const double det = (double)rec->a * (double)rec->e - (double)rec->b * (double)rec->d;
    const double scaleX = sqrt((double)rec->a * (double)rec->a + (double)rec->d * (double)rec->d);
    const double scaleY = sqrt((double)rec->b * (double)rec->b + (double)rec->e * (double)rec->e);
    if (fabs(det) < SHOWDUINO_TOUCH_CAL_MIN_ABS_DET) return SHOWDUINO_TOUCH_CAL_FAIL_SINGULAR;
    if (scaleX < SHOWDUINO_TOUCH_CAL_MIN_AXIS_SCALE || scaleX > SHOWDUINO_TOUCH_CAL_MAX_AXIS_SCALE ||
        scaleY < SHOWDUINO_TOUCH_CAL_MIN_AXIS_SCALE || scaleY > SHOWDUINO_TOUCH_CAL_MAX_AXIS_SCALE) {
      return SHOWDUINO_TOUCH_CAL_FAIL_SCALE;
    }
  }
  return SHOWDUINO_TOUCH_CAL_OK;
}

static inline double showduino_touch_cal_residual_rms(const ShowduinoTouchCalibrationRecord *rec,
                                                      const ShowduinoTouchCalPoint *pts, int n) {
  int i;
  double sum2 = 0.0;
  if (!rec || !pts || n < 1) return 1.0e9;
  for (i = 0; i < n; i++) {
    float mx, my;
    double dx, dy;
    showduino_touch_cal_apply(rec, pts[i].rawX, pts[i].rawY, &mx, &my);
    dx = (double)mx - (double)pts[i].screenX;
    dy = (double)my - (double)pts[i].screenY;
    sum2 += dx * dx + dy * dy;
  }
  return sqrt(sum2 / (double)n);
}

static inline void showduino_touch_cal_sample_reset(ShowduinoTouchCalSampleBuf *buf) {
  if (!buf) return;
  memset(buf, 0, sizeof(*buf));
}

static inline int showduino_touch_cal_sample_add(ShowduinoTouchCalSampleBuf *buf,
                                                 int32_t x, int32_t y) {
  if (!buf || buf->n >= SHOWDUINO_TOUCH_CAL_SAMPLES) return 0;
  buf->x[buf->n] = x;
  buf->y[buf->n] = y;
  buf->n++;
  return 1;
}

static inline int32_t showduino_touch_cal_median_i(int32_t *v, uint8_t n) {
  uint8_t i, j;
  int32_t tmp;
  if (n == 0) return 0;
  for (i = 1; i < n; i++) {
    tmp = v[i];
    j = i;
    while (j > 0 && v[j - 1] > tmp) {
      v[j] = v[j - 1];
      j--;
    }
    v[j] = tmp;
  }
  if (n & 1u) return v[n / 2];
  return (v[n / 2 - 1] + v[n / 2]) / 2;
}

static inline int showduino_touch_cal_sample_median(const ShowduinoTouchCalSampleBuf *buf,
                                                    int32_t *x, int32_t *y) {
  int32_t xs[SHOWDUINO_TOUCH_CAL_SAMPLES];
  int32_t ys[SHOWDUINO_TOUCH_CAL_SAMPLES];
  if (!buf || buf->n == 0) return 0;
  memcpy(xs, buf->x, sizeof(int32_t) * buf->n);
  memcpy(ys, buf->y, sizeof(int32_t) * buf->n);
  if (x) *x = showduino_touch_cal_median_i(xs, buf->n);
  if (y) *y = showduino_touch_cal_median_i(ys, buf->n);
  return 1;
}

#ifdef __cplusplus
}
#endif

#endif
