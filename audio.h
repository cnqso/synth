#ifndef AUDIO_H
#define AUDIO_H

#include "synth.h"

void audio_init_tables(void);
float note_freq(int note, Tuning t, float claudiness);
void note_on(Synth *s, int note);
void note_off(Synth *s, int note);
DrumType gm_to_drum(int note);
void drum_on(Synth *s, DrumType type);
int scancode_to_offset(SDL_Scancode sc);
void SDLCALL audio_cb(void *userdata, SDL_AudioStream *stream, int additional, int total);

#endif
