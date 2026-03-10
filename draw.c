#include "draw.h"

#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define FONT_FIRST 32
#define FONT_LAST 127
#define FONT_NGLYPHS (FONT_LAST - FONT_FIRST)
#define GLYPH3(r0, r1, r2, r3, r4) \
    (uint16_t)(((r0) << 12) | ((r1) << 9) | ((r2) << 6) | ((r3) << 3) | (r4))
#define TTF_EM_PX 16.0f
#define TTF_FONT_NAME "Nintendo-DS-BIOS.ttf"

typedef struct {
    unsigned char *bitmap;
    int w;
    int h;
    int advance;
    int xoff;
    int yoff;
} FontGlyph;

static FontGlyph font_glyphs[FONT_NGLYPHS];
static int font_ascent_ttf;
static int font_loaded;

static const uint32_t font4x6[128] = {
    ['A'] = 0x699F99, ['B'] = 0xE9E99E, ['C'] = 0x788896, ['D'] = 0xE9999E,
    ['E'] = 0xF8E88F, ['F'] = 0xF8E888, ['G'] = 0x78B996, ['H'] = 0x99F999,
    ['I'] = 0x622226, ['J'] = 0x311196, ['K'] = 0x9ACA99, ['L'] = 0x88888F,
    ['M'] = 0x9FF999, ['N'] = 0x9DFB99, ['O'] = 0x699996, ['P'] = 0xE99E88,
    ['Q'] = 0x699963, ['R'] = 0xE99EA9, ['S'] = 0x78611E, ['T'] = 0xF44444,
    ['U'] = 0x999996, ['V'] = 0x999A44, ['W'] = 0x999FF9, ['X'] = 0x996699,
    ['Y'] = 0x996244, ['Z'] = 0xF1248F,
    ['0'] = 0x69BD96, ['1'] = 0x262227, ['2'] = 0x69168F, ['3'] = 0xF16196,
    ['4'] = 0x99F111, ['5'] = 0xF8E196, ['6'] = 0x68E996, ['7'] = 0xF12444,
    ['8'] = 0x696996, ['9'] = 0x697116,
    [' '] = 0x000000, ['.'] = 0x000004, ['-'] = 0x00F000,
    ['+'] = 0x04E400, ['/'] = 0x112488, [':'] = 0x040040, ['#'] = 0x5F55F5,
};

static const uint16_t font3x5[128] = {
    ['A'] = GLYPH3(2, 5, 7, 5, 5), ['B'] = GLYPH3(6, 5, 6, 5, 6),
    ['C'] = GLYPH3(6, 4, 4, 4, 6), ['D'] = GLYPH3(6, 5, 5, 5, 6),
    ['E'] = GLYPH3(7, 4, 6, 4, 7), ['F'] = GLYPH3(7, 4, 6, 4, 4),
    ['G'] = GLYPH3(3, 4, 5, 5, 3), ['H'] = GLYPH3(5, 5, 7, 5, 5),
    ['I'] = GLYPH3(7, 2, 2, 2, 7), ['J'] = GLYPH3(1, 1, 1, 5, 2),
    ['K'] = GLYPH3(5, 5, 6, 5, 5), ['L'] = GLYPH3(4, 4, 4, 4, 7),
    ['M'] = GLYPH3(5, 7, 7, 5, 5), ['N'] = GLYPH3(5, 7, 7, 7, 5),
    ['O'] = GLYPH3(2, 5, 5, 5, 2), ['P'] = GLYPH3(6, 5, 6, 4, 4),
    ['Q'] = GLYPH3(2, 5, 5, 2, 1), ['R'] = GLYPH3(6, 5, 6, 5, 5),
    ['S'] = GLYPH3(7, 4, 7, 1, 7), ['T'] = GLYPH3(7, 2, 2, 2, 2),
    ['U'] = GLYPH3(5, 5, 5, 5, 7), ['V'] = GLYPH3(5, 5, 5, 5, 2),
    ['W'] = GLYPH3(5, 5, 7, 7, 5), ['X'] = GLYPH3(5, 5, 2, 5, 5),
    ['Y'] = GLYPH3(5, 5, 2, 2, 2), ['Z'] = GLYPH3(7, 1, 2, 4, 7),
    ['0'] = GLYPH3(7, 5, 5, 5, 7), ['1'] = GLYPH3(2, 6, 2, 2, 7),
    ['2'] = GLYPH3(6, 1, 7, 4, 7), ['3'] = GLYPH3(6, 1, 3, 1, 6),
    ['4'] = GLYPH3(5, 5, 7, 1, 1), ['5'] = GLYPH3(7, 4, 6, 1, 6),
    ['6'] = GLYPH3(3, 4, 7, 5, 7), ['7'] = GLYPH3(7, 1, 2, 2, 2),
    ['8'] = GLYPH3(7, 5, 7, 5, 7), ['9'] = GLYPH3(7, 5, 7, 1, 6),
    [' '] = GLYPH3(0, 0, 0, 0, 0), ['.'] = GLYPH3(0, 0, 0, 0, 2),
    ['-'] = GLYPH3(0, 0, 7, 0, 0), ['+'] = GLYPH3(0, 2, 7, 2, 0),
    ['/'] = GLYPH3(1, 1, 2, 4, 4), [':'] = GLYPH3(0, 2, 0, 2, 0),
    ['<'] = GLYPH3(1, 2, 4, 2, 1), ['>'] = GLYPH3(4, 2, 1, 2, 4),
    ['#'] = GLYPH3(5, 7, 5, 7, 5),
};

