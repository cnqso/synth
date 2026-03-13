#include "synth.h"

#include <SDL3/SDL_main.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "draw.h"
#include "midi.h"
#include "ui.h"

#ifndef APP_NAME
#define APP_NAME "cnqsosynth"
#endif

#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif

static void SDLCALL midi_dialog_cb(void *userdata, const char * const *files, int filter) {
    (void)filter;
    Synth *s = (Synth *)userdata;
    if (files && files[0]) load_midi(s, files[0]);
}

static void init_synth_defaults(Synth *s) {
    memset(s, 0, sizeof(*s));
    s->osc = OSC_SINE;
    s->tuning = TUN_12TET;
    s->octave = 4;
    s->adsr = (ADSR){0.01f, 0.15f, 0.7f, 0.3f};
    s->volume = 0.35f;
    s->cutoff = 1.0f;
    s->resonance = 0.0f;
    s->claudiness = 0.0f;
    s->drum_vol = 0.7f;
    s->seq_tempo = 112.0f;
    s->seq_length = SEQ_STEPS;
    s->seq_step = -1;
    s->drag_slider = -1;
    s->drag_wave = -1;
    for (int i = 0; i < DRUM_TYPE_COUNT; i++) s->drum_row_type[i] = i;
    init_drum_pattern(s);
    for (int i = 0; i < WAVE_PTS; i++) {
        s->waveform[i].x = (float)i / (WAVE_PTS - 1);
        s->waveform[i].y = 0.5f + 0.5f * sinf((float)i / (WAVE_PTS - 1) * 2 * PI);
    }
}

static int init_video(Synth *s) {
    s->win = SDL_CreateWindow("cnqsosynth", SCREEN_W, SCREEN_H, SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!s->win) {
        SDL_Log("Window: %s", SDL_GetError());
        return 0;
    }

    s->ren = SDL_CreateRenderer(s->win, NULL);
    if (!s->ren) {
        SDL_Log("Renderer: %s", SDL_GetError());
        return 0;
    }

    s->surf = SDL_CreateSurface(SCREEN_W, SCREEN_H, SDL_PIXELFORMAT_RGBA8888);
    s->tex = SDL_CreateTexture(s->ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
    if (!s->tex) {
        SDL_Log("Texture: %s", SDL_GetError());
        return 0;
    }

    if (!SDL_SetTextureScaleMode(s->tex, SDL_SCALEMODE_NEAREST))
        SDL_Log("Texture scale mode: %s", SDL_GetError());
    if (!SDL_SetRenderLogicalPresentation(s->ren, SCREEN_W, SCREEN_H, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE))
        SDL_Log("Logical presentation: %s", SDL_GetError());

    return 1;
}

static int init_audio_system(Synth *s) {
    SDL_AudioSpec spec;
    spec.format = SDL_AUDIO_F32;
    spec.channels = 1;
    spec.freq = SAMPLE_RATE;

    s->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audio_cb, s);
    if (!s->stream) {
        SDL_Log("Audio: %s", SDL_GetError());
        return 0;
    }

    SDL_ResumeAudioStreamDevice(s->stream);
    return 1;
}

static void destroy_synth(Synth *s) {
    if (s->rec_buf) SDL_free(s->rec_buf);
    if (s->stream) SDL_DestroyAudioStream(s->stream);
    if (s->tex) SDL_DestroyTexture(s->tex);
    if (s->surf) SDL_DestroySurface(s->surf);
    if (s->ren) SDL_DestroyRenderer(s->ren);
    if (s->win) SDL_DestroyWindow(s->win);
}

static void toggle_seq_playback(Synth *s) {
    SDL_LockAudioStream(s->stream);
    s->seq_playing = !s->seq_playing;
    s->seq_step = -1;
    s->seq_phase = 0.0f;
    SDL_UnlockAudioStream(s->stream);
}

static void adjust_seq_tempo(Synth *s, float delta) {
    SDL_LockAudioStream(s->stream);
    s->seq_tempo += delta;
    if (s->seq_tempo < 40.0f) s->seq_tempo = 40.0f;
    if (s->seq_tempo > 240.0f) s->seq_tempo = 240.0f;
    SDL_UnlockAudioStream(s->stream);
}

