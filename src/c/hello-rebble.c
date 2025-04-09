#include <pebble.h>

#define BUFFER_LEN (100)
#define DEBUG_BBOX (false)
#define DEBUG_GRID (false)
#define DEBUG_TIME (false)
#define INCLUDE_DATE (true)
#define INCLUDE_MINUTE_TICKS (true)

static Window* s_window;
static Layer* s_layer;
static char s_buffer[BUFFER_LEN];

static int32_t min(int32_t a, int32_t b) {
  if (a < b) {
    return a;
  }
  return b;
}

static GPoint cartesian_from_polar(GPoint center, int32_t radius, int32_t angle_deg) {
  GPoint ret = {
    .x = (int16_t)(sin_lookup(DEG_TO_TRIGANGLE(angle_deg)) * radius / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(DEG_TO_TRIGANGLE(angle_deg)) * radius / TRIG_MAX_RATIO) + center.y,
  };
  return ret;
}

static GRect rect_from_midpoint(GPoint midpoint, GSize size) {
  GRect ret;
  ret.origin.x = midpoint.x - size.w / 2;
  ret.origin.y = midpoint.y - size.h / 2;
  ret.size = size;
  return ret;
}

static void format_hour(struct tm* now) {
  int hour = now->tm_hour % 12;
  if (hour == 0) {
    hour = 12;
  }
  snprintf(s_buffer, BUFFER_LEN, "%d", hour);
}

static void format_date(struct tm* now) {
  strftime(s_buffer, BUFFER_LEN, "%A %B %e", now);
}

static bool is_tall_enough_for_date(GRect bounds, int32_t visible_circle_radius, int32_t font_height) {
  return INCLUDE_DATE && (bounds.size.h - visible_circle_radius * 2 > font_height);
}

static void draw_grid(GContext* ctx, GRect bounds) {
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  for (int x = 0; x <= bounds.size.w; x += 10) {
    for (int y = 0; y <= bounds.size.h; y += 10) {
      graphics_draw_pixel(ctx, GPoint(x, y));
    }
  }
}

static void fast_forward_time(struct tm* now) {
  now->tm_min = now->tm_sec;           /* Minutes. [0-59] */
  now->tm_hour = now->tm_sec % 24;     /* Hours.  [0-23] */
  now->tm_mday = now->tm_sec % 31 + 1; /* Day. [1-31] */
  now->tm_mon = now->tm_sec % 12;      /* Month. [0-11] */
  now->tm_wday = now->tm_sec % 7;      /* Day of week. [0-6] */
}

static void update_layer(Layer* layer, GContext* ctx) {
  time_t temp = time(NULL);
  struct tm* now = localtime(&temp);
  if (DEBUG_TIME) {
    fast_forward_time(now);
  }
  int32_t hand_width = 5;
  GRect bounds = layer_get_bounds(layer);
  if (DEBUG_GRID) {
    draw_grid(ctx, bounds);
  }
  int32_t visible_circle_radius = min(bounds.size.h, bounds.size.w) / 2 - 1;
  int32_t date_font_height = 18;
  bool tall_enough_for_date = is_tall_enough_for_date(bounds, visible_circle_radius, date_font_height);
  GPoint center;
  if (tall_enough_for_date) {
    center = GPoint(visible_circle_radius, visible_circle_radius);
  } else {
    center = grect_center_point(&bounds);
  }
  int32_t minute_hand_length = visible_circle_radius - hand_width / 2;
  int minute = now->tm_min;
  int32_t minute_deg = 360 * minute / 60;
  GPoint minute_hand_tip = cartesian_from_polar(center, minute_hand_length, minute_deg);

  // global graphics settings
  graphics_context_set_antialiased(ctx, true);

  // draw minute hand
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, hand_width);
  graphics_draw_line(ctx, center, minute_hand_tip);

  // draw analog face circular edge
  graphics_context_set_fill_color(ctx, GColorClear);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 1);
  if (DEBUG_BBOX) {
    graphics_draw_circle(ctx, center, visible_circle_radius);
  }

  // draw analog face ticks
  if (INCLUDE_MINUTE_TICKS) {
    for (int16_t tick_minute = 0; tick_minute < 60; tick_minute += 1) {
      int32_t tick_deg = tick_minute * 360 / 60;
      GPoint minute_tick_outer = cartesian_from_polar(center, minute_hand_length, tick_deg);
      int32_t tick_length = 1;
      if (tick_minute % 15 == 0) {
        graphics_context_set_stroke_width(ctx, 3);
        tick_length = 2 * visible_circle_radius / 10;
      } else if (tick_minute % 5 == 0) {
        graphics_context_set_stroke_width(ctx, 1);
        tick_length = visible_circle_radius / 10;
      } else {
        graphics_draw_pixel(ctx, minute_tick_outer);
        continue;
      }
      GPoint minute_tick_inner = cartesian_from_polar(center, minute_hand_length - tick_length, tick_deg);
      graphics_draw_line(ctx, minute_tick_inner, minute_tick_outer);
    }
  }

  GSize hour_bbox_size = GSize(40, 40);
  int32_t inverted_minute_deg = 180 + minute_deg;
  GPoint hour_bbox_midpoint = cartesian_from_polar(center, hour_bbox_size.w / 2, inverted_minute_deg);
  GRect hour_bbox = rect_from_midpoint(hour_bbox_midpoint, hour_bbox_size);
  GFont hour_font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  if (DEBUG_BBOX) {
    graphics_draw_rect(ctx, hour_bbox);
  }
  format_hour(now);
  graphics_draw_text(ctx, s_buffer, hour_font, hour_bbox, GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  // date on the bottom
  if (tall_enough_for_date) {
    GFont date_font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
    GRect date_bbox = GRect(0, 2 * visible_circle_radius, bounds.size.w, bounds.size.h - 2 * visible_circle_radius);
    if (DEBUG_BBOX) {
      graphics_draw_rect(ctx, date_bbox);
    }
    format_date(now);
    graphics_draw_text(ctx, s_buffer, date_font, date_bbox, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
}


static void window_load(Window* window) {
  Layer* window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  window_set_background_color(s_window, GColorBlack);
  s_layer = layer_create(bounds);
  layer_set_update_proc(s_layer, update_layer);
  layer_add_child(window_layer, s_layer);
}

static void window_unload(Window* window) {
  layer_destroy(s_layer);
}


static void tick_handler(struct tm* now, TimeUnits units_changed) {
  layer_mark_dirty(window_get_root_layer(s_window));
}

static void init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
  tick_timer_service_subscribe(DEBUG_TIME ? SECOND_UNIT : MINUTE_UNIT, tick_handler);
}

static void deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
