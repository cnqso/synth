#ifndef UI_H
#define UI_H

#include "synth.h"

const char *roll_view_name(int roll_view);
int roll_view_right_arrow_x(int roll_view);
DrumGridLayout drum_grid_layout(void);
void clear_drum_pattern(Synth *s);
void init_drum_pattern(Synth *s);
void draw_ui(Synth *s);
int slider_hit(int mx, int my);
void slider_update(Synth *s, int idx, int my);
int wave_hit(Synth *s, int mx, int my);
void wave_update(Synth *s, int idx, int mx, int my);
int drum_grid_hit(int mx, int my, int *out_row, int *out_step);
int piano_mouse_note(Synth *s, int mx, int my);

#endif
