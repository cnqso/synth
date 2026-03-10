#include "ui.h"

#include <stdlib.h>
#include <string.h>

#include "draw.h"

static const char *osc_names[] = {"SIN", "SAW", "SQR", "TRI", "CUS"};
static const char *tun_names[] = {"12T", "JST", "PYT", "7ET", "SLN", "CNQ"};
static const char *slider_names[] = {"VOL", "ATK", "DEC", "SUS", "REL", "CUT", "RES", "CNQ"};
static const char *drum_names[] = {
    "BAS", "SNR", "HAT", "CLP", "RDE", "TOM", "CGA", "SHK", "COW", "STK", "CRS"
};
static const char *roll_view_names[] = {"PIANO", "BEATS", "DRUMS"};

const char *roll_view_name(int roll_view) {
    if (roll_view < 0 || roll_view >= 3) return roll_view_names[0];
    return roll_view_names[roll_view];
}

int roll_view_right_arrow_x(int roll_view) {
    const char *name = roll_view_name(roll_view);
    return 14 + text_width_sm(name) + 5;
}

DrumGridLayout drum_grid_layout(void) {
    DrumGridLayout l;
    l.top = ROLL_Y + 13;
    l.bot = ROLL_Y + ROLL_H - 1;
    l.row_h = (l.bot - l.top) / DRUM_TYPE_COUNT;
    if (l.row_h < 1) l.row_h = 1;
    l.label_w = 26;
    l.grid_x = l.label_w + 2;
    l.cell_w = (SCREEN_W - l.grid_x - 4) / SEQ_STEPS;
    if (l.cell_w < 1) l.cell_w = 1;
    return l;
}

void clear_drum_pattern(Synth *s) {
    memset(s->drum_pattern, 0, sizeof(s->drum_pattern));
}

void init_drum_pattern(Synth *s) {
    clear_drum_pattern(s);
    s->drum_pattern[DRUM_KICK][0] = 1;
    s->drum_pattern[DRUM_KICK][6] = 1;
    s->drum_pattern[DRUM_KICK][8] = 1;
    s->drum_pattern[DRUM_KICK][11] = 1;
    s->drum_pattern[DRUM_SNARE][4] = 1;
    s->drum_pattern[DRUM_SNARE][12] = 1;
    for (int step = 0; step < SEQ_STEPS; step += 2)
        s->drum_pattern[DRUM_HIHAT][step] = 1;
    s->drum_pattern[DRUM_CLAP][12] = 1;
    s->drum_pattern[DRUM_SHAKER][3] = 1;
    s->drum_pattern[DRUM_SHAKER][7] = 1;
    s->drum_pattern[DRUM_SHAKER][11] = 1;
    s->drum_pattern[DRUM_SHAKER][15] = 1;
}