static void put_pixel(SDL_Surface *s, int x, int y, Uint32 col) {
    if (x < 0 || x >= s->w || y < 0 || y >= s->h) return;
    ((Uint32 *)((Uint8 *)s->pixels + y * s->pitch))[x] = col;
}

static void draw_char_fallback(SDL_Surface *s, int x, int y, char c, Uint32 col) {
    if (c >= 'a' && c <= 'z') c -= 32;
    uint32_t g = font4x6[(unsigned char)c];
    for (int row = 0; row < 6; row++)
        for (int cx = 0; cx < 4; cx++)
            if (g & (1 << (23 - row * 4 - cx)))
                put_pixel(s, x + cx, y + row, col);
}

static void draw_char_sm(SDL_Surface *s, int x, int y, char c, Uint32 col) {
    if (c >= 'a' && c <= 'z') c -= 32;
    uint16_t g = font3x5[(unsigned char)c];
    for (int row = 0; row < 5; row++)
        for (int cx = 0; cx < 3; cx++)
            if (g & (1 << (14 - row * 3 - cx)))
                put_pixel(s, x + cx, y + row, col);
}

static void draw_char_ttf(SDL_Surface *s, int x, int y, char c, Uint32 col) {
    unsigned char uc = (unsigned char)c;
    if (uc < FONT_FIRST || uc >= FONT_LAST) return;
    FontGlyph *g = &font_glyphs[uc - FONT_FIRST];
    if (!g->bitmap) return;

    int gx = x + g->xoff;
    int gy = y + font_ascent_ttf + g->yoff;

    for (int row = 0; row < g->h; row++) {
        int py = gy + row;
        if (py < 0 || py >= s->h) continue;
        for (int cx = 0; cx < g->w; cx++) {
            if (g->bitmap[row * g->w + cx])
                put_pixel(s, gx + cx, py, col);
        }
    }
}

static int char_advance_ttf(char c) {
    unsigned char uc = (unsigned char)c;
    if (uc < FONT_FIRST || uc >= FONT_LAST) return 5;
    int adv = font_glyphs[uc - FONT_FIRST].advance;
    return adv > 0 ? adv : 5;
}

