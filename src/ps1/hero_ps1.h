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
