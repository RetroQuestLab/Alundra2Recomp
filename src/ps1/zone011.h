#pragma once
/*
 * Chargeur minimal de la zone 011 (Village Paco) pour la cible PS1.
 * Parse le blob P1ZM produit par tools/ps1_export_zone.py, sans allocation
 * dynamique : tous les pointeurs sont des offsets dans le blob embarque.
 */
#include <stdint.h>

/* Televerse les 821 tuiles 4bpp et les CLUT en VRAM. A appeler apres pfInit. */
void zoneInit(void);

/* Dessine le decor visible. Le parametre z est ignore : la priorite est
 * calculee par ligne comme dans Map::draw (63 - (y & 63)). */
void zoneDraw(int camX, int camY, int z);

/* Acces aux cellules pour la physique (coordonnees pixels ou grille). */
const uint8_t* zoneCells(void);
int zoneW(void);
int zoneH(void);
uint8_t zoneAttr(int cx, int cy);
int zoneHeight(int cx, int cy);
uint16_t zoneTop(int cx, int cy);