void draw_ui(Synth *s) {
    SDL_Surface *sf = s->surf;
    fill_rect(sf, 0, 0, SCREEN_W, SCREEN_H, COL_BG);

    fill_rect(sf, 0, BAR_Y, SCREEN_W, BAR_H, COL_PANEL);
    draw_hline(sf, 0, SCREEN_W - 1, BAR_H - 1, COL_BORDER);
    draw_text(sf, 4, 2, "cnqsosynth", COL_DARK);

    for (int i = 0; i < OSC_COUNT; i++) {
        int bx = 100 + i * 32;
        Uint32 c = (s->osc == i) ? COL_PRESSED : COL_PANEL;
        fill_rect(sf, bx, 2, 28, 14, c);
        draw_rect_outline(sf, bx, 2, 28, 14, COL_BORDER);
        draw_text(sf, bx + 4, 2, osc_names[i], COL_TEXT);
    }

    for (int i = 0; i < TUN_COUNT; i++) {
        int bx = 260 + i * 26;
        Uint32 c = (s->tuning == i) ? COL_PRESSED : COL_PANEL;
        fill_rect(sf, bx, 2, 24, 14, c);
        draw_rect_outline(sf, bx, 2, 24, 14, COL_BORDER);
        draw_text(sf, bx + 2, 2, tun_names[i], COL_TEXT);
    }

    {
        int bx = 430;
        Uint32 c = s->recording ? COL_ACCENT : COL_PANEL;
        fill_rect(sf, bx, 2, 28, 14, c);
        draw_rect_outline(sf, bx, 2, 28, 14, COL_BORDER);
        draw_text(sf, bx + 4, 2, "REC", COL_TEXT);
    }

    if (s->rec_len > 0 && !s->recording) {
        int bx = 460;
        Uint32 c = s->rec_playing ? COL_ACCENT : COL_PANEL;
        fill_rect(sf, bx, 2, 22, 14, c);
        draw_rect_outline(sf, bx, 2, 22, 14, COL_BORDER);
        draw_text(sf, bx + 3, 2, s->rec_playing ? "STP" : "PLY", COL_TEXT);
    }

    {
        int bx = 490;
        Uint32 c = s->midi_loaded ? COL_PRESSED : COL_PANEL;
        fill_rect(sf, bx, 2, 28, 14, c);
        draw_rect_outline(sf, bx, 2, 28, 14, COL_BORDER);
        draw_text(sf, bx + 3, 2, "MID", COL_TEXT);
    }

    if (s->midi_loaded) {
        int bx = 520;
        Uint32 c = s->midi_playing ? COL_ACCENT : COL_PANEL;
        fill_rect(sf, bx, 2, 22, 14, c);
        draw_rect_outline(sf, bx, 2, 22, 14, COL_BORDER);
        draw_text(sf, bx + 3, 2, s->midi_playing ? "STP" : "PLY", COL_TEXT);
    }

    {
        int ox = 550;
        char obuf[8];
        fill_rect(sf, ox, 2, 14, 14, COL_PANEL);
        draw_rect_outline(sf, ox, 2, 14, 14, COL_BORDER);
        draw_text(sf, ox + 4, 2, "-", COL_TEXT);
        SDL_snprintf(obuf, sizeof(obuf), "C%d", s->octave);
        draw_text(sf, ox + 18, 2, obuf, COL_DARK);
        fill_rect(sf, ox + 34, 2, 14, 14, COL_PANEL);
        draw_rect_outline(sf, ox + 34, 2, 14, 14, COL_BORDER);
        draw_text(sf, ox + 38, 2, "+", COL_TEXT);
    }

    fill_rect(sf, 0, WAVE_ED_Y, SCREEN_W, WAVE_ED_H, COL_PANEL);
    draw_hline(sf, 0, SCREEN_W - 1, WAVE_ED_Y + WAVE_ED_H - 1, COL_BORDER);
    draw_text(sf, 4, WAVE_ED_Y + 2, "WAVEFORM", COL_TEXT);
    draw_hline(sf, 0, SCREEN_W - 1, WAVE_ED_Y + WAVE_ED_H / 2, COL_BORDER);

    for (int i = 0; i < WAVE_PTS - 1; i++) {
        int x0 = (int)(s->waveform[i].x * (SCREEN_W - 1));
        int y0 = WAVE_ED_Y + 10 + (int)((1.0f - s->waveform[i].y) * (WAVE_ED_H - 20));
        int x1 = (int)(s->waveform[i + 1].x * (SCREEN_W - 1));
        int y1 = WAVE_ED_Y + 10 + (int)((1.0f - s->waveform[i + 1].y) * (WAVE_ED_H - 20));
        draw_line(sf, x0, y0, x1, y1, COL_WAVE);
    }

    for (int i = 0; i < WAVE_PTS; i++) {
        int px = (int)(s->waveform[i].x * (SCREEN_W - 1));
        int py = WAVE_ED_Y + 10 + (int)((1.0f - s->waveform[i].y) * (WAVE_ED_H - 20));
        fill_rect(sf, px - 2, py - 2, 5, 5, COL_WHITE);
        draw_rect_outline(sf, px - 2, py - 2, 5, 5, COL_BORDER);
    }

    fill_rect(sf, 0, SCOPE_Y, SCREEN_W, SCOPE_H, COL_PANEL);
    draw_hline(sf, 0, SCREEN_W - 1, SCOPE_Y + SCOPE_H - 1, COL_BORDER);
    draw_text(sf, 4, SCOPE_Y + 2, "SCOPE", COL_TEXT);
    draw_hline(sf, 0, SCREEN_W - 1, SCOPE_Y + SCOPE_H / 2, COL_BORDER);

    {
        int base = s->scope_pos > SCREEN_W ? s->scope_pos - SCREEN_W : 0;
        for (int x = 1; x < SCREEN_W; x++) {
            float v0 = s->scope_buf[(base + x - 1) % SCREEN_W];
            float v1 = s->scope_buf[(base + x) % SCREEN_W];
            int y0 = SCOPE_Y + SCOPE_H / 2 - (int)(v0 * (SCOPE_H / 2 - 4));
            int y1 = SCOPE_Y + SCOPE_H / 2 - (int)(v1 * (SCOPE_H / 2 - 4));
            draw_line(sf, x - 1, y0, x, y1, COL_WAVE);
        }
    }

    fill_rect(sf, 0, ROLL_Y, SCREEN_W, ROLL_H, COL_PANEL);
    draw_hline(sf, 0, SCREEN_W - 1, ROLL_Y + ROLL_H - 1, COL_BORDER);
    draw_text_sm(sf, 4, ROLL_Y + 2, "<", COL_DARK);

    {
        const char *vn = roll_view_name(s->roll_view);
        int vn_x = 14;
        int vn_w = text_width_sm(vn) + 1;
        draw_text_sm(sf, vn_x, ROLL_Y + 2, vn, COL_TEXT);
        draw_text_sm(sf, vn_x + vn_w + 4, ROLL_Y + 2, ">", COL_DARK);
    }

    float px_per_sec = 80.0f;
    int roll_top = ROLL_Y + 12;
    int roll_bot = ROLL_Y + ROLL_H - 2;
    int roll_inner = roll_bot - roll_top;

    if (s->roll_view == 0) {
        int note_lo = s->octave * 12;
        int note_hi = note_lo + 36;
        if (note_hi > 127) {
            note_hi = 127;
            note_lo = 91;
        }
        int note_range = note_hi - note_lo;
        if (note_range < 1) note_range = 1;

        for (int i = 0; i <= 12; i++) {
            int ry = roll_top + i * roll_inner / 12;
            draw_hline(sf, 0, SCREEN_W - 1, ry, COL_BORDER);
        }
        for (int x = 0; x < SCREEN_W; x += 40)
            draw_vline(sf, x, roll_top, roll_bot, COL_BORDER);

        for (int i = 0; i < s->roll_count; i++) {
            RollNote *rn = &s->roll[i];
            if (rn->is_drum) continue;
            int rx = (int)((rn->start - s->roll_scroll) * px_per_sec);
            int rw = (int)(rn->dur * px_per_sec);
            if (rw < 2) rw = 2;
            if (rx + rw < 0 || rx > SCREEN_W) continue;
            int note_rel = rn->note - note_lo;
            if (note_rel < 0 || note_rel >= note_range) continue;
            int ry = roll_top + roll_inner - (note_rel * roll_inner / note_range);
            fill_rect(sf, rx, ry - 1, rw, 3, COL_WAVE);
        }
    } else if (s->roll_view == 1) {
        DrumGridLayout grid = drum_grid_layout();
        int play_x = 74;
        int clear_x = 102;
        int bpm_dn_x = 132;
        int bpm_up_x = 190;
        char bpm_buf[12];

        Uint32 play_col = s->seq_playing ? COL_ACCENT : COL_PANEL;
        fill_rect(sf, play_x, ROLL_Y + 1, 24, 10, play_col);
        draw_rect_outline(sf, play_x, ROLL_Y + 1, 24, 10, COL_BORDER);
        draw_text_sm(sf, play_x + 4, ROLL_Y + 3, s->seq_playing ? "STP" : "RUN", COL_TEXT);

        fill_rect(sf, clear_x, ROLL_Y + 1, 24, 10, COL_PANEL);
        draw_rect_outline(sf, clear_x, ROLL_Y + 1, 24, 10, COL_BORDER);
        draw_text_sm(sf, clear_x + 4, ROLL_Y + 3, "CLR", COL_TEXT);

        fill_rect(sf, bpm_dn_x, ROLL_Y + 1, 12, 10, COL_PANEL);
        draw_rect_outline(sf, bpm_dn_x, ROLL_Y + 1, 12, 10, COL_BORDER);
        draw_text_sm(sf, bpm_dn_x + 4, ROLL_Y + 3, "-", COL_TEXT);

        SDL_snprintf(bpm_buf, sizeof(bpm_buf), "%d", (int)s->seq_tempo);
        draw_text_sm(sf, bpm_dn_x + 18, ROLL_Y + 3, bpm_buf, COL_DARK);
        draw_text_sm(sf, bpm_dn_x + 36, ROLL_Y + 3, "BPM", COL_TEXT);

        fill_rect(sf, bpm_up_x, ROLL_Y + 1, 12, 10, COL_PANEL);
        draw_rect_outline(sf, bpm_up_x, ROLL_Y + 1, 12, 10, COL_BORDER);
        draw_text_sm(sf, bpm_up_x + 4, ROLL_Y + 3, "+", COL_TEXT);

        for (int step = 0; step < SEQ_STEPS; step++) {
            int sx = grid.grid_x + step * grid.cell_w;
            Uint32 step_col = ((step & 3) == 0) ? COL_BORDER : COL_PANEL;
            if (s->seq_playing && step == s->seq_step)
                fill_rect(sf, sx, grid.top, grid.cell_w, grid.row_h * DRUM_TYPE_COUNT, COL_PRESSED);
            draw_vline(sf, sx, grid.top, grid.bot, step_col);
        }
        draw_vline(sf, grid.grid_x + grid.cell_w * SEQ_STEPS, grid.top, grid.bot, COL_BORDER);

        for (int row = 0; row < DRUM_TYPE_COUNT; row++) {
            int ry = grid.top + row * grid.row_h;
            draw_hline(sf, 0, SCREEN_W - 1, ry, COL_BORDER);
            fill_rect(sf, 0, ry + 1, grid.label_w, grid.row_h - 1, COL_BG);
            draw_rect_outline(sf, 0, ry + 1, grid.label_w, grid.row_h - 1, COL_BORDER);
            draw_text_sm(sf, 2, ry + (grid.row_h - 5) / 2, drum_names[s->drum_row_type[row]], COL_DARK);

            for (int step = 0; step < SEQ_STEPS; step++) {
                int sx = grid.grid_x + step * grid.cell_w + 1;
                int sy = ry + 1;
                int cell_w = grid.cell_w - 2;
                int cell_h = grid.row_h - 2;
                Uint32 fill = ((step & 3) == 0) ? COL_PANEL : COL_BG;
                if (s->drum_pattern[row][step])
                    fill = (s->seq_playing && step == s->seq_step) ? COL_ACCENT : COL_WAVE;
                fill_rect(sf, sx, sy, cell_w, cell_h, fill);
                draw_rect_outline(sf, sx, sy, cell_w, cell_h, COL_BORDER);
            }
        }
        draw_hline(sf, 0, SCREEN_W - 1, grid.bot, COL_BORDER);
    } else {
        DrumGridLayout grid = drum_grid_layout();

        for (int d = 0; d < DRUM_TYPE_COUNT; d++) {
            int ry = grid.top + d * grid.row_h;
            draw_hline(sf, 0, SCREEN_W - 1, ry, COL_BORDER);
            fill_rect(sf, 0, ry + 1, grid.label_w, grid.row_h - 1, COL_BG);
            draw_rect_outline(sf, 0, ry + 1, grid.label_w, grid.row_h - 1, COL_BORDER);
            draw_text_sm(sf, 2, ry + (grid.row_h - 5) / 2, drum_names[s->drum_row_type[d]], COL_DARK);
        }
        draw_hline(sf, 0, SCREEN_W - 1, grid.bot, COL_BORDER);

        for (int step = 0; step <= SEQ_STEPS; step++) {
            int sx = grid.grid_x + step * grid.cell_w;
            Uint32 col = (step < SEQ_STEPS && (step & 3) == 0) ? COL_BORDER : COL_PANEL;
            draw_vline(sf, sx, grid.top, grid.bot, col);
        }

        for (int i = 0; i < s->roll_count; i++) {
            RollNote *rn = &s->roll[i];
            if (!rn->is_drum) continue;
            int rx = (int)((rn->start - s->roll_scroll) * px_per_sec) + grid.label_w;
            if (rx < grid.label_w || rx > SCREEN_W) continue;
            int d = (int)rn->drum;
            if (d < 0 || d >= DRUM_TYPE_COUNT) continue;
            int ry = grid.top + d * grid.row_h + grid.row_h / 2 - 1;
            fill_rect(sf, rx, ry, 4, 3, COL_ACCENT);
        }
    }

    if (s->midi_playing) {
        int px = (int)((s->midi_time - s->roll_scroll) * px_per_sec);
        if (s->roll_view == 2) px += drum_grid_layout().label_w;
        if (px >= 0 && px < SCREEN_W) draw_vline(sf, px, roll_top, roll_bot, COL_ACCENT);
    }

    fill_rect(sf, 0, SLIDER_Y, SCREEN_W, SLIDER_H, COL_BG);
    draw_hline(sf, 0, SCREEN_W - 1, SLIDER_Y + SLIDER_H - 1, COL_BORDER);

    float slider_vals[8] = {
        s->volume, s->adsr.a / 2.0f, s->adsr.d / 2.0f, s->adsr.s,
        s->adsr.r / 2.0f, s->cutoff, s->resonance, s->claudiness
    };
    int sw = SCREEN_W / 8;
    for (int i = 0; i < 8; i++) {
        int cx = sw * i + sw / 2;
        int ty = SLIDER_Y + 12;
        int by = SLIDER_Y + SLIDER_H - 16;
        int h = by - ty;
        draw_vline(sf, cx, ty, by, COL_BORDER);

        float v = slider_vals[i];
        if (v < 0) v = 0;
        if (v > 1) v = 1;
        int ky = by - (int)(v * h);
        fill_rect(sf, cx - 6, ky - 3, 13, 7, COL_KNOB);
        draw_rect_outline(sf, cx - 6, ky - 3, 13, 7, COL_BORDER);
        draw_text(sf, cx - 6, SLIDER_Y + SLIDER_H - 12, slider_names[i], COL_TEXT);

        char vbuf[8];
        SDL_snprintf(vbuf, sizeof(vbuf), "%d", (int)(v * 100));
        draw_text(sf, cx - 4, SLIDER_Y + 3, vbuf, COL_TEXT);
    }

    fill_rect(sf, 0, KEYS_Y, SCREEN_W, KEYS_H, COL_BG);

    int base_note = s->octave * 12;
    int top_note = base_note + 28;
    if (top_note > 127) top_note = 127;

    static const int white_notes[] = {0, 2, 4, 5, 7, 9, 11};
    int total_white = 0;
    int white_list[40];
    for (int oct = 0; oct < 3; oct++) {
        for (int i = 0; i < 7; i++) {
            int note = base_note + oct * 12 + white_notes[i];
            if (note > top_note || note > 127) break;
            white_list[total_white++] = note;
        }
    }
    if (total_white == 0) total_white = 1;

    int kw = SCREEN_W / total_white;
    int koff = (SCREEN_W - kw * total_white) / 2;

    for (int i = 0; i < total_white; i++) {
        int kx = koff + i * kw;
        Uint32 c = s->key_held[white_list[i]] ? COL_PRESSED : COL_WHITE;
        fill_rect(sf, kx + 1, KEYS_Y + 2, kw - 2, KEYS_H - 4, c);
        draw_rect_outline(sf, kx + 1, KEYS_Y + 2, kw - 2, KEYS_H - 4, COL_BORDER);
    }

    static const int black_offsets[] = {1, 3, 6, 8, 10};
    for (int oct = 0; oct < 3; oct++) {
        for (int b = 0; b < 5; b++) {
            int note = base_note + oct * 12 + black_offsets[b];
            if (note > top_note || note > 127) continue;
            int wi = -1;
            for (int w = 0; w < total_white - 1; w++) {
                if (white_list[w] == note - 1) {
                    wi = w;
                    break;
                }
            }
            if (wi < 0) continue;
            int bx = koff + wi * kw + kw * 2 / 3;
            int bw = kw * 2 / 3;
            Uint32 c = s->key_held[note] ? COL_PRESSED : COL_BLACK_KEY;
            fill_rect(sf, bx, KEYS_Y + 2, bw, KEYS_H * 3 / 5, c);
            draw_rect_outline(sf, bx, KEYS_Y + 2, bw, KEYS_H * 3 / 5, COL_BORDER);
        }
    }

    static const char *note_labels[] = {"C", "D", "E", "F", "G", "A", "B"};
    for (int i = 0; i < total_white; i++) {
        int kx = koff + i * kw;
        int pc = white_list[i] % 12;
        int ni = 0;
        for (int j = 0; j < 7; j++) {
            if (white_notes[j] == pc) {
                ni = j;
                break;
            }
        }
        draw_text(sf, kx + kw / 2 - 2, KEYS_Y + KEYS_H - 17, note_labels[ni], COL_BORDER);
        if (pc == 0) {
            char obuf[4];
            SDL_snprintf(obuf, sizeof(obuf), "%d", white_list[i] / 12);
            draw_text(sf, kx + kw / 2 - 2, KEYS_Y + KEYS_H - 30, obuf, COL_TEXT);
        }
    }
}

