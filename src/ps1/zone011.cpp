/*
 * Zone 011 : parsing P1ZM, upload VRAM et dessin des tuiles 24x16.
 * Aucune STL ; le blob P1ZM est lu sur le CD (zone_heap_ps1.cpp).
 */
#include "zone011.h"

#include "../platform/pf.h"
#include "zone_heap_ps1.h"

#include <stddef.h>


namespace {

constexpr int kTileW = 24;
constexpr int kTileH = 16;
constexpr int kMarginRows = 16;
constexpr int kClutWords = 16;

const uint8_t* g_cells = 0;
const uint8_t* g_walls = 0;
const uint8_t* g_tileRects = 0;
const uint8_t* g_clutRects = 0;
const uint8_t* g_tileClut = 0;
const uint8_t* g_tiles = 0;
uint32_t g_nWalls = 0;
int g_w = 0;
int g_h = 0;
int g_nTiles = 0;
int g_nCluts = 0;

inline uint16_t rd16(const uint8_t* p) {
	return (uint16_t)(p[0] | (p[1] << 8));
}

inline uint32_t rd32(const uint8_t* p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void drawTile(int tile, int x, int y, int z) {
	if (tile == 0xFFFF || tile < 0 || tile >= g_nTiles) return;
	const uint8_t* r = g_tileRects + tile * 8;
	const int vx = rd16(r);
	const int vy = rd16(r + 2);
	const uint16_t ci = rd16(g_tileClut + tile * 2);
	const uint8_t* cr = g_clutRects + (int)ci * 4;
	pfDrawSprite16(x, y, vx, vy, kTileW, kTileH, rd16(cr), rd16(cr + 2), z);
}

const uint8_t* cellAt(int cx, int cy) {
	if (cx < 0 || cy < 0 || cx >= g_w || cy >= g_h) return 0;
	return g_cells + ((size_t)cy * (size_t)g_w + (size_t)cx) * 8;
}

}  // namespace

void loadZone(const uint8_t* blob) {
	const uint8_t* p = blob + 4;  // magic P1ZM
	g_w = rd16(p);
	g_h = rd16(p + 2);
	g_nTiles = rd16(p + 4);
	g_nCluts = rd16(p + 6);
	p += 10;

	g_tileClut = p;
	p += g_nTiles * 2;
	g_tileRects = p;
	p += g_nTiles * 8;
	g_clutRects = p;
	p += g_nCluts * 4;
	const uint8_t* cluts = p;
	p += g_nCluts * kClutWords * 2;
	g_tiles = p;
	p += g_nTiles * 192;  // 24 px / 4 = 6 demi-mots * 2 octets * 16 lignes
	g_cells = p;
	p += (size_t)g_w * (size_t)g_h * 8;
	g_nWalls = rd32(p);
	p += 4;
	g_walls = p;

	// CLUT : 16 mots 16 bits a televerser avant le premier affichage.
	for (int i = 0; i < g_nCluts; ++i) {
		const uint8_t* cr = g_clutRects + i * 4;
		pfUpload16(rd16(cr), rd16(cr + 2), kClutWords, 1,
		           reinterpret_cast<const uint16_t*>(cluts + i * kClutWords * 2));
	}

	// Tuiles : g_tiles est contigu, chaque blob fait exactement 192 octets.
	const uint8_t* t = g_tiles;
	for (int i = 0; i < g_nTiles; ++i) {
		const uint8_t* r = g_tileRects + i * 8;
		pfUpload4bpp(rd16(r), rd16(r + 2), rd16(r + 4), rd16(r + 6), t);
		t += 192;
	}
}

// Toutes les cartes sont lues sur le CD (v0.8.31) : voir zone_heap_ps1.cpp.
void zoneInit(void) { zoneLoadFromCdPS1(11u); }
void zone013Init(void) { zoneLoadFromCdPS1(13u); }
void zone019Init(void) { zoneLoadFromCdPS1(19u); }
void zone119Init(void) { zoneLoadFromCdPS1(119u); }
void zone120Init(void) { zoneLoadFromCdPS1(120u); }

void zoneDraw(int camX, int camY, int /*z*/) {
	// Meme rendu que Map::draw : rangees du haut vers le bas (z decroissant).
	// Dans une rangee, toutes les tuiles ont le meme z, et addPrim insere en
	// tete : l'ordre de DESSIN est l'inverse de l'ordre d'AJOUT. Pour dessiner
	// comme le PC (gauche -> droite, murs puis dessus), on ajoute donc de
	// droite a gauche, le dessus avant les murs, et les murs du dernier au
	// premier. Avant v0.8.37, l'herbe des murs recouvrait les paves de Paco.
	for (int y = 0; y < g_h; ++y) {
		const int rowZ = 63 - (y & 63);
		for (int x = g_w - 1; x >= 0; --x) {
			const int sx = x * kTileW - camX;
			if (sx + kTileW < 0 || sx >= PF_W) continue;

			const uint8_t* c = cellAt(x, y);
			if (!c) continue;
			const int height = c[0];
			const int wallOffset = (int8_t)c[1];
			const int wallCount = c[2];
			const uint16_t top = rd16(c + 4);
			const uint16_t firstWall = rd16(c + 6);
			const int sy = (kMarginRows + y - height) * kTileH - camY;

			if (sy + kTileH >= 0 && sy < PF_H) drawTile(top, sx, sy, rowZ);
			for (int i = wallCount - 1; i >= 0; --i) {
				const int wy = sy + (i + 1 - wallOffset) * kTileH;
				if (wy + kTileH < 0 || wy >= PF_H) continue;
				if ((uint32_t)firstWall + i >= g_nWalls) continue;
				drawTile(rd16(g_walls + ((uint32_t)firstWall + i) * 2), sx, wy, rowZ);
			}
		}
	}
}

const uint8_t* zoneCells(void) { return g_cells; }
int zoneW(void) { return g_w; }
int zoneH(void) { return g_h; }

uint8_t zoneAttr(int cx, int cy) {
	const uint8_t* c = cellAt(cx, cy);
	return c ? c[3] : 0xFF;
}

int zoneHeight(int cx, int cy) {
	const uint8_t* c = cellAt(cx, cy);
	return c ? c[0] : 0;
}

int zoneElevAt(int px, int py) {
	const uint8_t* c = cellAt(px / kTileW, py / kTileH);
	return (kMarginRows - (c ? c[0] : 0)) * kTileH;
}

uint16_t zoneTop(int cx, int cy) {
	const uint8_t* c = cellAt(cx, cy);
	return c ? rd16(c + 4) : 0xFFFF;
}
