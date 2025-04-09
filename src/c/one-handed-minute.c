#include <pebble.h>

#define BUFFER_LEN (100)
#define DEBUG_BBOX (false)
#define DEBUG_GRID (false)
#define DEBUG_TIME (false)
#define INCLUDE_DATE (true)
#define INCLUDE_TICKS (true)

static const GPathInfo ARROW_POINTS = {
  .num_points = 5,
  // points will be filled by change_arrow_size
  .points = (GPoint []) {
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
  }
};

static Window* s_window;
static Layer* s_layer;
static char s_buffer[BUFFER_LEN];
static GPath* s_arrow;

static void fast_forward_time(struct tm* now) {
  now->tm_min = now->tm_sec;           /* Minutes. [0-59] */
  now->tm_hour = now->tm_sec % 24;     /* Hours.  [0-23] */
  now->tm_mday = now->tm_sec % 31 + 1; /* Day. [1-31] */
  now->tm_mon = now->tm_sec % 12;      /* Month. [0-11] */
  now->tm_wday = now->tm_sec % 7;      /* Day of week. [0-6] */
}

static int min(int a, int b) {
  if (a < b) {
    return a;
  }
  return b;
}

static GPoint cartesian_from_polar(GPoint center, int radius, int angle_deg) {
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
  int hour = now->tm_hour;
  if (!clock_is_24h_style()) {
    hour = now->tm_hour % 12;
    if (hour == 0) {
      hour = 12;
    }
  }
  snprintf(s_buffer, BUFFER_LEN, "%d", hour);
}

static void format_day_of_week(struct tm* now) {
  strftime(s_buffer, BUFFER_LEN, "%a", now);
}

static void format_day_and_month(struct tm* now) {
  strftime(s_buffer, BUFFER_LEN, "%b %e", now);
}

static void draw_grid(GContext* ctx, GRect bounds) {
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  for (int x = 0; x <= bounds.size.w; x += 10) {
    for (int y = 0; y <= bounds.size.h; y += 10) {
      graphics_draw_pixel(ctx, GPoint(x, y));
    }
  }
}
    
static void draw_ticks(GContext* ctx, GPoint center, int visible_circle_radius) {
  graphics_context_set_fill_color(ctx, GColorClear);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 1);
  for (int16_t tick_minute = 0; tick_minute < 60; tick_minute += 1) {
    int tick_deg = tick_minute * 360 / 60;
    GPoint minute_tick_outer = cartesian_from_polar(center, visible_circle_radius, tick_deg);
    int tick_length = 1;
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
    GPoint minute_tick_inner = cartesian_from_polar(center, visible_circle_radius - tick_length, tick_deg);
    graphics_draw_line(ctx, minute_tick_inner, minute_tick_outer);
  }
}

static void draw_hour(GContext* ctx, GPoint center, int minute_deg, int visible_circle_radius, struct tm* now) {
  GSize hour_bbox_size = GSize(80, 50);
  int inverted_minute_deg = 180 + minute_deg;
  GPoint hour_bbox_midpoint = cartesian_from_polar(center, visible_circle_radius / 2 - 5, inverted_minute_deg);
  GRect hour_bbox = rect_from_midpoint(hour_bbox_midpoint, hour_bbox_size);
  GFont hour_font = fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT);
  if (DEBUG_BBOX) {
    graphics_draw_rect(ctx, hour_bbox);
  }
  format_hour(now);
  graphics_draw_text(ctx, s_buffer, hour_font, hour_bbox, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void change_arrow_size(int w, int h) {
  ARROW_POINTS.points[0].x = w / 2;
  ARROW_POINTS.points[0].y = 0;

  ARROW_POINTS.points[1].x = w / 2;
  ARROW_POINTS.points[1].y = -h * 7 / 10;

  ARROW_POINTS.points[2].x = 0;
  ARROW_POINTS.points[2].y = -h;

  ARROW_POINTS.points[3].x = -w / 2;
  ARROW_POINTS.points[3].y = -h * 7 / 10;

  ARROW_POINTS.points[4].x = -w / 2;
  ARROW_POINTS.points[4].y = 0;
}

static void draw_hand(GContext* ctx, GPoint center, int minute_deg, int hand_length) {
  int hand_width = 8;
  // Black stroke makes it easier to see when overlapping tick
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 1);
  change_arrow_size(hand_width, hand_length);
  gpath_rotate_to(s_arrow, DEG_TO_TRIGANGLE(minute_deg));
  gpath_move_to(s_arrow, center);
  gpath_draw_filled(ctx, s_arrow);
  gpath_draw_outline(ctx, s_arrow);
  // Circle at the base to smooth out the rotation
  graphics_fill_circle(ctx, center, 3);
}

static void draw_date(GContext* ctx, GRect bounds, int visible_circle_radius, struct tm* now) {
  GFont date_font = fonts_get_system_font(FONT_KEY_GOTHIC_24);
  GRect date_bbox;
  date_bbox.origin = GPoint(2, 2 * visible_circle_radius - 8);
  date_bbox.size = GSize(bounds.size.w - 4, bounds.size.h - date_bbox.origin.y);
  if (DEBUG_BBOX) {
    graphics_draw_rect(ctx, date_bbox);
  }
  format_day_of_week(now);
  graphics_draw_text(ctx, s_buffer, date_font, date_bbox, GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  format_day_and_month(now);
  graphics_draw_text(ctx, s_buffer, date_font, date_bbox, GTextOverflowModeFill, GTextAlignmentRight, NULL);
}

static void update_layer(Layer* layer, GContext* ctx) {
  time_t temp = time(NULL);
  struct tm* now = localtime(&temp);
  if (DEBUG_TIME) {
    fast_forward_time(now);
  }
  GRect bounds = layer_get_bounds(layer);
  if (DEBUG_GRID) {
    draw_grid(ctx, bounds);
  }
  int visible_circle_radius = min(bounds.size.h, bounds.size.w) / 2 - 2;
  GPoint center = GPoint(visible_circle_radius + 1, visible_circle_radius + 1);
  int hand_length = visible_circle_radius - 2;
  int minute = now->tm_min;
  int minute_deg = 360 * minute / 60;

  if (INCLUDE_TICKS) {
    draw_ticks(ctx, center, visible_circle_radius);
  }
  draw_hour(ctx, center, minute_deg, visible_circle_radius, now);
  if (INCLUDE_DATE) {
    draw_date(ctx, bounds, visible_circle_radius, now);
  }
  draw_hand(ctx, center, minute_deg, hand_length);
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
  s_arrow = gpath_create(&ARROW_POINTS);
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