int slider_hit(int mx, int my) {
    if (my < SLIDER_Y + 8 || my > SLIDER_Y + SLIDER_H - 8) return -1;
    int sw = SCREEN_W / 8;
    int idx = mx / sw;
    if (idx < 0 || idx >= 8) return -1;
    int cx = sw * idx + sw / 2;
    if (abs(mx - cx) > 10) return -1;
    return idx;
}

void slider_update(Synth *s, int idx, int my) {
    int ty = SLIDER_Y + 12;
    int by = SLIDER_Y + SLIDER_H - 16;
    float v = 1.0f - (float)(my - ty) / (by - ty);
    if (v < 0) v = 0;
    if (v > 1) v = 1;
    switch (idx) {
        case 0: s->volume = v; break;
        case 1: s->adsr.a = v * 2.0f; break;
        case 2: s->adsr.d = v * 2.0f; break;
        case 3: s->adsr.s = v; break;
        case 4: s->adsr.r = v * 2.0f; break;
        case 5: s->cutoff = v; break;
        case 6: s->resonance = v; break;
        case 7: s->claudiness = v; break;
    }
}

int wave_hit(Synth *s, int mx, int my) {
    for (int i = 0; i < WAVE_PTS; i++) {
        int px = (int)(s->waveform[i].x * (SCREEN_W - 1));
        int py = WAVE_ED_Y + 10 + (int)((1.0f - s->waveform[i].y) * (WAVE_ED_H - 20));
        if (abs(mx - px) < 6 && abs(my - py) < 6) return i;
    }
    return -1;
}

