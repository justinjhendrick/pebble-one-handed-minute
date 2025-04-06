#include <pebble.h>

static Window* s_window;
static Layer* s_layer;
static GFont s_font;

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

static void update_layer(Layer* layer, GContext* ctx) {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  int32_t hand_width = 5;
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);
  int32_t circle_radius = min(bounds.size.h, bounds.size.w) / 2 - 1;
  int32_t minute_hand_length = circle_radius - hand_width / 2;
  int32_t minute_deg = 360 * tick_time->tm_sec / 60;
  GPoint minute_tip = cartesian_from_polar(center, minute_hand_length, minute_deg);

  // global graphics settings
  graphics_context_set_antialiased(ctx, true);

  // draw minute hand
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, hand_width);
  graphics_draw_line(ctx, center, minute_tip);

  // draw analog face circular edge
  graphics_context_set_fill_color(ctx, GColorClear);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, center, circle_radius);

  // Draw hour number
  static char s_buffer[3];
  if (clock_is_24h_style()) {
    snprintf(s_buffer, sizeof(s_buffer), "%d", tick_time->tm_hour);
  } else {
    snprintf(s_buffer, sizeof(s_buffer), "%d", tick_time->tm_hour % 12);
  }
  GSize hour_bbox_size = GSize(50, 50);
  int32_t inverted_minute_deg = 180 + minute_deg;
  GPoint hour_bbox_midpoint = cartesian_from_polar(center, circle_radius / 2, inverted_minute_deg);
  GRect hour_bbox = rect_from_midpoint(hour_bbox_midpoint, hour_bbox_size);
  graphics_draw_text(ctx, s_buffer, s_font, hour_bbox, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
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


static void tick_handler(struct tm* tick_time, TimeUnits units_changed) {
  layer_mark_dirty(window_get_root_layer(s_window));
}

static void init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  s_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_NEONEON_FONT_42));
}

static void deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
