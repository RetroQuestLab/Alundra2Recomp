#pragma once
/* Genere par tools/ps1_export_audio.py - ne pas editer a la main. */
#include <stdint.h>

#define PS1_AUDIO_BANK_SIZE 378304
#define PS1_AUDIO_SFX_COUNT 15
#define PS1_AUDIO_MUSIC_ADDR 0x00015d40
#define PS1_AUDIO_MUSIC_SIZE 288864

struct PS1AudioClip {
	uint16_t id;
	uint32_t addr;
	uint32_t size;
};

static const struct PS1AudioClip g_ps1AudioSfx[PS1_AUDIO_SFX_COUNT] = {
	{ 1, 0x00000000, 1520 },
	{ 2, 0x00000600, 3664 },
	{ 3, 0x00001480, 3024 },
	{ 12, 0x00002080, 4160 },
	{ 13, 0x000030c0, 4224 },
	{ 15, 0x00004140, 4160 },
	{ 20, 0x00005180, 4096 },
	{ 29, 0x00006180, 7392 },
	{ 33, 0x00007e80, 3296 },
	{ 36, 0x00008b80, 8448 },
	{ 41, 0x0000ac80, 15664 },
	{ 63, 0x0000e9c0, 13232 },
	{ 66, 0x00011d80, 6656 },
	{ 70, 0x00013780, 4512 },
	{ 71, 0x00014940, 5072 },
};
