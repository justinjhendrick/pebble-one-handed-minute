#include <pebble.h>

static Window* s_window;
static Layer* s_hour_layer;
static Layer* s_minute_layer;

static int32_t min(int32_t a, int32_t b) {
  if (a < b) {
    return a;
  }
  return b;
}

static void update_minute_layer(Layer* layer, GContext* ctx) {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  int32_t hand_width = 5;
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);
  int32_t circle_radius = min(bounds.size.h, bounds.size.w) / 2 - 1;
  int32_t minute_hand_length = circle_radius - hand_width / 2;
  int32_t minute_angle = TRIG_MAX_ANGLE * tick_time->tm_min / 60;
  GPoint minute_tip = {
    .x = (int16_t)(sin_lookup(minute_angle) * (int32_t)minute_hand_length / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(minute_angle) * (int32_t)minute_hand_length / TRIG_MAX_RATIO) + center.y,
  };

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
}

static void update_hour_layer(Layer* layer, GContext* ctx) {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  graphics_context_set_fill_color(ctx, GColorClear);
  graphics_context_set_stroke_color(ctx, GColorWhite);

  static char s_buffer[3];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ?  "%H" : "%I", tick_time);

  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);
  uint32_t h = 50;
  uint32_t w = 50;
  
  GRect box = GRect(center.x - w / 2, center.y - h / 2, w, h);
  GFont font = fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
  graphics_draw_text(ctx, s_buffer, font, box, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void window_load(Window* window) {
  // Get information about the Window
  Layer* window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  window_set_background_color(s_window, GColorBlack);

  // Add hour layer
  s_hour_layer = layer_create(bounds);
  layer_set_update_proc(s_hour_layer, update_hour_layer);
  layer_add_child(window_layer, s_hour_layer);

  // Add minute layer
  s_minute_layer = layer_create(bounds);
  layer_set_update_proc(s_minute_layer, update_minute_layer);
  layer_add_child(window_layer, s_minute_layer);
}

static void window_unload(Window* window) {
  layer_destroy(s_hour_layer);
  layer_destroy(s_minute_layer);
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
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