void wave_update(Synth *s, int idx, int mx, int my) {
    float ny = 1.0f - (float)(my - WAVE_ED_Y - 10) / (WAVE_ED_H - 20);
    if (ny < 0) ny = 0;
    if (ny > 1) ny = 1;
    s->waveform[idx].y = ny;

    float nx = (float)mx / (SCREEN_W - 1);
    if (nx < 0) nx = 0;
    if (nx > 1) nx = 1;
    if (idx > 0 && nx < s->waveform[idx - 1].x + 0.01f) nx = s->waveform[idx - 1].x + 0.01f;
    if (idx < WAVE_PTS - 1 && nx > s->waveform[idx + 1].x - 0.01f) nx = s->waveform[idx + 1].x - 0.01f;
    if (idx == 0) nx = 0;
    if (idx == WAVE_PTS - 1) nx = 1.0f;
    s->waveform[idx].x = nx;
}

int drum_grid_hit(int mx, int my, int *out_row, int *out_step) {
    DrumGridLayout grid = drum_grid_layout();
    int max_y = grid.top + grid.row_h * DRUM_TYPE_COUNT;
    int max_x = grid.grid_x + grid.cell_w * SEQ_STEPS;
    if (my < grid.top || my >= max_y) return 0;
    if (mx < grid.grid_x || mx >= max_x) return 0;
    *out_row = (my - grid.top) / grid.row_h;
    *out_step = (mx - grid.grid_x) / grid.cell_w;
    return (*out_row >= 0 && *out_row < DRUM_TYPE_COUNT &&
            *out_step >= 0 && *out_step < SEQ_STEPS);
}

