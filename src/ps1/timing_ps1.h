#pragma once

#include <stdint.h>

/* Instrumentation du temps d'image PS1 (POD, aucune allocation, aucune STL).
 * La sonde Lua lit this struct via son adresse dans la map ELF.
 *
 * Un echantillon = periode entre deux retours de VSync(0) dans pfPresent(),
 * lue par acces direct au compteur racine 1 (MMIO 0x1f801110, 16 bits).
 * GetRCnt() n'est pas utilise : sous OpenBIOS/PCSX-Redux il rend toujours 0
 * alors que le registre materiel avance. Les ticks BRUTS sont la mesure ; la
 * conversion en ms utilise un taux nominal et doit etre presentee comme telle,
 * jamais comme une mesure. */

#define PS1_FRAME_TIMING_SAMPLES 1024

typedef struct PS1FrameTiming {
	uint32_t ready;      /* 1 des que le premier echantillon est valide */
	uint32_t count;      /* nombre d'echantillons valides (plafonne a 1024) */
	uint32_t head;       /* index d'ecriture du ring */
	uint32_t lastTicks;  /* derniere valeur RCnt lue (16 bits) */
	uint16_t samples[PS1_FRAME_TIMING_SAMPLES];  /* periodes en ticks RCnt */
} PS1FrameTiming;

extern PS1FrameTiming g_ps1FrameTiming;
