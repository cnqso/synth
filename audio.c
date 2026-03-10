#include "audio.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const float just_ratios[12] = {
    1.0f, 16.0f / 15, 9.0f / 8, 6.0f / 5, 5.0f / 4, 4.0f / 3,
    45.0f / 32, 3.0f / 2, 8.0f / 5, 5.0f / 3, 9.0f / 5, 15.0f / 8
};

static const float pyth_ratios[12] = {
    1.0f, 256.0f / 243, 9.0f / 8, 32.0f / 27, 81.0f / 64, 4.0f / 3,
    729.0f / 512, 3.0f / 2, 128.0f / 81, 27.0f / 16, 16.0f / 9, 243.0f / 128
};

static float slendro_ratios[12];

static const float cnqso_detune[12] = {
     0.0f, -27.0f, +19.0f, -36.0f, +30.0f, -13.0f,
    +40.0f, -23.0f, +31.0f, -45.0f, +15.0f, -28.0f,
};

static uint32_t noise_state = 0x12345678;
static float audio_buf_static[AUDIO_BUF * 4];

static float osc_sample(float phase, OscType type, WavePoint *wv) {
    switch (type) {
        case OSC_SINE: return sinf(phase * 2 * PI);
        case OSC_SAW: return 2.0f * phase - 1.0f;
        case OSC_SQUARE: return phase < 0.5f ? 1.0f : -1.0f;
        case OSC_TRI: return 4.0f * fabsf(phase - 0.5f) - 1.0f;
        case OSC_CUSTOM: {
            float pos = phase * (WAVE_PTS - 1);
            int idx = (int)pos;
            if (idx >= WAVE_PTS - 1) return wv[WAVE_PTS - 1].y * 2.0f - 1.0f;
            float frac = pos - idx;
            float v = wv[idx].y * (1 - frac) + wv[idx + 1].y * frac;
            return v * 2.0f - 1.0f;
        }
        default: return 0;
    }
}

static float env_tick(Voice *v, ADSR *e, float dt) {
    switch (v->env_stage) {
        case ENV_IDLE:
            return 0;
        case ENV_ATK:
            v->env_time += dt;
            v->env_level = (e->a > 0.001f) ? v->env_time / e->a : 1.0f;
            if (v->env_level >= 1.0f) {
                v->env_level = 1.0f;
                v->env_stage = ENV_DEC;
                v->env_time = 0;
            }
            return v->env_level;
        case ENV_DEC:
            v->env_time += dt;
            v->env_level = 1.0f - (1.0f - e->s) * (e->d > 0.001f ? v->env_time / e->d : 1.0f);
            if (v->env_level <= e->s) {
                v->env_level = e->s;
                v->env_stage = ENV_SUS;
            }
            return v->env_level;
        case ENV_SUS:
            return e->s;
        case ENV_REL:
            v->env_time += dt;
            v->env_level = e->s * (1.0f - (e->r > 0.001f ? v->env_time / e->r : 1.0f));
            if (v->env_level <= 0.0f) {
                v->env_level = 0;
                v->env_stage = ENV_IDLE;
                v->active = 0;
            }
            return v->env_level > 0 ? v->env_level : 0;
        default:
            return 0;
    }
}

static float noise(void) {
    noise_state ^= noise_state << 13;
    noise_state ^= noise_state >> 17;
    noise_state ^= noise_state << 5;
    return (float)(noise_state & 0xFFFF) / 32768.0f - 1.0f;
}

static inline float fast_expf(float x) {
    if (x < -6.0f) return 0.0f;
    float a = 1.0f - x * (1.0f + x * (0.5f + x * 0.166667f));
    return 1.0f / a;
}

