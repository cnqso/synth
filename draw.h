#ifndef DRAW_H
#define DRAW_H

#include "synth.h"

void load_font(void);
void draw_text(SDL_Surface *s, int x, int y, const char *str, Uint32 col);
void draw_text_sm(SDL_Surface *s, int x, int y, const char *str, Uint32 col);
int text_width_sm(const char *str);
void fill_rect(SDL_Surface *s, int x, int y, int w, int h, Uint32 col);
void draw_hline(SDL_Surface *s, int x1, int x2, int y, Uint32 col);
void draw_vline(SDL_Surface *s, int x, int y1, int y2, Uint32 col);
void draw_line(SDL_Surface *s, int x0, int y0, int x1, int y1, Uint32 col);
void draw_rect_outline(SDL_Surface *s, int x, int y, int w, int h, Uint32 col);

#endif
