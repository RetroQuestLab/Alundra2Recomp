/*
 * Introduction PS1 : reproduction du prologue PC (src/title.cpp::drawPrologue),
 * narration A2 de data/dialogues.txt [prologue] (tools/ps1_export_intro.py).
 *
 *  - cartes "042", "018", "026" : la carte A1 du meme nom (comme useZone() du
 *    PC, sans alias de jeu), lue sur le CD, en travelling lent ;
 *  - "navire" : ciel de nuit, lune, nuages, navire volant en ombre chinoise ;
 *  - "chute" : ciel de chute et le vrai sprite de Flint, agrandi et tournoyant ;
 *  - bandes de cinema, texte blanc centre avec ombre, nom en turquoise,
 *    fleche de suite, invite de passage.
 * Arithmetique entiere uniquement (pas de FPU sur le R3000A). Adaptations PS1,
 * imposees par le materiel : alpha remplace par la semi-transparence 50 %
 * (fondus et degrades en paliers), "C : passer" devient "START : passer".
 */
#include "intro_ps1.h"

#include "../platform/pf.h"
#include "a1_font_ps1.h"
#include "hero_ps1.h"
#include "intro_data_ps1.h"
#include "intro_ps1_logic.h"
#include "isin_ps1.h"
#include "window_ps1.h"
#include "zone011.h"
#include "zone_heap_ps1.h"
#include "audio_ps1.h"