static float drum_sample(DrumVoice *d, float dt) {
    if (!d->active) return 0;

    float t = d->time;
    float out = 0;

    switch (d->type) {
        case DRUM_KICK: {
            float freq = 46.0f + 92.0f * fast_expf(-t * 24.0f);
            d->phase += freq * dt;
            if (d->phase >= 1.0f) d->phase -= 1.0f;
            float body = sinf(d->phase * 2 * PI);
            float sub = sinf(d->phase * 2 * PI * 0.5f);
            float click = noise() * fast_expf(-t * 170.0f) * 0.09f;
            d->filt += (click - d->filt) * 0.35f;
            click -= d->filt;
            out = (body * 0.82f + sub * 0.12f) * fast_expf(-t * 7.5f) + click;
            if (t > 0.65f) d->active = 0;
            break;
        }
        case DRUM_SNARE: {
            float freq = 184.0f + 28.0f * fast_expf(-t * 42.0f);
            d->phase += freq * dt;
            if (d->phase >= 1.0f) d->phase -= 1.0f;
            float body = sinf(d->phase * 2 * PI) * fast_expf(-t * 18.0f) * 0.42f;
            float nz = noise();
            d->filt += (nz - d->filt) * 0.07f;
            float hp = nz - d->filt;
            d->filt2 += (hp - d->filt2) * 0.22f;
            float wires = d->filt2 * fast_expf(-t * 16.0f) * 0.45f;
            out = body + wires;
            if (t > 0.32f) d->active = 0;
            break;
        }
        case DRUM_HIHAT: {
            float nz = noise();
            d->filt += (nz - d->filt) * 0.03f;
            float hp = nz - d->filt;
            float metal = sinf(t * 2 * PI * 5600.0f) * 0.10f
                        + sinf(t * 2 * PI * 8100.0f) * 0.07f
                        + sinf(t * 2 * PI * 10300.0f) * 0.04f;
            out = (hp * 0.20f + metal) * fast_expf(-t * 42.0f);
            if (t > 0.16f) d->active = 0;
            break;
        }
        case DRUM_CLAP: {
            float env = 0;
            if (t < 0.008f) env = 1.0f;
            else if (t < 0.016f) env = 0.0f;
            else if (t < 0.024f) env = 0.85f;
            else if (t < 0.032f) env = 0.0f;
            else env = 0.52f * fast_expf(-(t - 0.032f) * 14.0f);
            float nz = noise();
            d->filt += (nz - d->filt) * 0.14f;
            out = (nz - d->filt) * env * 0.42f;
            if (t > 0.28f) d->active = 0;
            break;
        }
        case DRUM_RIDE: {
            float bell = sinf(t * 2 * PI * 2860.0f) * 0.08f
                       + sinf(t * 2 * PI * 4320.0f) * 0.05f
                       + sinf(t * 2 * PI * 6480.0f) * 0.03f;
            float nz = noise();
            d->filt += (nz - d->filt) * 0.05f;
            out = (bell + (nz - d->filt) * 0.05f) * fast_expf(-t * 5.2f);
            if (t > 0.75f) d->active = 0;
            break;
        }
        case DRUM_TOM: {
            float freq = 86.0f + 46.0f * fast_expf(-t * 16.0f);
            d->phase += freq * dt;
            if (d->phase >= 1.0f) d->phase -= 1.0f;
            out = sinf(d->phase * 2 * PI) * fast_expf(-t * 8.5f) * 0.62f;
            if (t > 0.45f) d->active = 0;
            break;
        }
        case DRUM_CONGA: {
            float freq = 228.0f + 70.0f * fast_expf(-t * 30.0f);
            d->phase += freq * dt;
            if (d->phase >= 1.0f) d->phase -= 1.0f;
            float body = sinf(d->phase * 2 * PI) * fast_expf(-t * 10.0f);
            float slap = noise() * fast_expf(-t * 75.0f) * 0.12f;
            out = (body * 0.54f + slap) * 0.70f;
            if (t > 0.34f) d->active = 0;
            break;
        }
        case DRUM_SHAKER: {
            float nz = noise();
            d->filt += (nz - d->filt) * 0.08f;
            float hp = nz - d->filt;
            out = hp * fast_expf(-t * 28.0f) * 0.22f;
            if (t > 0.12f) d->active = 0;
            break;
        }
        case DRUM_COWBELL: {
            float s1 = sinf(t * 2 * PI * 420.0f) > 0 ? 1.0f : -1.0f;
            float s2 = sinf(t * 2 * PI * 632.0f) > 0 ? 1.0f : -1.0f;
            out = (s1 * 0.40f + s2 * 0.32f) * fast_expf(-t * 10.0f) * 0.32f;
            if (t > 0.3f) d->active = 0;
            break;
        }
        case DRUM_STICK: {
            float freq = 1520.0f + 360.0f * fast_expf(-t * 80.0f);
            d->phase += freq * dt;
            if (d->phase >= 1.0f) d->phase -= 1.0f;
            out = sinf(d->phase * 2 * PI) * fast_expf(-t * 52.0f) * 0.38f;
            if (t > 0.10f) d->active = 0;
            break;
        }
        case DRUM_CRASH: {
            float nz = noise();
            d->filt += (nz - d->filt) * 0.045f;
            float metal = sinf(t * 2 * PI * 3980.0f) * 0.06f
                        + sinf(t * 2 * PI * 6410.0f) * 0.04f;
            out = ((nz - d->filt) * 0.10f + metal) * fast_expf(-t * 3.2f) * 0.34f;
            if (t > 1.15f) d->active = 0;
            break;
        }
        default:
            break;
    }

    d->time += dt;
    return out;
}

