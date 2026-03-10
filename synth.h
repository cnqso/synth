#ifndef SYNTH_H
#define SYNTH_H

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdint.h>

#define SCREEN_W 640
#define SCREEN_H 480
#define SAMPLE_RATE 44100
#define MAX_VOICES 24
#define WAVE_PTS 32
#define MAX_ROLL 16384
#define SEQ_STEPS 16
#define PI 3.14159265358979323846f
#define AUDIO_BUF 1024
#define MAX_DRUMS 8

#define COL_BG 0xFAFAFAFF
#define COL_PANEL 0xF0F0F0FF
#define COL_BORDER 0xD0D0D0FF
#define COL_TEXT 0x808080FF
#define COL_DARK 0x606060FF
#define COL_PRESSED 0xE0E0E8FF
#define COL_ACCENT 0xD0A0A0FF
#define COL_WHITE 0xFFFFFFFF
#define COL_KNOB 0xC8C8C8FF
#define COL_BLACK_KEY 0xB0B0B0FF
#define COL_WAVE 0xA0A0A8FF

#define BAR_Y 0
#define BAR_H 18
#define WAVE_ED_Y 18
#define WAVE_ED_H 80
#define SCOPE_Y 98
#define SCOPE_H 72
#define ROLL_Y 170
#define ROLL_H 120
#define SLIDER_Y 290
#define SLIDER_H 80
#define KEYS_Y 370
#define KEYS_H 110

typedef enum { OSC_SINE, OSC_SAW, OSC_SQUARE, OSC_TRI, OSC_CUSTOM, OSC_COUNT } OscType;
typedef enum { TUN_12TET, TUN_JUST, TUN_PYTH, TUN_7TET, TUN_SLEND, TUN_CNQ, TUN_COUNT } Tuning;
typedef enum { ENV_IDLE, ENV_ATK, ENV_DEC, ENV_SUS, ENV_REL } EnvStage;

typedef struct { float a, d, s, r; } ADSR;

typedef struct {
    int active;
    int note;
    float phase;
    float freq;
    EnvStage env_stage;
    float env_level;
    float env_time;
    float filt_state;
} Voice;

typedef enum {
    DRUM_KICK, DRUM_SNARE, DRUM_HIHAT, DRUM_CLAP,
    DRUM_RIDE, DRUM_TOM, DRUM_CONGA, DRUM_SHAKER,
    DRUM_COWBELL, DRUM_STICK, DRUM_CRASH,
    DRUM_TYPE_COUNT
} DrumType;

typedef struct {
    int active;
    DrumType type;
    float time;
    float phase;
    float filt;
    float filt2;
} DrumVoice;

typedef struct {
    float x;
    float y;
} WavePoint;

typedef struct {
    int top;
    int bot;
    int row_h;
    int label_w;
    int grid_x;
    int cell_w;
} DrumGridLayout;

typedef struct {
    int note;
    float start;
    float dur;
    int is_drum;
    DrumType drum;
} RollNote;

typedef struct {
    SDL_AudioStream *stream;
    Voice voices[MAX_VOICES];
    DrumVoice drums[MAX_DRUMS];
    float drum_vol;
    OscType osc;
    Tuning tuning;
    ADSR adsr;
    float volume;
    float cutoff;
    float resonance;
    float claudiness;

    WavePoint waveform[WAVE_PTS];
    float scope_buf[SCREEN_W];
    int scope_pos;

    RollNote roll[MAX_ROLL];
    int roll_count;
    float roll_scroll;
    float roll_time;

    int recording;
    float *rec_buf;
    int rec_len;
    int rec_cap;

    int rec_playing;
    int rec_playpos;

    int midi_loaded;
    int midi_playing;
    float midi_time;
    float midi_max_t;
    int midi_cursor;

    int octave;
    int roll_view;
    int drum_row_type[DRUM_TYPE_COUNT];
    int drum_pattern[DRUM_TYPE_COUNT][SEQ_STEPS];
    int seq_playing;
    int seq_step;
    float seq_phase;
    float seq_tempo;
    int seq_length;

    int drag_slider;
    int drag_wave;
    int mouse_x;
    int mouse_y;
    int mouse_down;
    int key_held[128];

    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Surface *surf;
    SDL_Texture *tex;

    float global_filt_state;
} Synth;

#endif
