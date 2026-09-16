#include "TouchCalibrationMath.h"

#include <cstdio>
#include <cstring>
#include <cmath>

static int gFails = 0;

static void expect(bool ok, const char *msg) {
  if (ok) std::printf("PASS  %s\n", msg);
  else {
    std::printf("FAIL  %s\n", msg);
    gFails++;
  }
}

static void expect_rc(int got, int want, const char *msg) {
  if (got == want) std::printf("PASS  %s\n", msg);
  else {
    std::printf("FAIL  %s got=%d want=%d\n", msg, got, want);
    gFails++;
  }
}

static void fill_targets(ShowduinoTouchCalPoint *pts, uint16_t w, uint16_t h,
                         void (*rawFn)(float, float, float *, float *)) {
  for (uint8_t i = 0; i < SHOWDUINO_TOUCH_CAL_POINT_N; i++) {
    float sx = 0, sy = 0, rx = 0, ry = 0;
    showduino_touch_cal_target(i, w, h, &sx, &sy);
    rawFn(sx, sy, &rx, &ry);
    pts[i].rawX = rx;
    pts[i].rawY = ry;
    pts[i].screenX = sx;
    pts[i].screenY = sy;
  }
}

static void raw_factory(float sx, float sy, float *rx, float *ry) {
  const float w1 = (float)(SHOWDUINO_TOUCH_CAL_WIDTH - 1);
  const float h1 = (float)(SHOWDUINO_TOUCH_CAL_HEIGHT - 1);
  *rx = (float)TOUCH_CAL_X_LEFT + sx * ((float)TOUCH_CAL_X_RIGHT - (float)TOUCH_CAL_X_LEFT) / w1;
  *ry = (float)TOUCH_CAL_Y_TOP + sy * ((float)TOUCH_CAL_Y_BOT - (float)TOUCH_CAL_Y_TOP) / h1;
}

static void raw_normal(float sx, float sy, float *rx, float *ry) {
  *rx = sx;
  *ry = sy;
}

static void raw_rev_x(float sx, float sy, float *rx, float *ry) {
  *rx = (float)(SHOWDUINO_TOUCH_CAL_WIDTH - 1) - sx;
  *ry = sy;
}

static void raw_rev_y(float sx, float sy, float *rx, float *ry) {
  *rx = sx;
  *ry = (float)(SHOWDUINO_TOUCH_CAL_HEIGHT - 1) - sy;
}

static void raw_rev_both(float sx, float sy, float *rx, float *ry) {
  *rx = (float)(SHOWDUINO_TOUCH_CAL_WIDTH - 1) - sx;
  *ry = (float)(SHOWDUINO_TOUCH_CAL_HEIGHT - 1) - sy;
}

static void raw_skew(float sx, float sy, float *rx, float *ry) {
  *rx = sx + 0.045f * sy;
  *ry = sy + 0.035f * sx;
}

static bool mapped_near_targets(const ShowduinoTouchCalibrationRecord *rec,
                                const ShowduinoTouchCalPoint *pts, double tol) {
  for (int i = 0; i < SHOWDUINO_TOUCH_CAL_POINT_N; i++) {
    float mx, my;
    showduino_touch_cal_apply(rec, pts[i].rawX, pts[i].rawY, &mx, &my);
    const double dx = (double)mx - (double)pts[i].screenX;
    const double dy = (double)my - (double)pts[i].screenY;
    if (std::sqrt(dx * dx + dy * dy) > tol) return false;
  }
  return true;
}