static inline float fast_tanh(float x) {
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

static void trigger_seq_step(Synth *s, int step) {
    if (step < 0 || step >= s->seq_length) return;
    for (int row = 0; row < DRUM_TYPE_COUNT; row++) {
        if (!s->drum_pattern[row][step]) continue;
        drum_on(s, (DrumType)s->drum_row_type[row]);
    }
}

static void write_wav(const char *path, float *data, int n) {
    FILE *f = fopen(path, "wb");
    if (!f) return;

    int sr = SAMPLE_RATE;
    int ch = 1;
    int bps = 16;
    int data_size = n * ch * bps / 8;
    int fmt_size = 16;
    int file_size = 36 + data_size;

    fwrite("RIFF", 1, 4, f);
    fwrite(&file_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    fwrite(&fmt_size, 4, 1, f);

    short audio_fmt = 1;
    short nch = ch;
    fwrite(&audio_fmt, 2, 1, f);
    fwrite(&nch, 2, 1, f);
    fwrite(&sr, 4, 1, f);

    int byte_rate = sr * ch * bps / 8;
    fwrite(&byte_rate, 4, 1, f);

    short block_align = ch * bps / 8;
    short bit_depth = bps;
    fwrite(&block_align, 2, 1, f);
    fwrite(&bit_depth, 2, 1, f);

    fwrite("data", 1, 4, f);
    fwrite(&data_size, 4, 1, f);

    for (int i = 0; i < n; i++) {
        float s = data[i];
        if (s > 1.0f) s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        short v = (short)(s * 32767);
        fwrite(&v, 2, 1, f);
    }

    fclose(f);
}

void audio_init_tables(void) {
    static const int step_map[12] = {0, 0, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4};
    for (int i = 0; i < 12; i++)
        slendro_ratios[i] = powf(2.0f, (float)step_map[i] / 5.0f);
}

float note_freq(int note, Tuning t, float claudiness) {
    float freq;

    switch (t) {
        case TUN_JUST: {
            int oct = note / 12 - 5;
            int pc = note % 12;
            freq = 261.6256f * just_ratios[pc] * powf(2.0f, (float)oct);
            break;
        }
        case TUN_PYTH: {
            int oct = note / 12 - 5;
            int pc = note % 12;
            freq = 261.6256f * pyth_ratios[pc] * powf(2.0f, (float)oct);
            break;
        }
        case TUN_7TET:
            freq = 440.0f * powf(2.0f, (note - 69) / 7.0f);
            break;
        case TUN_SLEND: {
            int oct = note / 12 - 5;
            int pc = note % 12;
            freq = 261.6256f * slendro_ratios[pc] * powf(2.0f, (float)oct);
            break;
        }
        case TUN_CNQ:
        case TUN_12TET:
        default:
            freq = 440.0f * powf(2.0f, (note - 69) / 12.0f);
            break;
    }

    float cf = (t == TUN_CNQ) ? 1.0f : claudiness;
    if (cf > 0.001f) {
        int pc = note % 12;
        float cents = cnqso_detune[pc] + (note - 60) * 0.9f;
        freq *= powf(2.0f, (cents * cf) / 1200.0f);
    }

    return freq;
}

void note_on(Synth *s, int note) {
    if (note < 0 || note > 127) return;

    for (int i = 0; i < MAX_VOICES; i++) {
        if (s->voices[i].active && s->voices[i].note == note) {
            s->voices[i].env_stage = ENV_ATK;
            s->voices[i].env_time = 0;
            s->voices[i].env_level = 0;
            return;
        }
    }

    int vi = -1;
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!s->voices[i].active) {
            vi = i;
            break;
        }
    }

    if (vi < 0) {
        float mn = 2.0f;
        vi = 0;
        for (int i = 0; i < MAX_VOICES; i++) {
            if (s->voices[i].env_level < mn) {
                mn = s->voices[i].env_level;
                vi = i;
            }
        }
    }

    Voice *v = &s->voices[vi];
    v->active = 1;
    v->note = note;
    v->freq = note_freq(note, s->tuning, s->claudiness);
    v->phase = 0;
    v->env_stage = ENV_ATK;
    v->env_time = 0;
    v->env_level = 0;
    v->filt_state = 0;
}

