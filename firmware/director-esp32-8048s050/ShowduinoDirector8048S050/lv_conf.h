/**
 * lv_conf.h — Showduino Director LVGL 9 configuration
 * JC8048W550C / ESP32-S3 / 800×480 RGB panel
 *
 * Arduino IDE: copy this file to BOTH locations:
 *   1) ShowduinoDirector8048S050/lv_conf.h  (next to the sketch — already here)
 *   2) Documents/Arduino/libraries/lv_conf.h  (next to the lvgl folder)
 * The libraries copy is required — Arduino compiles LVGL separately from the sketch.
 *
 * Without CLIB malloc the default ~64 KB LVGL pool is too small for six screens;
 * LV_USE_ASSERT_MALLOC then hangs in while(1) during LIVE screen build.
 */

/* clang-format off */
#if 1

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#define LV_COLOR_DEPTH          16
#define LV_COLOR_16_SWAP        0
#define LV_COLOR_CHROMA_KEY     lv_color_hex(0x00ff00)

#define LV_USE_STDLIB_MALLOC    LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING    LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_CLIB

#define LV_MEM_SIZE             (256ul * 1024ul)
#define LV_MEM_ADR              0

#define LV_TICK_CUSTOM          0
#define LV_DEF_REFR_PERIOD      8
#define LV_DPI_DEF              130

#define LV_USE_DRAW_SW          1
#define LV_USE_DRAW_SW_ASM      LV_DRAW_SW_ASM_NONE
#define LV_DRAW_SW_SHADOW_CACHE_SIZE    0
#define LV_DRAW_SW_CIRCLE_CACHE_COUNT   4

#define LV_USE_DRAW_VGLITE      0
#define LV_USE_DRAW_PXP         0
#define LV_USE_DRAW_DAVE2D      0
#define LV_USE_DRAW_DMA2D       0
#define LV_USE_DRAW_OPENGLES    0
#define LV_USE_GPU_ARM2D        0

#define LV_DRAW_BUF_ALIGN           4
#define LV_DRAW_BUF_STRIDE_ALIGN    1
#define LV_CACHE_DEF_SIZE           0

#define LV_USE_LOG              0
#define LV_USE_ASSERT_NULL      1
#define LV_USE_ASSERT_MALLOC    0
#define LV_USE_ASSERT_STYLE     0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ       0

#define LV_OBJ_STYLE_MAX_NUM    16
#define LV_OBJ_TREE_MAX_DEPTH   50

#define LV_INDEV_DEF_SCROLL_LIMIT           8
#define LV_INDEV_DEF_SCROLL_THROW           35
#define LV_INDEV_DEF_LONG_PRESS_TIME        400
#define LV_INDEV_DEF_LONG_PRESS_REP_TIME    100
#define LV_INDEV_DEF_GESTURE_LIMIT          50
#define LV_INDEV_DEF_GESTURE_MIN_VELOCITY   3

#define LV_USE_ANIM             1
#define LV_USE_SCROLL_ON_FOCUS  1
#define LV_USE_OBJ_ID           0
#define LV_USE_SNAPSHOT         0
#define LV_USE_SYSMON           0
#define LV_USE_PERF_MONITOR     0
#define LV_USE_MEM_MONITOR      0
#define LV_USE_REFR_DEBUG       0

#define LV_USE_OS               LV_OS_NONE

#define LV_USE_BMP              0
#define LV_USE_TJPGD            0
#define LV_USE_LIBJPEG_TURBO    0
#define LV_USE_GIF              0
#define LV_USE_LODEPNG          0
#define LV_USE_LIBPNG           0
#define LV_USE_FFMPEG           0
#define LV_USE_RLE              0

#define LV_USE_FONT_COMPRESSED  0
#define LV_USE_FONT_SUBPX       0
#define LV_USE_FONT_PLACEHOLDER 1

