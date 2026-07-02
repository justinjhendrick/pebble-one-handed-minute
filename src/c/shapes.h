#ifndef SHAPES_H
#define SHAPES_H

void draw_arrow(GContext* ctx, int w, int h, int rotation, GPoint offset);
void draw_diamond(GContext* ctx, GRect bbox);
void draw_star(GContext* ctx, GRect bbox);
void draw_shell(GContext* ctx, GRect bbox);
void draw_acorn(GContext* ctx, GRect bbox);

#endif