int main() {
  std::printf("Showduino Director touch calibration host tests\n\n");
  const uint16_t W = SHOWDUINO_TOUCH_CAL_WIDTH;
  const uint16_t H = SHOWDUINO_TOUCH_CAL_HEIGHT;
  ShowduinoTouchCalPoint pts[SHOWDUINO_TOUCH_CAL_POINT_N];
  ShowduinoTouchCalibrationRecord rec;

  /* TEST 1 — current reversed-X factory orientation. */
  {
    fill_targets(pts, W, H, raw_factory);
    const int rc = showduino_touch_cal_fit(pts, SHOWDUINO_TOUCH_CAL_POINT_N, W, H, &rec);
    expect_rc(rc, SHOWDUINO_TOUCH_CAL_OK, "T1 factory-orientation fit");
    expect(mapped_near_targets(&rec, pts, 2.0), "T1 mapped points near targets");
    expect(showduino_touch_cal_residual_rms(&rec, pts, SHOWDUINO_TOUCH_CAL_POINT_N) < 2.0,
           "T1 residual small");
    int32_t sx = 0, sy = 0;
    showduino_touch_cal_factory_map(TOUCH_CAL_X_LEFT, TOUCH_CAL_Y_TOP, W, H, &sx, &sy);
    expect(sx == 0 && sy == 0, "T1 factory map left/top -> origin");
    showduino_touch_cal_factory_map(TOUCH_CAL_X_RIGHT, TOUCH_CAL_Y_BOT, W, H, &sx, &sy);
    expect(sx == (W - 1) && sy == (H - 1), "T1 factory map right/bot -> max");
  }

  /* TEST 2 — normal axes. */
  {
    fill_targets(pts, W, H, raw_normal);
    const int rc = showduino_touch_cal_fit(pts, SHOWDUINO_TOUCH_CAL_POINT_N, W, H, &rec);
    expect_rc(rc, SHOWDUINO_TOUCH_CAL_OK, "T2 normal-axis fit");
    expect(mapped_near_targets(&rec, pts, 2.0), "T2 mapped near targets");
  }

  /* TEST 3 — reversed X. */
  {
    fill_targets(pts, W, H, raw_rev_x);
    const int rc = showduino_touch_cal_fit(pts, SHOWDUINO_TOUCH_CAL_POINT_N, W, H, &rec);
    expect_rc(rc, SHOWDUINO_TOUCH_CAL_OK, "T3 reversed-X fit");
    expect(mapped_near_targets(&rec, pts, 2.0), "T3 mapped near targets");
  }

  /* TEST 4 — reversed Y. */
  {
    fill_targets(pts, W, H, raw_rev_y);
    const int rc = showduino_touch_cal_fit(pts, SHOWDUINO_TOUCH_CAL_POINT_N, W, H, &rec);
    expect_rc(rc, SHOWDUINO_TOUCH_CAL_OK, "T4 reversed-Y fit");
    expect(mapped_near_targets(&rec, pts, 2.0), "T4 mapped near targets");
  }

  /* TEST 5 — both axes reversed. */
  {
    fill_targets(pts, W, H, raw_rev_both);
    const int rc = showduino_touch_cal_fit(pts, SHOWDUINO_TOUCH_CAL_POINT_N, W, H, &rec);
    expect_rc(rc, SHOWDUINO_TOUCH_CAL_OK, "T5 both-reversed fit");
    expect(mapped_near_targets(&rec, pts, 2.0), "T5 mapped near targets");
  }

  /* TEST 6 — small skew. */
  {
    fill_targets(pts, W, H, raw_skew);
    const int rc = showduino_touch_cal_fit(pts, SHOWDUINO_TOUCH_CAL_POINT_N, W, H, &rec);
    expect_rc(rc, SHOWDUINO_TOUCH_CAL_OK, "T6 skew fit");
    expect(mapped_near_targets(&rec, pts, 3.0), "T6 affine corrects skew");
  }

  /* TEST 7 — centre residual. */
  {
    fill_targets(pts, W, H, raw_factory);
    showduino_touch_cal_fit(pts, SHOWDUINO_TOUCH_CAL_POINT_N, W, H, &rec);
    float mx, my;
    showduino_touch_cal_apply(&rec, pts[4].rawX, pts[4].rawY, &mx, &my);
    const double dx = (double)mx - (double)pts[4].screenX;
    const double dy = (double)my - (double)pts[4].screenY;
    expect(std::sqrt(dx * dx + dy * dy) <= SHOWDUINO_TOUCH_CAL_MAX_CENTRE_PX,
           "T7 centre residual acceptable");
  }

  /* TEST 8 — duplicate / singular points rejected. */
  {
    fill_targets(pts, W, H, raw_normal);
    for (int i = 0; i < SHOWDUINO_TOUCH_CAL_POINT_N; i++) {
      pts[i].rawX = 100;
      pts[i].rawY = 100;
    }
    const int rc = showduino_touch_cal_fit(pts, SHOWDUINO_TOUCH_CAL_POINT_N, W, H, &rec);
    expect(rc != SHOWDUINO_TOUCH_CAL_OK, "T8 duplicate points rejected");
  }

  /* TEST 9 — extreme nonsense rejected. */
  {
    fill_targets(pts, W, H, raw_normal);
    pts[0].rawX = 1;   pts[0].rawY = 1;
    pts[1].rawX = 2;   pts[1].rawY = 2;
    pts[2].rawX = 3;   pts[2].rawY = 1;
    pts[3].rawX = 1;   pts[3].rawY = 3;
    pts[4].rawX = 2;   pts[4].rawY = 2;
    const int rc = showduino_touch_cal_fit(pts, SHOWDUINO_TOUCH_CAL_POINT_N, W, H, &rec);
    expect(rc != SHOWDUINO_TOUCH_CAL_OK, "T9 garbage coordinates rejected");
  }

  /* TEST 10 — unsupported NVS version. */
  {
    fill_targets(pts, W, H, raw_normal);
    showduino_touch_cal_fit(pts, SHOWDUINO_TOUCH_CAL_POINT_N, W, H, &rec);
    rec.version = 99;
    rec.checksum = showduino_touch_cal_checksum(&rec);
    expect_rc(showduino_touch_cal_record_valid(&rec, sizeof(rec), W, H),
              SHOWDUINO_TOUCH_CAL_FAIL_VERSION, "T10 unsupported version rejected");
  }

  /* TEST 11 — bad checksum. */
  {
    fill_targets(pts, W, H, raw_normal);
    showduino_touch_cal_fit(pts, SHOWDUINO_TOUCH_CAL_POINT_N, W, H, &rec);
    rec.checksum ^= 0xFFFFFFFFu;
    expect_rc(showduino_touch_cal_record_valid(&rec, sizeof(rec), W, H),
              SHOWDUINO_TOUCH_CAL_FAIL_CHECKSUM, "T11 bad checksum rejected");
  }

  /* TEST 12 — wrong resolution. */
  {
    fill_targets(pts, W, H, raw_normal);
    showduino_touch_cal_fit(pts, SHOWDUINO_TOUCH_CAL_POINT_N, W, H, &rec);
    expect_rc(showduino_touch_cal_record_valid(&rec, sizeof(rec), 1024, 600),
              SHOWDUINO_TOUCH_CAL_FAIL_GEOMETRY, "T12 wrong resolution rejected");
    expect_rc(showduino_touch_cal_record_valid(&rec, sizeof(rec), W, H),
              SHOWDUINO_TOUCH_CAL_OK, "T12 matching resolution accepted");
    expect_rc(showduino_touch_cal_record_valid(&rec, 12, W, H),
              SHOWDUINO_TOUCH_CAL_FAIL_SIZE, "T12 short blob rejected");
  }

  /* Median sampling helper. */
  {
    ShowduinoTouchCalSampleBuf buf;
    showduino_touch_cal_sample_reset(&buf);
    const int32_t xs[9] = {10, 12, 11, 80, 11, 10, 12, 11, 11};
    const int32_t ys[9] = {20, 21, 19, 90, 20, 21, 20, 20, 19};
    for (int i = 0; i < 9; i++) showduino_touch_cal_sample_add(&buf, xs[i], ys[i]);
    int32_t mx = 0, my = 0;
    showduino_touch_cal_sample_median(&buf, &mx, &my);
    expect(mx == 11 && my == 20, "median rejects single outlier");
  }

  if (gFails) {
    std::printf("\n%d FAILED\n", gFails);
    return 1;
  }
  std::printf("\nALL PASS\n");
  return 0;
}