namespace {

IntroState g_intro;
IntroCardMeta g_meta[32];
int g_loadedCard = -1;
int g_zoneOk = 0;

inline uint32_t rgb(int r, int g, int b) {
	return uint32_t(r >> 3) | (uint32_t(g >> 3) << 5) | (uint32_t(b >> 3) << 10);
}
inline int mix(int a, int b, int num, int den) { return a + (b - a) * num / den; }

/* Demi-largeurs floor(sqrt(r^2 - dy^2)) precalculees : le calcul par boucle a
 * chaque ligne ramenait le prologue a ~17 images/s sur le ciel du navire. */
constexpr int kMaxRad = 48;
uint8_t g_halfW[kMaxRad + 1][kMaxRad + 1];
int g_halfReady = 0;

void initHalfWidths() {
	for (int r = 0; r <= kMaxRad; ++r)
		for (int dy = 0; dy <= r; ++dy) {
			int w = 0;
			while ((w + 1) * (w + 1) + dy * dy <= r * r) ++w;
			g_halfW[r][dy] = uint8_t(w);
		}
	g_halfReady = 1;
}

void disc(int cx, int cy, int rad, uint32_t col, int z, int step = 1) {
	if (rad > kMaxRad) rad = kMaxRad;
	for (int dy = -rad; dy <= rad; dy += step) {
		const int w = g_halfW[rad][dy < 0 ? -dy : dy];
		pfDrawRect(cx - w, cy + dy, 2 * w + 1, step, col, z);
	}
}

void glyph(unsigned char c, int x, int y, int bank, int z) {
	const int idx = c >= 32 ? int(c) : int('?');
	pfDrawSprite16(x, y, PF_FONT_X + (idx & 15) * 4, PF_FONT_Y + (idx >> 4) * 16, 16, 16,
	               PF_FONT_CLUT_X, PF_FONT_CLUT_Y + bank, z);
}

int length(const char* s) { int n = 0; while (s[n]) ++n; return n; }

/* PC : x = floor((320 - 8 * taille) / 2), ombre (+1,+1) noire, n caracteres. */
void centeredShadow(const char* s, int y, int bank, int n, int z) {
	const int len = length(s);
	int x = (PF_W - 8 * len) / 2;
	for (int i = 0; i < len && i < n; ++i) {
		const unsigned char c = static_cast<unsigned char>(s[i]);
		glyph(c, x, y, bank, z);
		glyph(c, x + 1, y + 1, PF_FONT_BANK_SHADOW, z + 1);
		x += kA1FontAdvance[c >= 32 ? c : '?'];
	}
}

// --- Decors (src/title.cpp) ----------------------------------------------------------
void nightSky(int tick, int shipNum, int shipDen) {
	for (int y = 0; y < PF_H; y += 4)  // degrade par bandes de 4 lignes (cout CPU)
		pfDrawRect(0, y, PF_W, 4, rgb(mix(6, 34, y, PF_H), mix(8, 44, y, PF_H), mix(30, 96, y, PF_H)), 63);
	for (unsigned i = 0; i < 90; ++i) {
		const int x = int(hash16(i * 3 + 1) * PF_W >> 16), y = int(hash16(i * 3 + 2) * 170 >> 16);
		const int tw = 2048 + isinTick(tick, 1000 + int(hash16(i) * 2000 >> 16), int(i) * 652) / 2;  // 0..4096
		const int v = 70 + 170 * tw / 4096;
		pfDrawRect(x, y, 1, 1, rgb(210 * v / 255, 220 * v / 255, 255 * v / 255), 62);
		if (i % 11 == 0 && tw > 2400) {
			pfDrawRect(x - 1, y, 3, 1, rgb(60, 64, 80), 62);
			pfDrawRect(x, y - 1, 1, 3, rgb(60, 64, 80), 62);
		}
	}
	// Lune et halo (le PC additionne 4 disques : paliers equivalents).
	for (int k = 3; k >= 0; --k) disc(238, 64, 26 + k * 7, rgb(6 + 14 * (4 - k), 8 + 16 * (4 - k), 30 + 26 * (4 - k)), 61, 3);
	disc(238, 64, 21, rgb(238, 236, 214), 60);
	disc(231, 58, 4, rgb(214, 212, 190), 59);
	disc(245, 72, 6, rgb(214, 212, 190), 59);
	disc(242, 54, 2, rgb(214, 212, 190), 59);
	for (int layer = 0; layer < 3; ++layer) {
		const int zl = 50 - layer * 6;
		if (layer == 1 && shipDen > 0) {  // navire derriere les nuages proches
			const int sx = 96 + 150 * shipNum / shipDen, sy = 92 - 26 * shipNum / shipDen;
			const int bob = 2 * isinTick(tick, 1300) / 4096;
			const uint32_t hull = rgb(8, 9, 24);
			const int x = sx, y = sy + bob;
			for (int j = 0; j < 11; ++j) pfDrawRect(x - 34 + j * 16 / 10, y + j, 74 - j * 46 / 10, 1, hull, zl + 3);
			pfDrawRect(x - 36, y - 7, 16, 7, hull, zl + 3);
			pfDrawRect(x - 38, y - 9, 20, 2, hull, zl + 3);
			pfDrawRect(x + 26, y - 3, 12, 3, hull, zl + 3);
			for (int k = 0; k < 16; ++k) pfDrawRect(x + 38 + k, y - 3 - k / 3, 1, 1, hull, zl + 3);
			const int masts[2] = {x - 10, x + 14};
			for (int m = 0; m < 2; ++m) {
				const int mx = masts[m], h = m ? 30 : 36;
				pfDrawRect(mx, y - h, 2, h, hull, zl + 3);
				for (int j = 0; j < 12; ++j) pfDrawRect(mx - 9 - j * 3 / 10, y - h + 3 + j, 22 + j / 2, 1, hull, zl + 3);
				for (int j = 0; j < 9; ++j) pfDrawRect(mx - 8 - j * 3 / 10, y - h + 18 + j, 19 + j / 2, 1, hull, zl + 3);
				pfDrawRect(mx + 2, y - h - 1, 6, 2, hull, zl + 3);
				pfDrawRect(mx + 8, y - h, 3, 1, hull, zl + 3);
			}
			for (int j = 0; j < 4; ++j) {
				pfDrawRect(x - 20 - j * 3, y + 6 + j, 16, 1, hull, zl + 3);
				pfDrawRect(x + 6 - j * 3, y + 6 + j, 16, 1, hull, zl + 3);
			}
			int blade = 7 * isinTick(tick, 18000) / 4096;
			if (blade < 0) blade = -blade;
			pfDrawRect(x - 40, y + 4 - blade, 1, 2 * blade + 1, hull, zl + 3);
			pfDrawRect(x - 42, y + 3, 6, 2, hull, zl + 3);
			pfDrawRect(x - 30, y - 4, 2, 2, rgb(255, 196, 90), zl + 2);
			pfDrawRect(x + 4, y + 2, 1, 1, rgb(255, 196, 90), zl + 2);
			pfDrawRect(x + 20, y + 2, 1, 1, rgb(255, 196, 90), zl + 2);
		}
		const int speed = 7 + 11 * layer, baseY = 128 + 34 * layer, span = PF_W + 200;
		const uint32_t body = rgb(mix(52, 14, layer, 2), mix(62, 16, layer, 2), mix(112, 40, layer, 2));
		const uint32_t rim = rgb(mix(120, 40, layer, 2), mix(132, 46, layer, 2), mix(180, 86, layer, 2));
		for (unsigned i = 0; i < 7; ++i) {
			const unsigned seed = unsigned(layer) * 100u + i;
			int x = (int(hash16(seed) * span >> 16) - tick * speed / 60) % span;
			if (x < 0) x += span;
			x -= 100;
			const int y = baseY + int(26 * hash16(seed + 40) >> 16) + 2 * isinTick(tick, 400, int(seed) * 652) / 4096;
			for (unsigned b = 0; b < 6; ++b) {
				const int rad = 9 + int(12 * hash16(seed * 7 + b) >> 16) + 4 * layer;
				const int by = y + int(5 * hash16(seed * 13 + b) >> 16);
				// Bandes de 4 lignes : 64 Kio de primitives par image (~4 000 TILE).
				disc(x + int(b) * 15, by, rad, body, zl, 4);
				if (layer < 2) disc(x + int(b) * 15, by - 2, rad, rim, zl + 1, 4);  // plan proche : sans liseré (cout CPU)
			}
		}
	}
}

void fallScene(int tick) {
	for (int y = 0; y < PF_H; y += 4)
		pfDrawRect(0, y, PF_W, 4, rgb(mix(4, 22, y, PF_H), mix(6, 34, y, PF_H), mix(24, 78, y, PF_H)), 63);
	for (unsigned i = 0; i < 60; ++i) {
		int y = (int(hash16(i * 5 + 2) * 300 >> 16) - tick * (160 + int(120 * hash16(i) >> 16)) / 60) % 300;
		if (y < 0) y += 300;
		const int v = 60 + int(120 * hash16(i + 77) >> 16);
		pfDrawRect(int(hash16(i * 5 + 1) * PF_W >> 16), y - 30, 1, 5 + int(9 * hash16(i + 5) >> 16),
		           rgb(200 * v / 255, 214 * v / 255, 255 * v / 255), 62);
	}
	for (int layer = 0; layer < 2; ++layer) {
		const int speed = 150 + 130 * layer, span = PF_H + 160;
		const uint32_t body = rgb(mix(46, 16, layer, 2), mix(56, 18, layer, 2), mix(104, 44, layer, 2));
		const uint32_t rim = rgb(mix(110, 44, layer, 2), mix(124, 50, layer, 2), mix(172, 92, layer, 2));
		for (unsigned i = 0; i < 5; ++i) {
			const unsigned seed = 300u + unsigned(layer) * 50u + i;
			int y = (int(hash16(seed) * span >> 16) - tick * speed / 60) % span;
			if (y < 0) y += span;
			y -= 80;
			const int x = int(hash16(seed + 9) * (PF_W + 80) >> 16) - 40;
			for (unsigned b = 0; b < 5; ++b) {
				const int rad = 10 + int(12 * hash16(seed * 7 + b) >> 16) + 3 * layer;
				const int by = y + int(5 * hash16(seed * 13 + b) >> 16);
				disc(x + int(b) * 16, by, rad, body, 50 - layer * 4, 3);
				disc(x + int(b) * 16, by + 2, rad, rim, 51 - layer * 4, 3);
			}
		}
	}
	// Flint tournoie en derivant (PC : x 2, rotation t*250 deg/s).
	const int cx = PF_W / 2 + 26 * isinTick(tick, 1100) / 4096;
	const int cy = 96 + 8 * isinTick(tick, 2300) / 4096;
	for (int k = 0; k < 6; ++k)
		pfDrawRectSemi(cx - 14 + k * 11 / 2, cy - 64 - int(14 * hash16(unsigned(k) + unsigned(tick / 3)) >> 16), 1, 34, rgb(220, 230, 255), 40);
	heroDrawFallPS1(cx, cy, tick * 250 * 4096 / 360 / 60, 2, 30);
}

void zoneScene(int zone, int tick) {
	pfDrawRect(0, 0, PF_W, PF_H, rgb(4, 6, 14), 63);
	if (!g_zoneOk) return;
	// drawBackdrop(g, -1, -1, t, 46, 10) : case (w/2, 3h/4), derive sinusoidale.
	const int cellX = zoneW() / 2, cellY = zoneH() * 3 / 4;
	int camX = cellX * 24 - PF_W / 2 + 46 * isinTick(tick, 110) / 4096;
	int camY = (16 + cellY - zoneHeight(cellX, cellY)) * 16 - PF_H / 2 + 10 * isinTick(tick, 70, 652) / 4096;
	const int maxX = zoneW() * 24 - PF_W, maxY = (zoneH() + 16) * 16 - PF_H;
	camX = camX < 0 ? 0 : (camX > maxX ? maxX : camX);
	camY = camY < 0 ? 0 : (camY > maxY ? maxY : camY);
	zoneDraw(camX, camY, 0);
	(void)zone;
}

int cdZoneFor(int zone) { return zone == 42 ? 1042 : zone; }  // fond brut 042.a1m

}  // namespace

