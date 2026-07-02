#include <pebble.h>

#include "shapes.h"

#define MAX_POINTS (40)
static GPoint s_points[MAX_POINTS];

static void draw_shape(GContext* ctx, uint32_t num_points, int rotation, GPoint offset) {
  GPath shape = (GPath) {
    .num_points=num_points,
    .points=s_points,
    .rotation=rotation,
    .offset=offset
  };
  gpath_draw_filled(ctx, &shape);
  gpath_draw_outline(ctx, &shape);
}

void draw_arrow(GContext* ctx, int w, int h, int rotation, GPoint offset) {
  s_points[0].x = 0;
  s_points[0].y = 0;

  s_points[1].x = w / 2;
  s_points[1].y = -h * 3 / 10;

  s_points[2].x = 0;
  s_points[2].y = -h;

  s_points[3].x = -w / 2;
  s_points[3].y = -h * 3 / 10;
  draw_shape(ctx, 4, rotation, offset);
}

void draw_diamond(GContext* ctx, GRect bbox) {
  int width = bbox.size.w - 5;
  int height = bbox.size.h - 5;
  GPoint start = GPoint(bbox.origin.x + 3, bbox.origin.y + height / 6 + 5);
  s_points[0] = start;

  s_points[1].x = start.x + width / 6;
  s_points[1].y = start.y - width / 6;

  s_points[2].x = start.x + width * 5 / 6;
  s_points[2].y = start.y - width / 6;

  s_points[3].x = start.x + width;
  s_points[3].y = start.y;

  s_points[4].x = start.x + width / 2;
  s_points[4].y = start.y + height * 4 / 6;

  GColor color = COLOR_FALLBACK(GColorElectricBlue, GColorWhite);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_fill_color(ctx, color);
  draw_shape(ctx, 5, 0, GPoint(0, 0));

  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_line(ctx, s_points[0], s_points[3]);

  GPoint p5 = GPoint(start.x + width / 4, start.y);
  GPoint p6 = GPoint(start.x + width * 3 / 4, start.y);
  GPoint p7 = GPoint((s_points[1].x + s_points[2].x) / 2, s_points[1].y);

  graphics_draw_line(ctx, p5, s_points[4]);
  graphics_draw_line(ctx, p5, s_points[1]);
  graphics_draw_line(ctx, p5, p7);

  graphics_draw_line(ctx, p6, s_points[4]);
  graphics_draw_line(ctx, p6, s_points[2]);
  graphics_draw_line(ctx, p6, p7);
}

void draw_star(GContext* ctx, GRect bbox) {
  GPoint start = GPoint(bbox.origin.x + 1, bbox.origin.y + bbox.size.h / 3 + 3);
  int len = bbox.size.w - 2;
  uint32_t num_points = 5;
  s_points[0] = start;
  int angle = DEG_TO_TRIGANGLE(0);
  for (uint32_t i = 1; i < num_points; i++) {
    GPoint* curr = s_points + i - 1;
    GPoint* next = s_points + i;
    next->x = curr->x + len * cos_lookup(angle) / TRIG_MAX_RATIO;
    next->y = curr->y + len * sin_lookup(angle) / TRIG_MAX_RATIO;
    angle += DEG_TO_TRIGANGLE(144);
  }

  GColor color = COLOR_FALLBACK(GColorYellow, GColorWhite);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_fill_color(ctx, color);
  draw_shape(ctx, num_points, 0, GPoint(0, 0));
}

void draw_shell(GContext* ctx, GRect bbox) {
  GColor pearl = COLOR_FALLBACK(GColorRichBrilliantLavender, GColorWhite);
  GPoint center = grect_center_point(&bbox);
  center.x -= 1;
  graphics_context_set_fill_color(ctx, pearl);
  int r = bbox.size.w / 2 - 2;
  graphics_fill_circle(ctx, center, r);

  graphics_context_set_fill_color(ctx, GColorWhite);
  GPoint shine = GPoint(center.x - r / 2 + 2, center.y - r / 2 + 2);
  graphics_fill_circle(ctx, shine, r / 3);
}

void draw_acorn(GContext* ctx, GRect bbox) {
  int width = bbox.size.w - 5;
  int height = bbox.size.h - 5;
  GPoint start = GPoint(bbox.origin.x + 2, bbox.origin.y + height / 6 + 7);

  s_points[0] = start;

  s_points[1].x = start.x + width / 6;
  s_points[1].y = start.y - width / 6;

  s_points[2].x = start.x + width * 5 / 6;
  s_points[2].y = start.y - width / 6;

  s_points[3].x = start.x + width;
  s_points[3].y = start.y;

  s_points[4].x = s_points[3].x;
  s_points[4].y = start.y + 1;

  s_points[5].x = s_points[2].x;
  s_points[5].y = s_points[4].y;

  s_points[6].x = s_points[5].x;
  s_points[6].y = start.y + width * 3 / 6;

  s_points[7].x = s_points[6].x - width / 6;
  s_points[7].y = s_points[6].y + width / 6;

  s_points[8].x = s_points[7].x - width * 2 / 6;
  s_points[8].y = s_points[7].y;

  s_points[9].x = s_points[8].x - width / 6;
  s_points[9].y = s_points[8].y - width / 6;

  s_points[10].x = s_points[9].x;
  s_points[10].y = s_points[4].y;

  s_points[11].x = s_points[0].x;
  s_points[11].y = s_points[10].y;

  GColor color = COLOR_FALLBACK(GColorWindsorTan, GColorWhite);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_fill_color(ctx, color);
  draw_shape(ctx, 12, 0, GPoint(0, 0));

  GPoint top = GPoint((s_points[1].x + s_points[2].x) / 2, s_points[1].y - 2);
  GPoint tip = GPoint(top.x - 2, top.y - 2);
  graphics_draw_line(ctx, top, tip);

  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, GColorBlack);

  GPoint p12 = GPoint(s_points[11].x, s_points[11].y + 2);
  GPoint p13 = GPoint(s_points[4].x, p12.y);
  graphics_draw_line(ctx, p12, p13);
}
