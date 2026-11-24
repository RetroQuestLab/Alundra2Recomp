#pragma once
/*
 * Chargeur des zones PS1 011 (Paco), 013 (Kindra), 019 (Epave), 119
 * (Toroledo) et 120 (salle du trone). Parse le blob P1ZM produit par tools/ps1_export_zone.py, sans
 * allocation dynamique : tous les pointeurs sont des offsets dans le blob.
 */
#include <stdint.h>

/* Televerse les tuiles 4bpp et les CLUT en VRAM. A appeler apres pfInit. */
void zoneInit(void);

/* Parse un blob P1ZM deja en RAM (harnais de mesure) et televerse ses tuiles.
 * Le decor precedent reste en VRAM jusqu'a ecrasement. */
void loadZone(const uint8_t* blob);

/* Charge un autre blob P1ZM embarque. Le decor precedent reste en VRAM
 * jusqu'a ecrasement : une seule zone est active a la fois. */
void zone013Init(void);
void zone019Init(void);
void zone119Init(void);
void zone120Init(void);

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

/* Decalage vertical ecran d'un objet pose au sol au pixel (px, py), comme
 * screenY() de src/main.cpp : (MARGIN_ROWS - hauteur) * TILE_H. Les tuiles
 * sont dessinees a (MARGIN_ROWS + y - hauteur) * 16 : sans ce decalage, un
 * objet sur un terrain de hauteur != 16 est dessine hors de son decor. */
int zoneElevAt(int px, int py);
