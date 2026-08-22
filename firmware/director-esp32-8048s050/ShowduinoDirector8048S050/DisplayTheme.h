#ifndef SHOWDUINO_DISPLAY_THEME_H
#define SHOWDUINO_DISPLAY_THEME_H

#include "DisplayTypes.h"

/**
 * Theme discovery + compatibility. Paths:
 *   /showduino/ui/themes/<currentTheme()>/theme.json
 *
 * Page BMP/PNG artwork is no longer used. LVGL owns every visible surface.
 */
class DisplayTheme {
 public:
  static void begin();
  static const char *currentTheme();
  static void setTheme(const char *name);

  /** Load/validate theme.json for current theme. Major mismatch or bad resolution → false. */
  static bool validateCurrentTheme();

  /** True after a successful theme.json validate for the current theme. */
  static bool isValid();

  /**
   * Page background images are retired. Always returns false.
   */
  static bool resolvePageImage(DisplayPageId page, const char *imageBasename,
                               char *out, size_t outLen);

  static int themeMajor();
  static const char *themeDisplayName();

 private:
  static bool parseThemeJson(const char *path);
  static void buildThemeRoot(char *out, size_t outLen);
};

#endif