#define LV_FONT_MONTSERRAT_8    0
#define LV_FONT_MONTSERRAT_10   1
#define LV_FONT_MONTSERRAT_12   1
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_MONTSERRAT_18   0
#define LV_FONT_MONTSERRAT_20   1
#define LV_FONT_MONTSERRAT_22   0
#define LV_FONT_MONTSERRAT_24   1
#define LV_FONT_MONTSERRAT_26   0
#define LV_FONT_MONTSERRAT_28   1
#define LV_FONT_MONTSERRAT_30   0
#define LV_FONT_MONTSERRAT_32   0
#define LV_FONT_MONTSERRAT_34   0
#define LV_FONT_MONTSERRAT_36   1
#define LV_FONT_MONTSERRAT_38   0
#define LV_FONT_MONTSERRAT_40   0
#define LV_FONT_MONTSERRAT_42   0
#define LV_FONT_MONTSERRAT_44   0
#define LV_FONT_MONTSERRAT_46   0
#define LV_FONT_MONTSERRAT_48   0
#define LV_FONT_DEFAULT         &lv_font_montserrat_16

#define LV_TXT_ENC              LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS      " ,.;:-_"
#define LV_TXT_LINE_BREAK_LONG_LEN  0
#define LV_TXT_COLOR_CMD        "#"
#define LV_USE_BIDI             0

#define LV_USE_ANIMIMG          0
#define LV_USE_ANIMIMAGE        0
#define LV_USE_ARC              1
#define LV_USE_BAR              1
#define LV_USE_BUTTON           1
#define LV_USE_BUTTONMATRIX     0
#define LV_USE_CALENDAR         0
#define LV_USE_CANVAS           1
#define LV_USE_CHART            0
#define LV_USE_CHECKBOX         0
#define LV_USE_COLORWHEEL       0
#define LV_USE_DROPDOWN         0
#define LV_USE_IMAGE            1
#define LV_USE_IMAGEBUTTON      0
#define LV_USE_KEYBOARD         0
#define LV_USE_LABEL            1
#define LV_USE_LED              0
#define LV_USE_LINE             0
#define LV_USE_LIST             0
#define LV_USE_LOTTIE           0
#define LV_USE_MENU             0
#define LV_USE_METER            0
#define LV_USE_MSGBOX           0
#define LV_USE_ROLLER           0
#define LV_USE_SCALE            0
#define LV_USE_SLIDER           0
#define LV_USE_SPAN             0
#define LV_USE_SPINBOX          0
#define LV_USE_SPINNER          0
#define LV_USE_SWITCH           0
#define LV_USE_TABLE            0
#define LV_USE_TABVIEW          0
#define LV_USE_TEXTAREA         0
#define LV_USE_TILEVIEW         0
#define LV_USE_WIN              0

#define LV_USE_THEME_DEFAULT    1
#define LV_USE_THEME_SIMPLE     1
#define LV_USE_THEME_MONO       0
#define LV_THEME_DEFAULT_DARK   1
#define LV_THEME_DEFAULT_GROW   0
#define LV_THEME_DEFAULT_TRANSITION_TIME 0

#define LV_USE_FLEX             1
#define LV_USE_GRID             0

#define LV_USE_FRAGMENT         0
#define LV_USE_IMGFONT          0
#define LV_USE_OBSERVER         0
#define LV_USE_MONKEY           0
#define LV_USE_GRIDNAV          0
#define LV_USE_LZ4              0
#define LV_USE_QRCODE           0
#define LV_USE_BARCODE          0

#define LV_EXPORT_CONST_INT(int_value)  struct _silence_gcc_warning
#define LV_ATTRIBUTE_FAST_MEM
#define LV_ATTRIBUTE_MEM_ALIGN_SIZE     4
#define LV_ATTRIBUTE_MEM_ALIGN          __attribute__((aligned(LV_ATTRIBUTE_MEM_ALIGN_SIZE)))
#define LV_ATTRIBUTE_LARGE_CONST
#define LV_ATTRIBUTE_LARGE_RAM_ARRAY
#define LV_ATTRIBUTE_DISPLAY_FLUSH_LIST
#define LV_ATTRIBUTE_TIMER_HANDLER
#define LV_ATTRIBUTE_FONT_HEAVY
#define LV_GC_ROOT(x)                   x
#define LV_ATTRIBUTE_EXTERN_DATA

#endif /* LV_CONF_H */
#endif
