/*
 * Machine-independent BMX menu geometry.
 *
 * This header deliberately has no dependency on an emulator canvas or a
 * framebuffer backend.  Keeping the calculation pure makes the UI contract
 * cheap to use and straightforward to verify on the host.
 */

#ifndef RASPI_UI_GEOMETRY_H_
#define RASPI_UI_GEOMETRY_H_

#include <limits.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  UI_MENU_WIDTH_CHARS = 40,
  UI_MENU_HEIGHT_CHARS = 25,
  UI_MENU_GLYPH_WIDTH = 8,
  UI_MENU_GLYPH_HEIGHT = 8,
  UI_MENU_CONTENT_WIDTH = UI_MENU_WIDTH_CHARS * UI_MENU_GLYPH_WIDTH,
  UI_MENU_CONTENT_HEIGHT = UI_MENU_HEIGHT_CHARS * UI_MENU_GLYPH_HEIGHT,
  UI_MENU_BORDER = 1,
  UI_MENU_ROOT_LEFT = UI_MENU_BORDER,
  UI_MENU_ROOT_TOP = UI_MENU_BORDER,
  UI_SOURCE_WIDTH = UI_MENU_CONTENT_WIDTH + UI_MENU_BORDER * 2,
  UI_SOURCE_HEIGHT = UI_MENU_CONTENT_HEIGHT + UI_MENU_BORDER * 2,
  UI_DESIGN_WIDTH = 384,
  UI_DESIGN_HEIGHT = 240,
  UI_MENU_SCALE_MIN = 75,
  UI_MENU_SCALE_DEFAULT = 100,
  UI_MENU_SCALE_MAX = 115,
  UI_MENU_SCALE_STEP = 5,
  UI_MENU_SCALE_FINE_STEP = 1,
  UI_MENU_ROW_GAP_MIN = 0,
  UI_MENU_ROW_GAP_DEFAULT = 0,
  UI_MENU_ROW_GAP_MAX = 4,
  UI_MENU_ROW_GAP_STEP = 1,
};

struct ui_output_geometry {
  int source_x;
  int source_y;
  int source_width;
  int source_height;
  int target_width;
  int target_height;
};

static inline int ui_menu_scale_is_valid(int percent) {
  return percent >= UI_MENU_SCALE_MIN && percent <= UI_MENU_SCALE_MAX;
}

static inline int ui_menu_row_gap_is_valid(int gap) {
  return gap >= UI_MENU_ROW_GAP_MIN && gap <= UI_MENU_ROW_GAP_MAX;
}

static inline int ui_menu_row_pitch(int gap) {
  if (!ui_menu_row_gap_is_valid(gap)) {
    return 0;
  }
  return UI_MENU_GLYPH_HEIGHT + gap;
}

static inline int ui_menu_height_for_rows(int rows, int gap) {
  uint64_t height;

  if (rows <= 0 || !ui_menu_row_gap_is_valid(gap)) {
    return 0;
  }
  height = (uint64_t)(unsigned)rows * UI_MENU_GLYPH_HEIGHT +
           (uint64_t)(unsigned)(rows - 1) * (unsigned)gap;
  return height <= INT_MAX ? (int)height : 0;
}

static inline int ui_menu_visible_rows(int height, int gap) {
  int pitch = ui_menu_row_pitch(gap);

  if (height <= 0 || pitch == 0) {
    return 0;
  }
  return (int)(((uint64_t)(unsigned)height + (unsigned)gap) /
               (unsigned)pitch);
}

static inline int ui_calculate_output_geometry(
    int display_width, int display_height, int percent,
    struct ui_output_geometry *geometry) {
  uint64_t scale_numerator;
  uint64_t scale_denominator;
  uint64_t target_width;
  uint64_t target_height;

  if (geometry == 0 || display_width <= 0 || display_height <= 0 ||
      !ui_menu_scale_is_valid(percent)) {
    return 0;
  }

  /* Select the limiting dimension without using floating point. */
  if ((uint64_t)(unsigned)display_width * UI_DESIGN_HEIGHT <=
      (uint64_t)(unsigned)display_height * UI_DESIGN_WIDTH) {
    scale_numerator = (uint64_t)(unsigned)display_width * (unsigned)percent;
    scale_denominator = (uint64_t)UI_DESIGN_WIDTH * 100U;
  } else {
    scale_numerator = (uint64_t)(unsigned)display_height * (unsigned)percent;
    scale_denominator = (uint64_t)UI_DESIGN_HEIGHT * 100U;
  }

  /* Round both dimensions with the same exact scale fraction. */
  target_width =
      ((uint64_t)UI_SOURCE_WIDTH * scale_numerator +
       scale_denominator / 2U) /
      scale_denominator;
  target_height =
      ((uint64_t)UI_SOURCE_HEIGHT * scale_numerator +
       scale_denominator / 2U) /
      scale_denominator;

  if (target_width == 0U || target_height == 0U ||
      target_width > (uint64_t)(unsigned)display_width ||
      target_height > (uint64_t)(unsigned)display_height) {
    return 0;
  }

  geometry->source_x = 0;
  geometry->source_y = 0;
  geometry->source_width = UI_SOURCE_WIDTH;
  geometry->source_height = UI_SOURCE_HEIGHT;
  geometry->target_width = (int)target_width;
  geometry->target_height = (int)target_height;
  return 1;
}

#ifdef __cplusplus
}
#endif

#endif
