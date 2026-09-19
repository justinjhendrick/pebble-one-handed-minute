#include <pebble.h>
#include "utils.h"
#include "shapes.h"

#define BUFFER_LEN (10)
#define DEBUG_TIME (false)
#define DEBUG_BBOX (false)
#define FORCE_BT_MISSING (false)
#define BIG (PBL_DISPLAY_WIDTH >= 200)
#define HAS_COLOR (PBL_IF_COLOR_ELSE(true, false))
#define IS_ROUND (PBL_IF_ROUND_ELSE(true, false))
#define IS_RECT (PBL_IF_ROUND_ELSE(false, true))
#define SETTINGS_RESERVED_BYTES (35)
#define DEFAULT_STEP_GOAL (0)
#define DEFAULT_COLOR_MIDDLE_TICK (GColorWhite)

#define SETTINGS_VERSION_KEY 1
#define SETTINGS_KEY 2

typedef struct ClaySettings {
  GColor color_background;
  GColor color_major_tick; // 15m
  GColor color_minor_tick; // 1m
  GColor color_hand;
  GColor color_hand_inside;
  GColor color_hour;
  GColor color_day_of_week;
  GColor color_month_date;
  GColor color_battery_inside;
  GColor color_battery_outside;
  // Above this line was in settings v1
  int step_goal;
  // Above this line was in settings v2
  GColor color_middle_tick; // 5m
  // Above this line was in settings v3

  uint8_t reserved[SETTINGS_RESERVED_BYTES]; // for later growth
} __attribute__((__packed__)) ClaySettings;

ClaySettings settings;

static void default_settings() {
  settings.color_background = GColorBlack;
  settings.color_major_tick = GColorWhite;
  settings.color_minor_tick = GColorWhite;
  settings.color_hand = COLOR_FALLBACK(GColorPictonBlue, GColorWhite);
  settings.color_hand_inside = COLOR_FALLBACK(GColorCobaltBlue, GColorBlack);
  settings.color_hour = GColorWhite;
  settings.color_day_of_week = GColorWhite;
  settings.color_month_date = GColorWhite;
  settings.color_battery_inside = COLOR_FALLBACK(GColorBrightGreen, GColorWhite);
  settings.color_battery_outside = GColorWhite;
  settings.step_goal = DEFAULT_STEP_GOAL;
  settings.color_middle_tick = DEFAULT_COLOR_MIDDLE_TICK;

  for (int i = 0; i < SETTINGS_RESERVED_BYTES; i++) {
    settings.reserved[i] = 0;
  }
}

static Window* s_window;
static Layer* s_layer;
static char s_buffer[BUFFER_LEN];
static GFont s_font_lg = NULL;

static void debug_bbox(GContext* ctx, GRect bbox) {
  if (DEBUG_BBOX) {
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_rect(ctx, bbox);
  }
}

static void draw_ticks(GContext* ctx, GPoint center, int visible_circle_radius) {
  for (int16_t tick_minute = 0; tick_minute < 60; tick_minute += 1) {
    int tick_deg = tick_minute * 360 / 60;
    GPoint minute_tick_outer = cartesian_from_polar(center, visible_circle_radius, tick_deg);
    int tick_length = 1;
    if (tick_minute % 15 == 0) {
      graphics_context_set_stroke_color(ctx, settings.color_major_tick);
      graphics_context_set_stroke_width(ctx, 7);
      tick_length = 2 * visible_circle_radius / 10;
    } else if (tick_minute % 5 == 0) {
      graphics_context_set_stroke_color(ctx, settings.color_middle_tick);
      graphics_context_set_stroke_width(ctx, 5);
      tick_length = 2 * visible_circle_radius / 10;
    } else {
      graphics_context_set_stroke_color(ctx, settings.color_minor_tick);
      graphics_context_set_stroke_width(ctx, 1);
      tick_length = visible_circle_radius / 10;
    }
    GPoint minute_tick_inner = cartesian_from_polar(center, visible_circle_radius - tick_length, tick_deg);
    graphics_draw_line(ctx, minute_tick_inner, minute_tick_outer);
    if (tick_minute % 15 == 0) {
      graphics_context_set_stroke_color(ctx, settings.color_background);
      graphics_context_set_stroke_width(ctx, 1);
      graphics_draw_line(ctx, minute_tick_inner, minute_tick_outer);
    }
  }
}