int piano_mouse_note(Synth *s, int mx, int my) {
    if (my < KEYS_Y + 2 || my > KEYS_Y + KEYS_H - 2) return -1;

    int base_note = s->octave * 12;
    int top_note = base_note + 28;
    if (top_note > 127) top_note = 127;

    static const int white_notes[] = {0, 2, 4, 5, 7, 9, 11};
    int total_white = 0;
    int white_list[40];
    for (int oct = 0; oct < 3; oct++) {
        for (int i = 0; i < 7; i++) {
            int note = base_note + oct * 12 + white_notes[i];
            if (note > top_note || note > 127) break;
            white_list[total_white++] = note;
        }
    }
    if (total_white == 0) return -1;

    int kw = SCREEN_W / total_white;
    int koff = (SCREEN_W - kw * total_white) / 2;
    static const int black_offsets[] = {1, 3, 6, 8, 10};

    if (my < KEYS_Y + 2 + KEYS_H * 3 / 5) {
        for (int oct = 0; oct < 3; oct++) {
            for (int b = 0; b < 5; b++) {
                int note = base_note + oct * 12 + black_offsets[b];
                if (note > top_note || note > 127) continue;
                int wi = -1;
                for (int w = 0; w < total_white - 1; w++) {
                    if (white_list[w] == note - 1) {
                        wi = w;
                        break;
                    }
                }
                if (wi < 0) continue;
                int bx = koff + wi * kw + kw * 2 / 3;
                int bw = kw * 2 / 3;
                if (mx >= bx && mx < bx + bw) return note;
            }
        }
    }

    int wi = (mx - koff) / kw;
    if (wi >= 0 && wi < total_white) return white_list[wi];
    return -1;
}
