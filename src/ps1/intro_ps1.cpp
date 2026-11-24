/*
 * Introduction PS1 native : visuel Alundra 1, narration Alundra 2.
 * Panneaux generes depuis data/dialogues.txt [prologue] (voir ps1_export_intro.py).
 * Aucun STL, aucun SDK direct, aucune ressource chargee a l'execution.
 */
#include "intro_ps1.h"

#include "../platform/pf.h"
#include "a1_font_ps1.h"
#include "intro_data_ps1.h"
#include "intro_ps1_logic.h"
#include "window_ps1.h"

namespace {

IntroState g_intro;

// Couleur 15 bits GPU PS1 : rouge en bits 0..4, vert 5..9, bleu 10..14.
// pfDrawRect() decode cet ordre ; l'ancien helper inversait rouge et bleu.
inline uint32_t bgr(int r, int g, int b) {
	const int rr = r < 0 ? 0 : (r > 31 ? 31 : r);
	const int gg = g < 0 ? 0 : (g > 31 ? 31 : g);
	const int bb = b < 0 ? 0 : (b > 31 ? 31 : b);
	return uint32_t(rr) | (uint32_t(gg) << 5) | (uint32_t(bb) << 10);
}
uint32_t hash01(unsigned n) {
	n = (n ^ 61u) ^ (n >> 16);
	n *= 9u;
	n ^= n >> 4;
	n *= 0x27d4eb2du;
	n ^= n >> 15;
	return n & 0xffffu;
}

// sin(2*pi*frame/period) approximé en entier, sans FPU (R3000A).
int intSin(uint32_t frame, uint32_t period) {
	const uint32_t phase = (frame * 256u / period) & 255u;
	if (phase < 64u) return int(phase) / 16;
	if (phase < 128u) return (64 - int(phase)) / 16;
	if (phase < 192u) return -int(phase - 128) / 16;
	return -(64 - int(phase - 192)) / 16;
}

void disc(int cx, int cy, int rad, uint32_t col, int z) {
	for (int dy = -rad; dy <= rad; ++dy) {
		int w = 0;
		for (int x = 0; x <= rad; ++x) {
			if (x * x + dy * dy > rad * rad) break;
			w = x;
		}
		pfDrawRect(cx - w, cy + dy, 2 * w + 1, 1, col, z);
	}
}

int glyphIndex(unsigned char c) { return c >= 32 ? int(c) : int('?'); }

void text(const char* s, int x, int y, int bank, int z) {
	for (int i = 0; s[i] != '\0'; ++i) {
		const unsigned char c = static_cast<unsigned char>(s[i]);
		const int idx = glyphIndex(c);
		pfDrawSprite16(x, y, PF_FONT_X + (idx & 15) * 4, PF_FONT_Y + (idx >> 4) * 16,
		               16, 16, PF_FONT_CLUT_X, PF_FONT_CLUT_Y + bank, z);
		x += kA1FontAdvance[idx];
	}
}

void drawPanel(int panel) {
	// Fenetre WIND partagee avec le dialogue : la source des rouleaux est
	// identique, sinon des rows etrangeres recouvrent les glyphes.
	a1window::draw(16, 170, 286, 54, 0, 4);
	if (panel < 0 || panel >= kIntroPanelCount) return;
	for (int line = 0; line < 3; ++line)
		text(kIntroLines[panel][line], 32, 174 + line * 16, 0, 2);
}

// --- Decors -----------------------------------------------------------------------
void skyBase(int frame, uint32_t top, uint32_t bottom) {
	const int tr = int(top & 31u), tg = int((top >> 5) & 31u), tb = int((top >> 10) & 31u);
	const int br = int(bottom & 31u), bg = int((bottom >> 5) & 31u), bb = int((bottom >> 10) & 31u);
	for (int y = 0; y < PF_H; y += 4) {
		const int t = y * 32 / PF_H;
		pfDrawRect(0, y, PF_W, 4,
		           bgr(tr + (br - tr) * t / 32u,
		               tg + (bg - tg) * t / 32u,
		               tb + (bb - tb) * t / 32u), 63);
	}
	for (unsigned i = 0; i < 50; ++i) {
		const int x = int(hash01(i * 3 + 1) * PF_W) % PF_W;
		const int y = int(hash01(i * 3 + 2) * 150u);
		pfDrawRect(x, y, ((frame / 20 + int(i)) % 5 == 0) ? 2 : 1, 1, bgr(20, 22, 28), 62);
	}
}

void pirateShip(int x, int y, uint32_t frame) {
	const uint32_t hull = bgr(6, 7, 18);
	y += int((frame / 18u) % 3u) - 1;
	for (int j = 0; j < 9; ++j) pfDrawRect(x - 30 + j, y + j, 60 - 2 * j, 1, hull, 30);
	pfDrawRect(x - 10, y - 34, 2, 34, hull, 30);
	pfDrawRect(x + 14, y - 28, 2, 28, hull, 30);
	for (int j = 0; j < 12; ++j) {
		pfDrawRect(x - 20 - j / 2, y - 30 + j, 20 + j, 1, hull, 30);
		pfDrawRect(x + 16, y - 24 + j, 17 + j / 2, 1, hull, 30);
	}
	pfDrawRect(x - 30, y - 4, 2, 2, bgr(30, 22, 6), 29);
}

void drawSky(uint32_t frame) {
	skyBase(frame, bgr(2, 3, 10), bgr(12, 16, 30));
	disc(236, 56, 20, bgr(28, 27, 24), 60);
	disc(230, 50, 4, bgr(22, 21, 19), 59);
	for (int i = 0; i < 3; ++i) {
		const int cx = ((i * 130 + 40) - int(frame / 2)) % 460 - 60;
		disc(cx, 130 + i * 22, 12, bgr(10, 12, 30), 50);
		disc(cx + 20, 132 + i * 22, 14, bgr(10, 12, 30), 50);
	}
}

void drawShip(uint32_t frame) {
	drawSky(frame);
	const int x = 24 + int((frame / 4u) % 280u);
	pirateShip(x, 100, frame);
	// Silhouette de Flint sur le pont, face au navire.
	const uint32_t dark = bgr(3, 4, 12);
	pfDrawRect(272, 132, 6, 8, dark, 28);
	pfDrawRect(273, 140, 4, 12, dark, 28);
	pfDrawRect(270, 128, 10, 4, dark, 28);
}

void drawFall(uint32_t frame) {
	skyBase(frame, bgr(2, 3, 8), bgr(8, 12, 28));
	for (unsigned i = 0; i < 45; ++i) {
		const int x = int(hash01(i * 5 + 1) * PF_W) % PF_W;
		int y = (int(hash01(i * 5 + 2) * 300u) - int(frame * 3)) % 300;
		if (y < 0) y += 300;
		pfDrawRect(x, y - 30, 1, 8 + int(hash01(i + 7) * 10u), bgr(18, 20, 30), 55);
	}
	// Flint tournoie en tombant : silhouette alternee, derive laterale.
	const int cx = 160 + 26 * intSin(frame, 60);
	const int cy = 90 + 8 * intSin(frame, 37);
	const uint32_t hero = bgr(10, 12, 28);
	pfDrawRect(cx - 6, cy - 10, 12, 10, hero, 25);
	pfDrawRect(cx - 4, cy, 8, 14, hero, 25);
	pfDrawRect(cx - 10, cy + 2, 4, 10, hero, 25);
	pfDrawRect(cx + 6, cy + 2, 4, 10, hero, 25);
}

void drawBeach(uint32_t frame) {
	skyBase(frame, bgr(12, 14, 30), bgr(30, 20, 14));
	const uint32_t sand = bgr(22, 18, 10), sea = bgr(8, 14, 26);
	pfDrawRect(0, 150, PF_W, 20, sea, 50);
	pfDrawRect(0, 170, PF_W, 70, sand, 50);
	for (int i = 0; i < 8; ++i) {
		const int x = (i * 47 + int(frame / 6)) % (PF_W + 40) - 20;
		pfDrawRect(x, 166, 22, 2, bgr(18, 20, 30), 49);
	}
	const uint32_t hero = bgr(14, 10, 6);
	pfDrawRect(70, 140, 10, 10, hero, 20);
	pfDrawRect(72, 150, 6, 16, hero, 20);
	pfDrawRect(66, 142, 4, 12, hero, 20);
	pfDrawRect(80, 142, 4, 12, hero, 20);
}

}  // namespace