void note_off(Synth *s, int note) {
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!s->voices[i].active || s->voices[i].note != note || s->voices[i].env_stage == ENV_REL) continue;
        s->voices[i].env_stage = ENV_REL;
        s->voices[i].env_time = 0;
    }
}

DrumType gm_to_drum(int note) {
    switch (note) {
        case 35:
        case 36:
            return DRUM_KICK;
        case 38:
        case 40:
            return DRUM_SNARE;
        case 37:
            return DRUM_STICK;
        case 39:
            return DRUM_CLAP;
        case 42:
        case 44:
        case 46:
            return DRUM_HIHAT;
        case 41:
        case 43:
        case 45:
        case 47:
        case 48:
        case 50:
            return DRUM_TOM;
        case 49:
        case 52:
        case 55:
        case 57:
            return DRUM_CRASH;
        case 51:
        case 53:
        case 59:
            return DRUM_RIDE;
        case 56:
            return DRUM_COWBELL;
        case 60:
        case 61:
        case 62:
        case 63:
        case 64:
            return DRUM_CONGA;
        case 65:
        case 66:
            return DRUM_TOM;
        case 69:
        case 70:
        case 54:
            return DRUM_SHAKER;
        case 75:
        case 76:
        case 77:
            return DRUM_STICK;
        default:
            if (note < 40) return DRUM_KICK;
            if (note < 50) return DRUM_TOM;
            return DRUM_SHAKER;
    }
}

void drum_on(Synth *s, DrumType type) {
    int di = 0;
    float oldest = 0;

    for (int i = 0; i < MAX_DRUMS; i++) {
        if (!s->drums[i].active) {
            di = i;
            goto found;
        }
        if (s->drums[i].time > oldest) {
            oldest = s->drums[i].time;
            di = i;
        }
    }

found:
    s->drums[di].active = 1;
    s->drums[di].type = type;
    s->drums[di].time = 0;
    s->drums[di].phase = 0;
    s->drums[di].filt = 0;
    s->drums[di].filt2 = 0;
}

int scancode_to_offset(SDL_Scancode sc) {
    switch (sc) {
        case SDL_SCANCODE_Z: return 0;
        case SDL_SCANCODE_S: return 1;
        case SDL_SCANCODE_X: return 2;
        case SDL_SCANCODE_D: return 3;
        case SDL_SCANCODE_C: return 4;
        case SDL_SCANCODE_V: return 5;
        case SDL_SCANCODE_G: return 6;
        case SDL_SCANCODE_B: return 7;
        case SDL_SCANCODE_H: return 8;
        case SDL_SCANCODE_N: return 9;
        case SDL_SCANCODE_J: return 10;
        case SDL_SCANCODE_M: return 11;
        case SDL_SCANCODE_Q: return 12;
        case SDL_SCANCODE_2: return 13;
        case SDL_SCANCODE_W: return 14;
        case SDL_SCANCODE_3: return 15;
        case SDL_SCANCODE_E: return 16;
        case SDL_SCANCODE_R: return 17;
        case SDL_SCANCODE_5: return 18;
        case SDL_SCANCODE_T: return 19;
        case SDL_SCANCODE_6: return 20;
        case SDL_SCANCODE_Y: return 21;
        case SDL_SCANCODE_7: return 22;
        case SDL_SCANCODE_U: return 23;
        case SDL_SCANCODE_I: return 24;
        case SDL_SCANCODE_9: return 25;
        case SDL_SCANCODE_O: return 26;
        case SDL_SCANCODE_0: return 27;
        case SDL_SCANCODE_P: return 28;
        default: return -1;
    }
}

