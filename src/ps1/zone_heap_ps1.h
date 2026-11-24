#pragma once

#include "zone_heap_ps1_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

/* POD(lisible par la sonde Lua), 16 mots, voir zone_heap_ps1_logic.h. */
extern PS1ZoneHeapDiag g_ps1ZoneHeapDiag;

/* Avant pfInit : etend l'arene malloc pour accueillir un blob P1ZM entier. */
void zoneHeapInitPS1(void);

/* Une commande par image, uniquement si le harnais a arme cmd/done. */
void zoneHeapSamplePS1(void);

/* Jeu normal : charge depuis le CD une zone non embarquee (026) dans l'arene
 * malloc, libere le blob precedent, parse et televerse en VRAM. Arrete le flux
 * musical (meme lecteur CD) : l'appelant relance la musique de la zone.
 * Retourne 1 si la carte est active, 0 sinon (carte courante conservee). */
int zoneLoadFromCdPS1(uint32_t zone);

#ifdef __cplusplus
}
#endif
