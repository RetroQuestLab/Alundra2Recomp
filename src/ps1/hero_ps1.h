#pragma once
/*
 * Hero PS1 : physique 16.16, animation P1SP et upload de la frame courante
 * dans le scratch VRAM. Aucune STL, aucune allocation dynamique.
 */
#include <stdint.h>

void heroInit(void);
void heroUpdate(uint32_t pad, uint32_t frame);
void heroDraw(int camX, int camY);
/* Position en pixels entiers (partie entiere du fixed-point 16.16). */
void heroPos(int* x, int* y);
/* Teleporte le heros (passages entre zones), en pixels entiers. */
void heroSetPos(int x, int y);
/* Direction courante (PF_DIR_*, ordre des frames A1SP). */
int heroDir(void);

/* Decalage vertical ecran du heros : (MARGIN_ROWS - niveau) * 16, comme
 * screenY()/followHero() cote PC. La camera le reutilise. */
int heroElevPS1(void);
/* Restaure la direction apres un chargement (0=haut, 1=gauche, 2=droite, 3=bas). */
void heroSetDirPS1(int dir);
/* Portrait A1 de Flint pour le panneau droit de l'inventaire : reutilise la
 * frame courante, televersee dans le scratch VRAM, puis restaure l'etat
 * d'upload pour que heroDraw() recharge sa frame au retour au jeu. */
void heroDrawPortraitPS1(int x, int y, int scale);