static void retune_active_voices(Synth *s) {
    SDL_LockAudioStream(s->stream);
    for (int v = 0; v < MAX_VOICES; v++) {
        if (!s->voices[v].active) continue;
        s->voices[v].freq = note_freq(s->voices[v].note, s->tuning, s->claudiness);
    }
    SDL_UnlockAudioStream(s->stream);
}

static void toggle_recording(Synth *s) {
    SDL_LockAudioStream(s->stream);
    if (s->recording) {
        s->recording = 0;
    } else {
        s->rec_playing = 0;
        s->recording = 1;
        s->rec_len = 0;
        s->rec_cap = SAMPLE_RATE * 60;
        if (s->rec_buf) SDL_free(s->rec_buf);
        s->rec_buf = (float *)SDL_malloc(s->rec_cap * sizeof(float));
        s->roll_time = 0;
    }
    SDL_UnlockAudioStream(s->stream);
}

static void toggle_recorded_playback(Synth *s) {
    if (s->rec_playing) {
        s->rec_playing = 0;
    } else {
        s->rec_playing = 1;
        s->rec_playpos = 0;
    }
}

static void trigger_fkey_drum(Synth *s, SDL_Scancode scancode) {
    DrumType dt = -1;
    switch (scancode) {
        case SDL_SCANCODE_F1: dt = DRUM_KICK; break;
        case SDL_SCANCODE_F2: dt = DRUM_SNARE; break;
        case SDL_SCANCODE_F3: dt = DRUM_HIHAT; break;
        case SDL_SCANCODE_F4: dt = DRUM_CLAP; break;
        default: break;
    }
    if (dt < 0) return;
    SDL_LockAudioStream(s->stream);
    drum_on(s, dt);
    SDL_UnlockAudioStream(s->stream);
}

static void add_recorded_note_start(Synth *s, int note) {
    if (!s->recording || s->roll_count >= MAX_ROLL) return;
    s->roll[s->roll_count].note = note;
    s->roll[s->roll_count].start = s->roll_time;
    s->roll[s->roll_count].dur = 0;
    s->roll[s->roll_count].is_drum = 0;
    s->roll_count++;
}

static void close_recorded_note(Synth *s, int note) {
    if (!s->recording) return;
    for (int i = s->roll_count - 1; i >= 0; i--) {
        if (s->roll[i].note != note || s->roll[i].dur != 0) continue;
        s->roll[i].dur = s->roll_time - s->roll[i].start;
        if (s->roll[i].dur < 0.05f) s->roll[i].dur = 0.05f;
        break;
    }
}

static void play_note_from_input(Synth *s, int note) {
    SDL_LockAudioStream(s->stream);
    note_on(s, note);
    s->key_held[note] = 1;
    SDL_UnlockAudioStream(s->stream);
}

static void release_note_from_input(Synth *s, int note) {
    SDL_LockAudioStream(s->stream);
    note_off(s, note);
    s->key_held[note] = 0;
    SDL_UnlockAudioStream(s->stream);
}

static void update_midi_playback(Synth *s) {
    float prev = s->midi_time;
    s->midi_time += 1.0f / 60.0f;
    s->roll_scroll = s->midi_time - 2.0f;
    if (s->roll_scroll < 0) s->roll_scroll = 0;

    SDL_LockAudioStream(s->stream);
    while (s->midi_cursor < s->roll_count) {
        RollNote *rn = &s->roll[s->midi_cursor];
        if (rn->start >= s->midi_time) break;
        if (rn->start >= prev) {
            if (rn->is_drum) {
                drum_on(s, (DrumType)s->drum_row_type[(int)rn->drum]);
            } else {
                note_on(s, rn->note);
                s->key_held[rn->note] = 1;
            }
        }
        s->midi_cursor++;
    }

    int scan_start = s->midi_cursor > 256 ? s->midi_cursor - 256 : 0;
    for (int i = scan_start; i < s->midi_cursor; i++) {
        RollNote *rn = &s->roll[i];
        if (rn->is_drum) continue;
        float end = rn->start + rn->dur;
        if (end >= prev && end < s->midi_time) {
            note_off(s, rn->note);
            s->key_held[rn->note] = 0;
        }
    }
    SDL_UnlockAudioStream(s->stream);

    if (s->midi_time > s->midi_max_t + 1.0f) {
        s->midi_playing = 0;
        memset(s->key_held, 0, sizeof(s->key_held));
    }
}