void introInitPS1(uint32_t padSeed) {
	const int n = kIntroCardCount < 32 ? kIntroCardCount : 32;
	for (int i = 0; i < n; ++i) {
		g_meta[i].scene = uint8_t(kIntroCards[i].scene);
		g_meta[i].len = uint16_t(kIntroCards[i].len);
	}
	if (!g_halfReady) initHalfWidths();
	introInit(&g_intro, g_meta, n);
	g_intro.prevPad = padSeed;  // anti-rebond entre le menu et l'intro
	g_loadedCard = -1;
	g_zoneOk = 0;
}

void introUpdatePS1(uint32_t pad) { introStep(&g_intro, pad); }

void introDrawPS1(void) {
	if (introDone(&g_intro)) return;
	const IntroCardData& c = kIntroCards[g_intro.card];
	const int t = g_intro.tick;
	// Fond de carte charge au debut de la carte (pendant le fondu d'entree).
	if (g_loadedCard != g_intro.card) {
		g_loadedCard = g_intro.card;
		if (c.scene == INTRO_SCENE_ZONE) {
			g_zoneOk = zoneLoadFromCdPS1(uint32_t(cdZoneFor(c.zone)));
			// La lecture CD arrete le flux musical (meme lecteur) : le PC garde le
			// theme du titre pendant tout le prologue, on le relance.
			audioStreamMusicPS1(1);
		}
	}
	switch (c.scene) {
		case INTRO_SCENE_ZONE: zoneScene(c.zone, t); break;
		case INTRO_SCENE_FALL: fallScene(t); break;
		case INTRO_SCENE_SHIP: {
			const int span = c.shipLast - c.shipFirst + 1;
			int prog = t * 1000 / 540;  // clamp01(t / 9 s) en millioemes
			if (prog > 1000) prog = 1000;
			nightSky(t + g_intro.card * 600, (g_intro.card - c.shipFirst) * 1000 + prog, span * 1000);
			break;
		}
		default: nightSky(t + g_intro.card * 600, 0, 0); break;
	}

	// Bandes de cinema (PC : 20 px en haut, 62 px en bas, degrade de 16 lignes).
	pfDrawRect(0, 0, PF_W, 20, 0x0000u, 8);
	pfDrawRect(0, PF_H - 62, PF_W, 62, 0x0000u, 8);
	for (int k = 0; k < 8; ++k) {
		pfDrawRectSemi(0, PF_H - 63 - k, PF_W, 1, 0x0000u, 8);
		if (k < 4) pfDrawRectSemi(0, PF_H - 63 - k, PF_W, 1, 0x0000u, 8);
		pfDrawRectSemi(0, 20 + k / 2, PF_W, 1, 0x0000u, 8);
	}

	// Texte : machine a ecrire, lignes centrees, nom turquoise au-dessus.
	const int shown = introShown(t, c.len);
	int y = PF_H - 54 + (c.nlines < 4 ? (4 - c.nlines) * 5 : 0);
	if (c.name[0] != '\0') {
		y = PF_H - 46;
		if (t > INTRO_FADE / 2) centeredShadow(c.name, PF_H - 59, PF_FONT_BANK_TEAL, 99, 2);
	}
	int left = shown;
	for (int i = 0; i < c.nlines; ++i) {
		const int len = length(c.lines[i]);
		centeredShadow(c.lines[i], y, PF_FONT_BANK_WHITE, left, 2);
		left -= (left < len + 1) ? left : len + 1;
		y += 11;
	}
	if (shown >= c.len && ((g_intro.frame / 24) % 2) != 0)
		a1window::windRect(PF_W - 22, PF_H - 20, 16, 16, 176, 56, 8, 1);  // fleche de suite
	const char* hint = "START : passer";
	int hx = 6;
	for (int i = 0; hint[i]; ++i) {
		const unsigned char ch = static_cast<unsigned char>(hint[i]);
		glyph(ch, hx, 6, PF_FONT_BANK_SKIP, 1);
		hx += kA1FontAdvance[ch];
	}

	// Fondus (PC : voile noir d'opacite 1 - tick/FADE, puis leaving/FADE).
	int veil = 0;
	if (t < INTRO_FADE) veil = 3 - t * 4 / INTRO_FADE;
	if (g_intro.leaving > 0) veil = 3 - g_intro.leaving * 4 / INTRO_FADE;
	for (int k = 0; k < veil; ++k) pfDrawRectSemi(0, 0, PF_W, PF_H, 0x0000u, 0);
	if (veil >= 3) pfDrawRect(0, 0, PF_W, PF_H, 0x0000u, 0);
}

int introDonePS1(void) { return introDone(&g_intro); }
int introSkippedPS1(void) { return g_intro.skipped; }