void introInitPS1(uint32_t padSeed) {
	introInit(&g_intro);
	g_intro.prevPad = padSeed;  // anti-rebond entre le menu et l'intro
}

void introUpdatePS1(uint32_t pad) { introStep(&g_intro, pad); }

void introDrawPS1(void) {
	const uint32_t frame = uint32_t(g_intro.frame);
	// Bandes de cinema : l'action reste lisible, le texte A1 en bas.
	pfDrawRect(0, 0, PF_W, 14, 0x0000u, 6);
	pfDrawRect(0, 226, PF_W, 14, 0x0000u, 6);

	switch (g_intro.scene) {
		case INTRO_SCENE_SKY:   drawSky(frame); break;
		case INTRO_SCENE_SHIP:  drawShip(frame); break;
		case INTRO_SCENE_FALL:  drawFall(frame); break;
		case INTRO_SCENE_BEACH: drawBeach(frame); break;
		case INTRO_SCENE_DONE:
		default: break;
	}

	const int panel = (frame * kIntroPanelCount) / INTRO_TOTAL_FRAMES;
	drawPanel(panel);

	// Invite PC identique (src/title.cpp::drawPrologue), en bas de l'ecran.
	text("C : passer", 6, 6, 1, 1);
	// Aucun numero de version affiche cote PC : supprime pour la parite.
}

int introDonePS1(void) { return introDone(&g_intro); }
int introSkippedPS1(void) { return g_intro.skipped; }
