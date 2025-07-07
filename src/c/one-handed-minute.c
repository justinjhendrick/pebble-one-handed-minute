#include <pebble.h>
#include "utils.h"

#define BUFFER_LEN (10)
#define DEBUG_TIME (false)

#define SETTINGS_VERSION_KEY 1
#define SETTINGS_KEY 2

// TODO: battery charge indicator
// TODO: bluetooth disconnect indicator?
// TODO: silent mode indicator?

static const GPathInfo ARROW_POINTS = {
  .num_points = 4,
  // points will be filled by change_arrow_size
  .points = (GPoint []) { {0, 0}, {0, 0}, {0, 0}, {0, 0} }
};

typedef struct ClaySettings {
  GColor color_background;
  GColor color_major_tick;
  GColor color_minor_tick;
  GColor color_hand;
  GColor color_hour;
  GColor color_day_of_week;
  GColor color_month_date;
  // TODO: reserve some spots in this config before release
} __attribute__((__packed__)) ClaySettings;

ClaySettings settings;

static void default_settings() {
  settings.color_background = GColorBlack;
  settings.color_major_tick = GColorWhite;
  settings.color_minor_tick = GColorWhite;
  settings.color_hand = GColorWhite;
  settings.color_hour = GColorWhite;
  settings.color_day_of_week = GColorWhite;
  settings.color_month_date = GColorWhite;
}

static Window* s_window;
static Layer* s_layer;
static char s_buffer[BUFFER_LEN];
static GPath* s_arrow;

static void draw_ticks(GContext* ctx, GPoint center, int visible_circle_radius) {
  for (int16_t tick_minute = 0; tick_minute < 60; tick_minute += 1) {
    int tick_deg = tick_minute * 360 / 60;
    GPoint minute_tick_outer = cartesian_from_polar(center, visible_circle_radius, tick_deg);
    int tick_length = 1;
    if (tick_minute % 15 == 0) {
      graphics_context_set_stroke_color(ctx, settings.color_major_tick);
      graphics_context_set_stroke_width(ctx, 3);
      tick_length = 2 * visible_circle_radius / 10;
    } else if (tick_minute % 5 == 0) {
      graphics_context_set_stroke_color(ctx, settings.color_minor_tick);
      graphics_context_set_stroke_width(ctx, 1);
      tick_length = visible_circle_radius / 10;
    } else {
      graphics_context_set_stroke_color(ctx, settings.color_minor_tick);
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
  format_hour(now, s_buffer, BUFFER_LEN);
  graphics_context_set_text_color(ctx, settings.color_hour);
  graphics_draw_text(ctx, s_buffer, hour_font, hour_bbox, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void change_arrow_size(int w, int h) {
  ARROW_POINTS.points[0].x = 0;
  ARROW_POINTS.points[0].y = 0;

  ARROW_POINTS.points[1].x = w / 2;
  ARROW_POINTS.points[1].y = -h * 3 / 10;

  ARROW_POINTS.points[2].x = 0;
  ARROW_POINTS.points[2].y = -h;

  ARROW_POINTS.points[3].x = -w / 2;
  ARROW_POINTS.points[3].y = -h * 3 / 10;
}

static void draw_hand(GContext* ctx, GPoint center, int minute_deg, int hand_length) {
  int hand_width = 12;
  graphics_context_set_stroke_width(ctx, 3);
  graphics_context_set_stroke_color(ctx, settings.color_hand);
  change_arrow_size(hand_width, hand_length);
  gpath_rotate_to(s_arrow, DEG_TO_TRIGANGLE(minute_deg));
  gpath_move_to(s_arrow, center);
  gpath_draw_outline(ctx, s_arrow);
  // Circle at the base to smooth out the rotation
  graphics_context_set_fill_color(ctx, settings.color_hand);
  graphics_fill_circle(ctx, center, 3);
}

static void draw_date(GContext* ctx, GRect bounds, int visible_circle_radius, struct tm* now) {
  // TODO: bigger font on emery
  GFont date_font = fonts_get_system_font(FONT_KEY_GOTHIC_24);
  GRect date_bbox;
  date_bbox.origin = GPoint(2, 2 * visible_circle_radius - 8);
  date_bbox.size = GSize(bounds.size.w - 4, bounds.size.h - date_bbox.origin.y);
  format_day_of_week(now, s_buffer, BUFFER_LEN);
  graphics_context_set_text_color(ctx, settings.color_day_of_week);
  graphics_draw_text(ctx, s_buffer, date_font, date_bbox, GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  format_day_and_month(now, s_buffer, BUFFER_LEN);
  graphics_context_set_text_color(ctx, settings.color_month_date);
  graphics_draw_text(ctx, s_buffer, date_font, date_bbox, GTextOverflowModeFill, GTextAlignmentRight, NULL);
}

static void update_layer(Layer* layer, GContext* ctx) {
  time_t temp = time(NULL);
  struct tm* now = localtime(&temp);
  if (DEBUG_TIME) {
    fast_forward_time(now);
  }
  GRect bounds = layer_get_bounds(layer);
  window_set_background_color(s_window, settings.color_background);
  int visible_circle_radius = min(bounds.size.h, bounds.size.w) / 2 - 2;
  GPoint center = GPoint(visible_circle_radius + 1, visible_circle_radius + 1);
  int hand_length = visible_circle_radius - 6;
  int minute = now->tm_min;
  int minute_deg = 360 * minute / 60;

  draw_ticks(ctx, center, visible_circle_radius);
  draw_hour(ctx, center, minute_deg, visible_circle_radius, now);
  draw_date(ctx, bounds, visible_circle_radius, now);
  draw_hand(ctx, center, minute_deg, hand_length);
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
  persist_read_data(SETTINGS_KEY, &settings, sizeof(settings));
}

static void save_settings() {
  persist_write_int(SETTINGS_VERSION_KEY, 1);
  persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple* t;
  if ((t = dict_find(iter, MESSAGE_KEY_color_background ))) settings.color_background  = GColorFromHEX(t->value->int32);
  if ((t = dict_find(iter, MESSAGE_KEY_color_major_tick ))) settings.color_major_tick  = GColorFromHEX(t->value->int32);
  if ((t = dict_find(iter, MESSAGE_KEY_color_minor_tick ))) settings.color_minor_tick  = GColorFromHEX(t->value->int32);
  if ((t = dict_find(iter, MESSAGE_KEY_color_hand       ))) settings.color_hand        = GColorFromHEX(t->value->int32);
  if ((t = dict_find(iter, MESSAGE_KEY_color_hour       ))) settings.color_hour        = GColorFromHEX(t->value->int32);
  if ((t = dict_find(iter, MESSAGE_KEY_color_day_of_week))) settings.color_day_of_week = GColorFromHEX(t->value->int32);
  if ((t = dict_find(iter, MESSAGE_KEY_color_month_date ))) settings.color_month_date  = GColorFromHEX(t->value->int32);
  save_settings();
  // Update the display based on new settings
  layer_mark_dirty(window_get_root_layer(s_window));
}

static void init(void) {
  load_settings();
  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());

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
  if (s_arrow) gpath_destroy(s_arrow);
  if (s_window) window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
