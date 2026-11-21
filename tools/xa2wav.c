/* Decode les flux audio XA (ADPCM CD-ROM XA) d'une image disque PS1 brute en fichiers WAV.
 *   xa2wav <image.bin> <lba> <nb_secteurs> <dossier_sortie> <prefixe>
 * Un fichier par (fichier, canal) entrelace, coupe a chaque marque de fin (submode EOF).
 * Compilation : clang -O2 -o xa2wav.exe tools/xa2wav.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define SECTOR 2352
#define NSTREAM 64

typedef struct {
    int16_t *pcm; size_t n, cap; int stereo, rate, seg;
    int h[2][2];
} Stream;

static const int K0[4] = {0, 60, 115, 98}, K1[4] = {0, 0, -52, -55};

static void push(Stream *s, int16_t v) {
    if (s->n == s->cap) { s->cap = s->cap ? s->cap * 2 : 1 << 20; s->pcm = realloc(s->pcm, s->cap * 2); }
    s->pcm[s->n++] = v;
}

static void wr32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void wr16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }

static void flush(Stream *s, const char *dir, const char *pre, int id) {
    if (s->n < 4000) { s->n = 0; return; }
    char path[512];
    snprintf(path, sizeof path, "%s/%s_%02d_%02d.wav", dir, pre, id, s->seg++);
    FILE *f = fopen(path, "wb");
    int ch = s->stereo ? 2 : 1;
    uint32_t bytes = (uint32_t)(s->n * 2);
    fwrite("RIFF", 1, 4, f); wr32(f, 36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
    wr32(f, 16); wr16(f, 1); wr16(f, ch); wr32(f, s->rate); wr32(f, s->rate * ch * 2); wr16(f, ch * 2); wr16(f, 16);
    fwrite("data", 1, 4, f); wr32(f, bytes); fwrite(s->pcm, 2, s->n, f);
    fclose(f);
    printf("%s %.1f s\n", path, (double)s->n / ch / s->rate);
    s->n = 0;
    memset(s->h, 0, sizeof s->h);
}

int main(int argc, char **argv) {
    if (argc < 6) return 1;
    FILE *f = fopen(argv[1], "rb");
    long lba = atol(argv[2]), count = atol(argv[3]);
    static Stream st[NSTREAM];
    uint8_t sec[SECTOR];
    for (long i = 0; i < count; i++) {
        _fseeki64(f, (long long)(lba + i) * SECTOR, SEEK_SET);
        if (fread(sec, 1, SECTOR, f) != SECTOR) break;
        uint8_t file = sec[16], chan = sec[17], sub = sec[18], coding = sec[19];
        if (!(sub & 0x04)) continue;              /* pas de l'audio */
        int id = ((file & 1) << 5 | (chan & 31)) % NSTREAM;
        Stream *s = &st[id];
        s->stereo = coding & 1;
        s->rate = (coding & 4) ? 18900 : 37800;
        if (coding & 0x10) continue;              /* 8 bits : non gere */
        for (int g = 0; g < 18; g++) {
            const uint8_t *grp = sec + 24 + g * 128;
            if (s->stereo) {
                int16_t L[4][28], R[4][28];
                for (int u = 0; u < 8; u++) {
                    int p = grp[4 + u], shift = p & 15, filt = (p >> 4) & 3, c = u & 1;
                    for (int k = 0; k < 28; k++) {
                        int b = grp[16 + k * 4 + u / 2];
                        int nib = (u & 1) ? b >> 4 : b & 15;
                        int v = (int16_t)(nib << 12) >> shift;
                        v += (s->h[c][0] * K0[filt] + s->h[c][1] * K1[filt] + 32) >> 6;
                        if (v > 32767) v = 32767; if (v < -32768) v = -32768;
                        s->h[c][1] = s->h[c][0]; s->h[c][0] = v;
                        if (c) R[u / 2][k] = (int16_t)v; else L[u / 2][k] = (int16_t)v;
                    }
                }
                for (int u = 0; u < 4; u++)
                    for (int k = 0; k < 28; k++) { push(s, L[u][k]); push(s, R[u][k]); }
            } else {
                for (int u = 0; u < 8; u++) {
                    int p = grp[4 + u], shift = p & 15, filt = (p >> 4) & 3;
                    for (int k = 0; k < 28; k++) {
                        int b = grp[16 + k * 4 + u / 2];
                        int nib = (u & 1) ? b >> 4 : b & 15;
                        int v = (int16_t)(nib << 12) >> shift;
                        v += (s->h[0][0] * K0[filt] + s->h[0][1] * K1[filt] + 32) >> 6;
                        if (v > 32767) v = 32767; if (v < -32768) v = -32768;
                        s->h[0][1] = s->h[0][0]; s->h[0][0] = v;
                        push(s, (int16_t)v);
                    }
                }
            }
        }
        if (sub & 0x80) flush(s, argv[4], argv[5], id);   /* fin d'un morceau */
    }
    for (int i = 0; i < NSTREAM; i++) flush(&st[i], argv[4], argv[5], i);
    return 0;
}
