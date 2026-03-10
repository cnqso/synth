#include "midi.h"

#include <stdio.h>
#include <string.h>

#include "audio.h"

static int read_u16be(const uint8_t *p) {
    return (p[0] << 8) | p[1];
}

static int read_u32be(const uint8_t *p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static int read_vlq(const uint8_t *p, int *out) {
    int val = 0;
    int i = 0;

    for (;;) {
        val = (val << 7) | (p[i] & 0x7F);
        if (!(p[i] & 0x80)) {
            *out = val;
            return i + 1;
        }
        i++;
        if (i > 4) {
            *out = 0;
            return i;
        }
    }
}

int load_midi(Synth *s, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *buf = (uint8_t *)SDL_malloc(sz);
    if (!buf) {
        fclose(f);
        return 0;
    }

    fread(buf, 1, sz, f);
    fclose(f);

    if (sz < 14 || memcmp(buf, "MThd", 4) != 0) {
        SDL_free(buf);
        return 0;
    }

    int tpq = read_u16be(buf + 12);
    if (tpq <= 0) tpq = 480;
    float tempo = 500000.0f;
    float tick_sec = tempo / (1000000.0f * tpq);

    s->roll_count = 0;
    int ntrks = read_u16be(buf + 10);
    int hdr_len = read_u32be(buf + 4);
    int pos = 8 + hdr_len;

    SDL_Log("MIDI: format=%d, tracks=%d, tpq=%d", read_u16be(buf + 8), ntrks, tpq);

    for (int trk = 0; trk < ntrks && pos + 8 <= sz && s->roll_count < MAX_ROLL; trk++) {
        while (pos + 8 <= sz && memcmp(buf + pos, "MTrk", 4) != 0) pos++;
        if (pos + 8 > sz) break;

        int tlen = read_u32be(buf + pos + 4);
        int tend = pos + 8 + tlen;
        if (tend > sz) tend = sz;
        int tp = pos + 8;
        float t = 0;
        uint8_t running = 0;
        float note_starts[16][128];

        memset(note_starts, 0, sizeof(note_starts));
        SDL_Log("  Track %d: %d bytes", trk, tlen);

        while (tp < tend && tp < sz) {
            int delta;
            tp += read_vlq(buf + tp, &delta);
            t += delta * tick_sec;

            if (tp >= sz) break;

            uint8_t status = buf[tp];
            if (status & 0x80) {
                running = status;
                tp++;
            } else {
                status = running;
            }

            uint8_t hi = status & 0xF0;
            int ch = status & 0x0F;
            int is_drum_ch = (ch == 9);

            if (hi == 0x90 && tp + 1 < sz) {
                int note = buf[tp] & 127;
                int vel = buf[tp + 1];
                tp += 2;
                if (vel > 0) {
                    if (is_drum_ch) {
                        if (s->roll_count < MAX_ROLL) {
                            s->roll[s->roll_count].note = note;
                            s->roll[s->roll_count].start = t;
                            s->roll[s->roll_count].dur = 0.05f;
                            s->roll[s->roll_count].is_drum = 1;
                            s->roll[s->roll_count].drum = gm_to_drum(note);
                            s->roll_count++;
                        }
                    } else {
                        note_starts[ch][note] = t;
                    }
                } else if (!is_drum_ch && s->roll_count < MAX_ROLL) {
                    s->roll[s->roll_count].note = note;
                    s->roll[s->roll_count].start = note_starts[ch][note];
                    s->roll[s->roll_count].dur = t - note_starts[ch][note];
                    if (s->roll[s->roll_count].dur < 0.01f) s->roll[s->roll_count].dur = 0.01f;
                    s->roll[s->roll_count].is_drum = 0;
                    s->roll_count++;
                }
            } else if (hi == 0x80 && tp + 1 < sz) {
                int note = buf[tp] & 127;
                tp += 2;
                if (!is_drum_ch && s->roll_count < MAX_ROLL) {
                    s->roll[s->roll_count].note = note;
                    s->roll[s->roll_count].start = note_starts[ch][note];
                    s->roll[s->roll_count].dur = t - note_starts[ch][note];
                    if (s->roll[s->roll_count].dur < 0.01f) s->roll[s->roll_count].dur = 0.01f;
                    s->roll[s->roll_count].is_drum = 0;
                    s->roll_count++;
                }
            } else if (status == 0xFF && tp + 1 < sz) {
                uint8_t mtype = buf[tp];
                tp++;
                int mlen;
                tp += read_vlq(buf + tp, &mlen);
                if (mtype == 0x51 && mlen == 3 && tp + 2 < sz) {
                    tempo = (float)((buf[tp] << 16) | (buf[tp + 1] << 8) | buf[tp + 2]);
                    tick_sec = tempo / (1000000.0f * tpq);
                }
                tp += mlen;
            } else if (hi == 0xC0 || hi == 0xD0) {
                tp += 1;
            } else if (status == 0xF0 || status == 0xF7) {
                int slen;
                tp += read_vlq(buf + tp, &slen);
                tp += slen;
            } else if (hi == 0xF0) {
                tp += 1;
            } else {
                tp += 2;
            }
        }

        pos = tend;
    }

    SDL_free(buf);

    s->midi_loaded = s->roll_count > 0;
    s->midi_playing = 0;
    s->midi_time = 0;
    s->midi_cursor = 0;
    s->roll_scroll = 0;

    for (int i = 1; i < s->roll_count; i++) {
        RollNote tmp = s->roll[i];
        int j = i - 1;
        while (j >= 0 && s->roll[j].start > tmp.start) {
            s->roll[j + 1] = s->roll[j];
            j--;
        }
        s->roll[j + 1] = tmp;
    }

    s->midi_max_t = 0;
    int ndrum = 0;
    int nmel = 0;
    for (int i = 0; i < s->roll_count; i++) {
        if (s->roll[i].is_drum) ndrum++;
        else nmel++;
        float end = s->roll[i].start + s->roll[i].dur;
        if (end > s->midi_max_t) s->midi_max_t = end;
    }

    SDL_Log("MIDI loaded: %d melodic notes, %d drum hits", nmel, ndrum);
    return s->midi_loaded;
}