void SDLCALL audio_cb(void *userdata, SDL_AudioStream *stream, int additional, int total) {
    (void)total;

    Synth *s = (Synth *)userdata;
    int nsamples = additional / sizeof(float);
    if (nsamples <= 0) return;

    int buf_cap = (int)(sizeof(audio_buf_static) / sizeof(float));
    if (nsamples > buf_cap) nsamples = buf_cap;
    float *buf = audio_buf_static;
    float dt = 1.0f / SAMPLE_RATE;

    float alpha = s->cutoff * s->cutoff;
    if (alpha < 0.001f) alpha = 0.001f;
    if (alpha > 1.0f) alpha = 1.0f;
    float fb = s->resonance * 3.5f;
    OscType osc = s->osc;
    float vol = s->volume;
    float dvol = s->drum_vol;

    int active_count = 0;
    for (int v = 0; v < MAX_VOICES; v++)
        if (s->voices[v].active) active_count++;
    float voice_scale = (active_count > 1) ? 1.0f / sqrtf((float)active_count) : 1.0f;

    for (int i = 0; i < nsamples; i++) {
        float mix = 0;

        for (int v = 0; v < MAX_VOICES; v++) {
            if (!s->voices[v].active) continue;
            Voice *vc = &s->voices[v];
            float env = env_tick(vc, &s->adsr, dt);
            float samp = osc_sample(vc->phase, osc, s->waveform) * env;

            samp = samp - fb * (vc->filt_state - samp);
            vc->filt_state += alpha * (samp - vc->filt_state);
            samp = vc->filt_state;

            mix += samp;
            vc->phase += vc->freq * dt;
            if (vc->phase >= 1.0f) vc->phase -= 1.0f;
        }

        mix *= voice_scale;

        if (s->seq_playing && s->seq_length > 0) {
            float bpm = s->seq_tempo;
            if (bpm < 40.0f) bpm = 40.0f;
            if (bpm > 240.0f) bpm = 240.0f;
            float step_len = 15.0f / bpm;

            if (s->seq_step < 0) {
                s->seq_step = 0;
                s->seq_phase = 0.0f;
                trigger_seq_step(s, 0);
            }

            s->seq_phase += dt;
            while (s->seq_phase >= step_len) {
                s->seq_phase -= step_len;
                s->seq_step = (s->seq_step + 1) % s->seq_length;
                trigger_seq_step(s, s->seq_step);
            }
        }

        float drum_mix = 0;
        for (int d = 0; d < MAX_DRUMS; d++)
            drum_mix += drum_sample(&s->drums[d], dt);

        mix = mix * vol + drum_mix * dvol;
        mix = fast_tanh(mix * 1.5f) * 0.9f;
        buf[i] = mix;

        if ((i & 3) == 0) {
            s->scope_buf[s->scope_pos % SCREEN_W] = mix;
            s->scope_pos++;
        }
    }

    if (s->recording) {
        if (s->rec_len + nsamples > s->rec_cap) {
            s->rec_cap = (s->rec_len + nsamples) * 2;
            s->rec_buf = (float *)SDL_realloc(s->rec_buf, s->rec_cap * sizeof(float));
        }
        if (s->rec_buf) {
            memcpy(s->rec_buf + s->rec_len, buf, nsamples * sizeof(float));
            s->rec_len += nsamples;
        }
    }

    if (s->rec_playing && s->rec_buf && s->rec_len > 0) {
        for (int i = 0; i < nsamples; i++) {
            if (s->rec_playpos >= s->rec_len) {
                s->rec_playing = 0;
                break;
            }
            buf[i] += s->rec_buf[s->rec_playpos++];
            if (buf[i] > 1.0f) buf[i] = 1.0f;
            if (buf[i] < -1.0f) buf[i] = -1.0f;
        }
    }

    SDL_PutAudioStreamData(stream, buf, nsamples * sizeof(float));
}
