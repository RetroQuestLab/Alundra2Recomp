#pragma once
/*
 * Introduction PS1 native : prologue Alundra 2 (Flint, navire pirate, chute,
 * plage de Paco) presenté avec les decors et la police Alundra 1.
 * START passe immediatement l'introduction ; l'etat de jeu ne change pas.
 */
#include <stdint.h>

/* padSeed : masque manette courant. Le front START utilise pour choisir
 * « Nouvelle partie » ne doit pas etre reinterprete comme « passer l'intro ». */
void introInitPS1(uint32_t padSeed);
void introUpdatePS1(uint32_t pad);
void introDrawPS1(void);
int introDonePS1(void);
int introSkippedPS1(void);