void load_font(void) {
    char path[512] = {0};
    const char *base = SDL_GetBasePath();
    if (base) SDL_snprintf(path, sizeof(path), "%s%s", base, TTF_FONT_NAME);

    size_t fsize;
    void *fdata = SDL_LoadFile(path, &fsize);
    if (!fdata) fdata = SDL_LoadFile(TTF_FONT_NAME, &fsize);
    if (!fdata) {
        SDL_Log("Font '%s' not found; using bitmap fallback", TTF_FONT_NAME);
        return;
    }

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, (const unsigned char *)fdata, 0)) {
        SDL_Log("Font parse failed; using bitmap fallback");
        SDL_free(fdata);
        return;
    }

    float scale = stbtt_ScaleForMappingEmToPixels(&info, TTF_EM_PX);
    int asc;
    int desc;
    int lgap;
    stbtt_GetFontVMetrics(&info, &asc, &desc, &lgap);
    font_ascent_ttf = (int)(asc * scale);

    for (int i = 0; i < FONT_NGLYPHS; i++) {
        int ch = FONT_FIRST + i;
        int adv;
        int lsb;
        stbtt_GetCodepointHMetrics(&info, ch, &adv, &lsb);
        font_glyphs[i].advance = (int)(adv * scale + 0.5f);

        int x0;
        int y0;
        int x1;
        int y1;
        stbtt_GetCodepointBitmapBox(&info, ch, scale, scale, &x0, &y0, &x1, &y1);
        int gw = x1 - x0;
        int gh = y1 - y0;
        if (gw <= 0 || gh <= 0) continue;

        unsigned char *tmp = (unsigned char *)SDL_malloc(gw * gh);
        stbtt_MakeCodepointBitmap(&info, tmp, gw, gh, gw, scale, scale, ch);
        for (int p = 0; p < gw * gh; p++) tmp[p] = tmp[p] >= 80 ? 255 : 0;
        font_glyphs[i].bitmap = tmp;
        font_glyphs[i].w = gw;
        font_glyphs[i].h = gh;
        font_glyphs[i].xoff = x0;
        font_glyphs[i].yoff = y0;
    }

    SDL_free(fdata);
    font_loaded = 1;
    SDL_Log("TTF font loaded: %.0fpx em, ascent=%d", TTF_EM_PX, font_ascent_ttf);
}

void draw_text(SDL_Surface *s, int x, int y, const char *str, Uint32 col) {
    if (font_loaded) {
        while (*str) {
            draw_char_ttf(s, x, y, *str, col);
            x += char_advance_ttf(*str);
            str++;
        }
        return;
    }

    while (*str) {
        draw_char_fallback(s, x, y, *str, col);
        x += 5;
        str++;
    }
}

void draw_text_sm(SDL_Surface *s, int x, int y, const char *str, Uint32 col) {
    while (*str) {
        draw_char_sm(s, x, y, *str, col);
        x += 4;
        str++;
    }
}

int text_width_sm(const char *str) {
    int w = 0;
    while (*str) {
        w += 4;
        str++;
    }
    return w > 0 ? w - 1 : 0;
}

void fill_rect(SDL_Surface *s, int x, int y, int w, int h, Uint32 col) {
    SDL_Rect r = {x, y, w, h};
    SDL_FillSurfaceRect(s, &r, col);
}

void draw_hline(SDL_Surface *s, int x1, int x2, int y, Uint32 col) {
    if (x1 > x2) {
        int t = x1;
        x1 = x2;
        x2 = t;
    }
    fill_rect(s, x1, y, x2 - x1 + 1, 1, col);
}

void draw_vline(SDL_Surface *s, int x, int y1, int y2, Uint32 col) {
    if (y1 > y2) {
        int t = y1;
        y1 = y2;
        y2 = t;
    }
    fill_rect(s, x, y1, 1, y2 - y1 + 1, col);
}

void draw_line(SDL_Surface *s, int x0, int y0, int x1, int y1, Uint32 col) {
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int e = dx + dy;

    for (;;) {
        put_pixel(s, x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * e;
        if (e2 >= dy) {
            e += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            e += dx;
            y0 += sy;
        }
    }
}

void draw_rect_outline(SDL_Surface *s, int x, int y, int w, int h, Uint32 col) {
    draw_hline(s, x, x + w - 1, y, col);
    draw_hline(s, x, x + w - 1, y + h - 1, col);
    draw_vline(s, x, y, y + h - 1, col);
    draw_vline(s, x + w - 1, y, y + h - 1, col);
}
