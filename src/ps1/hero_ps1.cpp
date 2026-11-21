/*
 * Hero jouable PS1 : deplacement, course, saut, attaque et animation.
 * Le blob P1SP garde les pixels en RAM ; seule la frame courante est copiee
 * dans le scratch VRAM, ce qui evite de reserver ~130 Ko de VRAM au heros.
 */
#include "hero_ps1.h"

#include "../platform/pf.h"
#include "zone011.h"

#include <stddef.h>

extern "C" {
extern const uint8_t hero_dat[];
extern const uint32_t hero_dat_size;
}

namespace {

constexpr int kTileW = 24;
constexpr int kTileH = 16;
constexpr int kClutWords = 16;
constexpr int kHalfW = 6;   // demi-largeur de collision (pixels)
constexpr int kHalfD = 3;   // profondeur de collision (pixels)
constexpr int kFp = 16;
constexpr int32_t kOne = 1 << kFp;
constexpr int32_t kWalkSpeed = 81920;   // 1,25 px/frame
constexpr int32_t kRunSpeed = 137625;   // 2,10 px/frame
constexpr int32_t kJumpVel = -6 * (1 << kFp);
constexpr int32_t kGravity = (1 << kFp) / 2;  // 0,5 px/frame^2

const uint8_t* g_frames = 0;
const uint8_t* g_anims = 0;
const uint8_t* g_clutRects = 0;
const uint8_t* g_frameClut = 0;
uint32_t g_frameOff[128];
uint16_t g_frameStride[128];  // demi-mots VRAM par ligne (aligne sur 8)
uint16_t g_frameUploadH[128]; // lignes televersees (alignees sur 16 mots 32 bits)
int g_nFrames = 0;
int g_nGroups = 0;
int g_nCluts = 0;

// Etat du heros (16.16 pour la position et le saut).
int32_t g_x = 0;
int32_t g_y = 0;
int32_t g_jumpY = 0;
int32_t g_jumpV = 0;
int g_z = 1;
int g_dir = PF_DIR_DOWN; // ordre A1SP : 0=haut, 1=gauche, 2=droite, 3=bas
int g_group = 0;        // 0=repos, 1=marche, 2=course, 3=saut, 4=epee
int g_step = 0;
int g_timer = 0;
int g_lastUploaded = -1;
uint32_t g_prevPad = 0;
bool g_attack = false;

inline uint16_t rd16(const uint8_t* p) {
	return (uint16_t)(p[0] | (p[1] << 8));
}

inline int16_t rd16s(const uint8_t* p) {
	return (int16_t)rd16(p);
}

bool walkable(int px, int py) {
	const int cx = px / kTileW;
	const int cy = py / kTileH;
	if (cx < 0 || cy < 0 || cx >= zoneW() || cy >= zoneH()) return false;
	const uint8_t* c = zoneCells() + ((size_t)cy * (size_t)zoneW() + (size_t)cx) * 8;
	const uint16_t top = (uint16_t)(c[4] | (c[5] << 8));
	return top != 0xFFFF && !(c[3] & 0x11) && ((int)c[0] - g_z >= -1) && ((int)c[0] - g_z <= 1);
}

bool canStand(int xpx, int ypx) {
	// Port exact de world.cpp::canStand (4 points d'appui).
	return walkable(xpx - kHalfW, ypx - kHalfD) && walkable(xpx + kHalfW, ypx - kHalfD) &&
	       walkable(xpx - kHalfW, ypx) && walkable(xpx + kHalfW, ypx);
}

int dirFromPad(uint32_t pad) {
	// Ordre des frames A1SP : UP=0, LEFT=1, RIGHT=2, DOWN=3 (voir pf.h).
	// Ne pas confondre avec l'ordre des bits PAD (PF_BTN_*) : l'ancien
	// mapping RIGHT=1/LEFT=2 faisait marcher le heros a reculons sur les
	// deux profils (retour materiel 2026-11-21 + inspection des frames).
	if (pad & PF_BTN_DOWN) return PF_DIR_DOWN;
	if (pad & PF_BTN_UP) return PF_DIR_UP;
	if (pad & PF_BTN_LEFT) return PF_DIR_LEFT;
	if (pad & PF_BTN_RIGHT) return PF_DIR_RIGHT;
	return g_dir;
}

// Les animations sont stockees apres les frames ; on calcule leur offset exact
// une fois pour toutes dans heroInit et on le garde dans g_anims.
const uint8_t* dirSteps(int group, int dir, int* count) {
	// Chaque direction : u16 count puis count * {u16 frame, u16 ticks}.
	const uint8_t* p = g_anims;
	for (int i = 0; i < group * 4 + dir; ++i) {
		const int n = rd16(p) & 0x7FFF;
		p += 2 + (size_t)n * 4;
	}
	const int n = rd16(p) & 0x7FFF;
	*count = n;
	return p + 2;
}

}  // namespace

void heroInit(void) {
	const uint8_t* p = hero_dat + 4;  // magic P1SP
	g_nFrames = rd16(p);
	g_nGroups = rd16(p + 2);
	g_nCluts = rd16(p + 4);
	p += 8;

	g_frameClut = p;
	p += g_nFrames * 2;
	g_clutRects = p;
	p += g_nCluts * 4;
	p += g_nCluts * kClutWords * 2;  // CLUT deja televersees par zoneInit

	g_frames = p;
	for (int i = 0; i < g_nFrames && i < 128; ++i) {
		g_frameOff[i] = (uint32_t)(p - hero_dat);
		const int h = rd16(p + 2);
		g_frameStride[i] = rd16(p + 8);
		g_frameUploadH[i] = rd16(p + 10);
		p += 12 + (size_t)g_frameStride[i] * 2 * (size_t)g_frameUploadH[i];
	}
	g_anims = p;

	g_x = 660 * kOne;
	g_y = 760 * kOne;
	g_z = 1;
	g_jumpY = 0;
	g_jumpV = 0;
	g_dir = 0;
	g_group = 0;
	g_step = 0;
	g_timer = 0;
	g_lastUploaded = -1;
	g_prevPad = 0;
}