int main(int argc, char *argv[]) {
    const char *midi_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--version")) {
            printf("%s %s\n", APP_NAME, APP_VERSION);
            return 0;
        }
        if (!strcmp(argv[i], "--help")) {
            printf("usage: %s [midi-file]\n", argv[0]);
            printf("       %s --version\n", argv[0]);
            return 0;
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            return 1;
        }
        midi_path = argv[i];
        break;
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    audio_init_tables();
    load_font();

    Synth synth;
    init_synth_defaults(&synth);

    if (!init_video(&synth) || !init_audio_system(&synth)) {
        destroy_synth(&synth);
        SDL_Quit();
        return 1;
    }

    if (midi_path) load_midi(&synth, midi_path);

    int running = 1;
    int mouse_piano_note = -1;

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_EVENT_QUIT:
                    running = 0;
                    break;

                case SDL_EVENT_KEY_DOWN:
                    if (ev.key.repeat) break;
                    if (ev.key.scancode == SDL_SCANCODE_SPACE) {
                        toggle_seq_playback(&synth);
                        break;
                    }
                    if (ev.key.scancode == SDL_SCANCODE_LEFTBRACKET) {
                        adjust_seq_tempo(&synth, -2.0f);
                        break;
                    }
                    if (ev.key.scancode == SDL_SCANCODE_RIGHTBRACKET) {
                        adjust_seq_tempo(&synth, 2.0f);
                        break;
                    }
                    if (ev.key.scancode == SDL_SCANCODE_LEFT) {
                        if (synth.octave > 0) synth.octave--;
                        break;
                    }
                    if (ev.key.scancode == SDL_SCANCODE_RIGHT) {
                        if (synth.octave < 8) synth.octave++;
                        break;
                    }

                    trigger_fkey_drum(&synth, ev.key.scancode);

                    {
                        int off = scancode_to_offset(ev.key.scancode);
                        if (off >= 0) {
                            int note = synth.octave * 12 + off;
                            if (note > 127) note = 127;
                            play_note_from_input(&synth, note);
                            add_recorded_note_start(&synth, note);
                        }
                    }
                    break;

                case SDL_EVENT_KEY_UP: {
                    int off = scancode_to_offset(ev.key.scancode);
                    if (off >= 0) {
                        int note = synth.octave * 12 + off;
                        if (note > 127) note = 127;
                        release_note_from_input(&synth, note);
                        close_recorded_note(&synth, note);
                    }
                    break;
                }

                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    synth.mouse_down = 1;
                    synth.mouse_x = (int)ev.button.x;
                    synth.mouse_y = (int)ev.button.y;

                    if (synth.mouse_y < BAR_H) {
                        int mx = synth.mouse_x;
                        for (int i = 0; i < OSC_COUNT; i++) {
                            int bx = 100 + i * 32;
                            if (mx >= bx && mx < bx + 28) synth.osc = i;
                        }
                        for (int i = 0; i < TUN_COUNT; i++) {
                            int bx = 260 + i * 26;
                            if (mx >= bx && mx < bx + 24) {
                                synth.tuning = i;
                                retune_active_voices(&synth);
                            }
                        }
                        if (mx >= 430 && mx < 458) toggle_recording(&synth);
                        if (!synth.recording && synth.rec_len > 0 && mx >= 460 && mx < 482)
                            toggle_recorded_playback(&synth);
                        if (mx >= 490 && mx < 518)
                            SDL_ShowOpenFileDialog(midi_dialog_cb, &synth, synth.win, NULL, 0, NULL, false);
                        if (synth.midi_loaded && mx >= 520 && mx < 542) {
                            synth.midi_playing = !synth.midi_playing;
                            if (synth.midi_playing) {
                                synth.midi_time = 0;
                                synth.midi_cursor = 0;
                                synth.roll_scroll = 0;
                            }
                        }
                        if (mx >= 550 && mx < 564 && synth.octave > 0) synth.octave--;
                        if (mx >= 584 && mx < 598 && synth.octave < 8) synth.octave++;
                    }

                    synth.drag_slider = slider_hit(synth.mouse_x, synth.mouse_y);
                    if (synth.drag_slider >= 0) slider_update(&synth, synth.drag_slider, synth.mouse_y);

                    if (synth.mouse_y >= WAVE_ED_Y && synth.mouse_y < WAVE_ED_Y + WAVE_ED_H)
                        synth.drag_wave = wave_hit(&synth, synth.mouse_x, synth.mouse_y);

                    if (synth.mouse_y >= ROLL_Y && synth.mouse_y < ROLL_Y + ROLL_H) {
                        int mx = synth.mouse_x;
                        int my = synth.mouse_y;

                        if (my < ROLL_Y + 12) {
                            if (mx >= 0 && mx < 12) {
                                synth.roll_view = (synth.roll_view + 1) % 3;
                            } else {
                                int arrow_x = roll_view_right_arrow_x(synth.roll_view);
                                if (mx >= arrow_x && mx < arrow_x + 10)
                                    synth.roll_view = (synth.roll_view + 1) % 3;
                            }
                        }

                        if (synth.roll_view == 1 && my < ROLL_Y + 12) {
                            if (mx >= 74 && mx < 98) toggle_seq_playback(&synth);
                            else if (mx >= 102 && mx < 126) {
                                SDL_LockAudioStream(synth.stream);
                                clear_drum_pattern(&synth);
                                SDL_UnlockAudioStream(synth.stream);
                            } else if (mx >= 132 && mx < 144) {
                                adjust_seq_tempo(&synth, -2.0f);
                            } else if (mx >= 190 && mx < 202) {
                                adjust_seq_tempo(&synth, 2.0f);
                            }
                        }

                        if ((synth.roll_view == 1 || synth.roll_view == 2) &&
                            mx < drum_grid_layout().label_w &&
                            my >= drum_grid_layout().top) {
                            DrumGridLayout grid = drum_grid_layout();
                            int d = (my - grid.top) / grid.row_h;
                            if (d >= 0 && d < DRUM_TYPE_COUNT) {
                                SDL_LockAudioStream(synth.stream);
                                synth.drum_row_type[d] = (synth.drum_row_type[d] + 1) % DRUM_TYPE_COUNT;
                                drum_on(&synth, (DrumType)synth.drum_row_type[d]);
                                SDL_UnlockAudioStream(synth.stream);
                            }
                        }

                        if (synth.roll_view == 1) {
                            int row;
                            int step;
                            if (drum_grid_hit(mx, my, &row, &step)) {
                                SDL_LockAudioStream(synth.stream);
                                synth.drum_pattern[row][step] = !synth.drum_pattern[row][step];
                                if (synth.drum_pattern[row][step])
                                    drum_on(&synth, (DrumType)synth.drum_row_type[row]);
                                SDL_UnlockAudioStream(synth.stream);
                            }
                        }
                    }

                    if (synth.mouse_y >= KEYS_Y) {
                        int pn = piano_mouse_note(&synth, synth.mouse_x, synth.mouse_y);
                        if (pn >= 0) {
                            mouse_piano_note = pn;
                            play_note_from_input(&synth, pn);
                        }
                    }
                    break;

                case SDL_EVENT_MOUSE_MOTION:
                    synth.mouse_x = (int)ev.motion.x;
                    synth.mouse_y = (int)ev.motion.y;
                    if (synth.drag_slider >= 0) slider_update(&synth, synth.drag_slider, synth.mouse_y);
                    if (synth.drag_wave >= 0) wave_update(&synth, synth.drag_wave, synth.mouse_x, synth.mouse_y);
                    break;

                case SDL_EVENT_MOUSE_BUTTON_UP:
                    synth.mouse_down = 0;
                    synth.drag_slider = -1;
                    synth.drag_wave = -1;
                    if (mouse_piano_note >= 0) {
                        release_note_from_input(&synth, mouse_piano_note);
                        mouse_piano_note = -1;
                    }
                    break;
            }
        }

        if (synth.midi_playing) update_midi_playback(&synth);

        if (synth.recording) {
            synth.roll_time += 1.0f / 60.0f;
            synth.roll_scroll = synth.roll_time - 4.0f;
            if (synth.roll_scroll < 0) synth.roll_scroll = 0;
        }

        draw_ui(&synth);
        SDL_UpdateTexture(synth.tex, NULL, synth.surf->pixels, synth.surf->pitch);
        SDL_SetRenderDrawColor(synth.ren, 0xFA, 0xFA, 0xFA, 0xFF);
        SDL_RenderClear(synth.ren);
        SDL_RenderTexture(synth.ren, synth.tex, NULL, NULL);
        SDL_RenderPresent(synth.ren);
        SDL_Delay(16);
    }

    destroy_synth(&synth);
    SDL_Quit();
    return 0;
}