static void draw_hour(GContext* ctx, GPoint center, int minute_deg, int visible_circle_radius, struct tm* now) {
#if BIG
  int shift_up = 7;
  GSize hour_bbox_size = GSize(80, 50);
  GFont hour_font = s_font_lg;
#else
  int shift_up = 0;
  GSize hour_bbox_size = GSize(80, 50);
  GFont hour_font = fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT);
#endif
  int inverted_minute_deg = 180 + minute_deg;
  GPoint hour_bbox_midpoint = cartesian_from_polar(center, visible_circle_radius * 8 / 20, inverted_minute_deg);
  GRect hour_bbox = rect_from_midpoint(hour_bbox_midpoint, hour_bbox_size);
  debug_bbox(ctx, hour_bbox);
  hour_bbox.origin.y -= shift_up;

  format_hour(now, s_buffer, BUFFER_LEN);
  graphics_context_set_text_color(ctx, settings.color_hour);
  graphics_draw_text(ctx, s_buffer, hour_font, hour_bbox, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void draw_hand(GContext* ctx, GPoint center, int minute_deg, int hand_length) {
  int hand_width = 12;
  graphics_context_set_stroke_width(ctx, 3);
  graphics_context_set_stroke_color(ctx, settings.color_hand);
  graphics_context_set_fill_color(ctx, settings.color_hand_inside);
  draw_arrow(ctx, hand_width, hand_length, DEG_TO_TRIGANGLE(minute_deg), center);
  // Circle at the base to smooth out the rotation
  graphics_context_set_fill_color(ctx, settings.color_hand);
  graphics_fill_circle(ctx, center, 3);
}

static void draw_date(GContext* ctx, GRect bounds, int visible_circle_radius, struct tm* now) {
  GFont date_font;
  if (bounds.size.h >= 200) {
    date_font = fonts_get_system_font(FONT_KEY_GOTHIC_28);
  } else {
    date_font = fonts_get_system_font(FONT_KEY_GOTHIC_24);
  }
  GRect date_bbox;
  date_bbox.origin = GPoint(3, 2 * visible_circle_radius - 8);
  date_bbox.size = GSize(bounds.size.w - 6, bounds.size.h - date_bbox.origin.y);
  if (date_bbox.size.h < 20) {
    return;
  }
  debug_bbox(ctx, date_bbox);
  format_day_of_week(now, s_buffer, BUFFER_LEN);
  graphics_context_set_text_color(ctx, settings.color_day_of_week);
  graphics_draw_text(ctx, s_buffer, date_font, date_bbox, GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  format_day_and_month(now, s_buffer, BUFFER_LEN);
  graphics_context_set_text_color(ctx, settings.color_month_date);
  graphics_draw_text(ctx, s_buffer, date_font, date_bbox, GTextOverflowModeFill, GTextAlignmentRight, NULL);
}

static void fill_rect_dither(GContext* ctx, GColor a, GColor b, GRect r) {
  int i = 0;
  for (int y = r.origin.y; y < r.origin.y + r.size.h; y++) {
    for (int x = r.origin.x; x < r.origin.x + r.size.w; x++) {
      if (i % 2) {
        graphics_context_set_stroke_color(ctx, a);
      } else {
        graphics_context_set_stroke_color(ctx, b);
      }
      graphics_draw_pixel(ctx, GPoint(x, y));
      i++;
    }
  }
}

static GPoint draw_battery(GContext* ctx, GRect bounds) {
  int w;
  int h;
  if (bounds.size.h > 200) { // bigger on emery
    w = 10;
    h = 34;
  } else {
    w = 8;
    h = 28;
  }
  int top = bounds.origin.y + 6;
  int bot = top + h;
  int lft = bounds.origin.x + 3;
  int rgt = lft + w;

  if (!gcolor_equal(settings.color_battery_outside, settings.color_background) ||
      !gcolor_equal(settings.color_battery_inside, settings.color_background)
  ) {
    graphics_context_set_stroke_color(ctx, settings.color_battery_outside);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_line(ctx, GPoint(lft, top), GPoint(rgt, top));
    graphics_draw_line(ctx, GPoint(lft, bot), GPoint(rgt, bot));
    graphics_draw_line(ctx, GPoint(lft, top), GPoint(lft, bot));
    graphics_draw_line(ctx, GPoint(rgt, top), GPoint(rgt, bot));
    graphics_draw_line(ctx, GPoint(lft + 2, top - 1), GPoint(rgt - 2, top - 1));  // like a AAA cap

    graphics_context_set_fill_color(ctx, settings.color_battery_inside);
    BatteryChargeState bcs = battery_state_service_peek();
    int fill_size = (h - 1) * bcs.charge_percent / 100;
    GRect fill_area = GRect(lft + 1, bot - fill_size, w - 1, fill_size);
    fill_rect_dither(ctx, settings.color_battery_inside, settings.color_background, fill_area);
  }
  return GPoint(rgt, top - 1);
}

#if BIG
#define PHONE_RASTER_Y (24)
#define PHONE_RASTER_X (12)
#else
#define PHONE_RASTER_Y (17)
#define PHONE_RASTER_X (12)
#endif

static void draw_bluetooth(GContext* ctx, GRect bounds, GPoint top_left) {
  bool bt_ok = connection_service_peek_pebble_app_connection();
  if (FORCE_BT_MISSING) {
    bt_ok = false;
  }
  if (bt_ok) {
    return;
  }
  // Draw the phone disconnected icon
  GPoint z = top_left;
  static const uint8_t phone_raster[PHONE_RASTER_Y][PHONE_RASTER_X] = {
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},
    {0,1,1,1,1,1,1,1,1,1,1,0},
#if BIG
    {0,1,1,1,1,1,1,1,1,1,1,0},
    {0,1,0,0,0,0,0,0,0,0,1,0},
    {0,1,0,0,0,0,0,0,0,0,1,0},
    {0,1,0,0,0,0,0,0,0,0,1,0},
#endif
    {0,1,2,0,0,0,0,0,0,2,1,0},
    {0,1,2,2,0,0,0,0,2,2,1,0},
    {0,1,0,2,2,0,0,2,2,0,1,0},
    {0,1,0,0,2,2,2,2,0,0,1,0},
    {0,1,0,0,0,2,2,0,0,0,1,0},
    {0,1,0,0,2,2,2,2,0,0,1,0},
    {0,1,0,2,2,0,0,2,2,0,1,0},
    {0,1,2,2,0,0,0,0,2,2,1,0},
    {0,1,2,0,0,0,0,0,0,2,1,0},
#if BIG
    {0,1,0,0,0,0,0,0,0,0,1,0},
    {0,1,0,0,0,0,0,0,0,0,1,0},
    {0,1,0,0,0,0,0,0,0,0,1,0},
#endif
    {0,1,1,1,1,1,1,1,1,1,1,0},
    {0,1,1,1,1,0,0,1,1,1,1,0},
    {0,1,1,1,1,0,0,1,1,1,1,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
  };
  for (int y = 0; y < PHONE_RASTER_Y; y++) {
    for (int x = 0; x < PHONE_RASTER_X; x++) {
      int pixel = phone_raster[y][x];
      if (pixel == 1) {
        graphics_context_set_stroke_color(ctx, GColorWhite);
      } else if (pixel == 2) {
        graphics_context_set_stroke_color(ctx, COLOR_FALLBACK(GColorRed, GColorWhite));
      } else {
        graphics_context_set_stroke_color(ctx, GColorBlack);
      }
      graphics_draw_pixel(ctx, GPoint(z.x + x, z.y + y));
    }
  }
}

static void draw_steps(GContext* ctx, GRect bounds, int vcr) {
  if (settings.step_goal == 0) {
    return;
  }
  int steps = health_service_sum_today(HealthMetricStepCount);
  if (steps < settings.step_goal / 3) {
    return;
  }
  int size = vcr - 1000 * vcr / 1414;
  GRect bbox = (GRect) {
    .origin = (GPoint) {
      .x = bounds.origin.x + bounds.size.w - size,
      .y = bounds.origin.y
    },
    .size = (GSize) {
      .w = size,
      .h = size
    }
  };

  // Need a contrasty background for light colors
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bbox, 3, GCornersAll);

#if BIG
  if (steps > settings.step_goal * 4 / 3) {
    draw_diamond(ctx, bbox);
  } else if (steps > settings.step_goal) {
    draw_star(ctx, bbox);
  } else if (steps > settings.step_goal * 2 / 3) {
    draw_shell(ctx, bbox);
  } else {
    draw_acorn(ctx, bbox);
  }
#else
  if (steps > settings.step_goal) {
    draw_star(ctx, bbox);
  }
#endif
}

static void update_layer(Layer* layer, GContext* ctx) {
  time_t temp = time(NULL);
  struct tm* now = localtime(&temp);
  if (DEBUG_TIME) {
    fast_forward_time(now);
  }
  GRect full_bounds = layer_get_bounds(layer);
  GRect bounds = layer_get_unobstructed_bounds(layer);
  bool timeline_quick_view = (bounds.size.h < full_bounds.size.h - 1);
  graphics_context_set_fill_color(ctx, settings.color_background);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  int visible_circle_radius = min(bounds.size.h, bounds.size.w) / 2;
#if IS_ROUND
  GPoint center = grect_center_point(&bounds);
#else
  GPoint center = GPoint(bounds.origin.x + bounds.size.w / 2, bounds.origin.y + visible_circle_radius + 2);
#endif
  int hand_length = visible_circle_radius - 6;
  int minute = now->tm_min;
  int minute_deg = 360 * minute / 60;

  draw_ticks(ctx, center, visible_circle_radius);
  draw_hour(ctx, center, minute_deg, visible_circle_radius, now);
  draw_hand(ctx, center, minute_deg, hand_length);
  if (IS_RECT && !timeline_quick_view) {
    draw_date(ctx, bounds, visible_circle_radius, now);
    GPoint batt_top_right = draw_battery(ctx, bounds);
    GPoint bt_top_left = GPoint(batt_top_right.x + 2, batt_top_right.y);
    draw_bluetooth(ctx, bounds, bt_top_left);
    draw_steps(ctx, bounds, visible_circle_radius);
  }
}

static void window_load(Window* window) {
  Layer* window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  window_set_background_color(s_window, settings.color_background);
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

static void load_settings() {
  default_settings();
  // If we need a new version of settings, check SETTINGS_VERSION_KEY and migrate
  int loaded_version = persist_read_int(SETTINGS_VERSION_KEY);
  persist_read_data(SETTINGS_KEY, &settings, sizeof(settings));
  if (loaded_version == 1) {
    settings.step_goal = DEFAULT_STEP_GOAL;
  }
  if (loaded_version == 2) {
    settings.color_middle_tick = DEFAULT_COLOR_MIDDLE_TICK;
  }
}

static void save_settings() {
  persist_write_int(SETTINGS_VERSION_KEY, 3);
  persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple* t;
  if ((t = dict_find(iter, MESSAGE_KEY_color_background      ))) settings.color_background       = GColorFromHEX(t->value->int32);
  if ((t = dict_find(iter, MESSAGE_KEY_color_major_tick      ))) settings.color_major_tick       = GColorFromHEX(t->value->int32);
  if ((t = dict_find(iter, MESSAGE_KEY_color_minor_tick      ))) settings.color_minor_tick       = GColorFromHEX(t->value->int32);
  if ((t = dict_find(iter, MESSAGE_KEY_color_hand            ))) settings.color_hand             = GColorFromHEX(t->value->int32);
  if ((t = dict_find(iter, MESSAGE_KEY_color_hand_inside     ))) settings.color_hand_inside      = GColorFromHEX(t->value->int32);
  if ((t = dict_find(iter, MESSAGE_KEY_color_hour            ))) settings.color_hour             = GColorFromHEX(t->value->int32);
  if ((t = dict_find(iter, MESSAGE_KEY_color_day_of_week     ))) settings.color_day_of_week      = GColorFromHEX(t->value->int32);
  if ((t = dict_find(iter, MESSAGE_KEY_color_month_date      ))) settings.color_month_date       = GColorFromHEX(t->value->int32);
  if ((t = dict_find(iter, MESSAGE_KEY_color_battery_inside  ))) settings.color_battery_inside   = GColorFromHEX(t->value->int32);
  if ((t = dict_find(iter, MESSAGE_KEY_color_battery_outside ))) settings.color_battery_outside  = GColorFromHEX(t->value->int32);
  if ((t = dict_find(iter, MESSAGE_KEY_step_goal             ))) settings.step_goal              = atoi(t->value->cstring);
  if ((t = dict_find(iter, MESSAGE_KEY_color_middle_tick     ))) settings.color_middle_tick      = GColorFromHEX(t->value->int32);
  save_settings();
  // Update the display based on new settings
  layer_mark_dirty(window_get_root_layer(s_window));
}

static void init(void) {
#if BIG
  s_font_lg = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_LATO_52));
#endif
  shapes_init();
  load_settings();
  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
  tick_timer_service_subscribe(DEBUG_TIME ? SECOND_UNIT : MINUTE_UNIT, tick_handler);
}

static void deinit(void) {
  if (s_window) window_destroy(s_window);
  if (s_font_lg) fonts_unload_custom_font(s_font_lg);
  shapes_deinit();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