void heroPos(int* x, int* y) {
	if (x) *x = (int)(g_x >> kFp);
	if (y) *y = (int)(g_y >> kFp);
}

void heroUpdate(uint32_t pad, uint32_t /*frame*/) {
	const uint32_t pressed = pad & ~g_prevPad;
	g_prevPad = pad;

	const int cx = (int)(g_x >> kFp) / kTileW;
	const int cy = (int)(g_y >> kFp) / kTileH;
	g_z = zoneHeight(cx, cy);

	// Attaque : maintien de Carre, prioritaire sur les autres animations.
	g_attack = (pad & PF_BTN_SQUARE) != 0;

	const bool moving = (pad & (PF_BTN_UP | PF_BTN_DOWN | PF_BTN_LEFT | PF_BTN_RIGHT)) != 0;
	if (moving) g_dir = dirFromPad(pad);

	const int32_t speed = (pad & PF_BTN_R1) ? kRunSpeed : kWalkSpeed;
	int32_t dx = 0;
	int32_t dy = 0;
	if (pad & PF_BTN_RIGHT) dx += speed;
	if (pad & PF_BTN_LEFT) dx -= speed;
	if (pad & PF_BTN_DOWN) dy += speed;
	if (pad & PF_BTN_UP) dy -= speed;
	if (dx && dy) {  // diagonale normalisee grossierement
		dx = (dx * 3) / 4;
		dy = (dy * 3) / 4;
	}

	if (dx) {
		const int32_t nx = g_x + dx;
		if (canStand((int)(nx >> kFp), (int)(g_y >> kFp))) g_x = nx;
	}
	if (dy) {
		const int32_t ny = g_y + dy;
		if (canStand((int)(g_x >> kFp), (int)(ny >> kFp))) g_y = ny;
	}

	// Saut : impulsion sur front montant, parabole en fixed-point.
	if ((pressed & PF_BTN_CROSS) && g_jumpY == 0) {
		g_jumpV = kJumpVel;
	}
	if (g_jumpV != 0 || g_jumpY < 0) {
		g_jumpV += kGravity;
		g_jumpY += g_jumpV;
		if (g_jumpY >= 0) {
			g_jumpY = 0;
			g_jumpV = 0;
		}
	}

	// Choix du groupe d'animation.
	int want;
	if (g_attack) want = 4;
	else if (g_jumpY < 0) want = 3;
	else if (moving) want = (pad & PF_BTN_R1) ? 2 : 1;
	else want = 0;
	if (want != g_group) {
		g_group = want;
		g_step = 0;
		g_timer = 0;
	}

	// Avancee des steps : ticks exprimes en 1/60 s dans le blob A1SP.
	// Le groupe 0 de hero.a1s alterne des poses de jambes qui donnent
	// l'impression d'une marche sur place (signale sur materiel reel) :
	// on le fige sur sa premiere frame, qui est une pose debout.
	int count = 0;
	const uint8_t* steps = dirSteps(g_group, g_dir, &count);
	if (g_group == 0) {
		g_step = 0;
		g_timer = 0;
	} else if (count > 0) {
		int step = g_step;
		if (step >= count) step = 0;
		if (++g_timer >= (int)rd16(steps + step * 4 + 2)) {
			g_timer = 0;
			g_step = (g_step + 1) % count;
		}
	}
}

void heroDraw(int camX, int camY) {
	int count = 0;
	const uint8_t* steps = dirSteps(g_group, g_dir, &count);
	if (count <= 0) return;
	int step = g_step;
	if (step >= count) step = 0;
	const int frame = rd16(steps + step * 4);
	if (frame < 0 || frame >= g_nFrames || frame >= 128) return;

	// Une seule frame en VRAM : televersement a chaque changement d'etape.
	if (frame != g_lastUploaded) {
		const uint8_t* f = hero_dat + g_frameOff[frame];
		const int w = rd16(f);
		const int h = rd16(f + 2);
		const int stride = g_frameStride[frame];
		const int uploadH = g_frameUploadH[frame];
		pfUpload4bpp(PF_HERO_SCRATCH_X, PF_HERO_SCRATCH_Y, stride, uploadH, f + 12);
		g_lastUploaded = frame;
	}

	const uint8_t* f = hero_dat + g_frameOff[frame];
	const int w = rd16(f);
	const int h = rd16(f + 2);
	const int ox = rd16s(f + 4);
	const int oy = rd16s(f + 6);
	const uint16_t ci = rd16(g_frameClut + frame * 2);
	const uint8_t* cr = g_clutRects + (int)ci * 4;

	const int sx = (int)(g_x >> kFp) - camX - ox;
	const int sy = (int)(g_y >> kFp) - camY - oy + (int)(g_jumpY >> kFp);
	const int z = 63 - ((((int)(g_y >> kFp)) / kTileH) & 63);
	pfDrawSprite16(sx, sy, PF_HERO_SCRATCH_X, PF_HERO_SCRATCH_Y, w, h,
	               rd16(cr), rd16(cr + 2), z > 0 ? z : 0);
}
