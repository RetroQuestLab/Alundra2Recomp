/*
 * Dialogue A1 natif (PS1) : fenetre WIND.TX + police FONT3.TIM, narration A2.
 *
 * La composition reproduit a l'echelle 1:1 drawWindow()/drawLatin1() de
 * src/dialogue.cpp : parchemin repete entre deux bords dechires, rouleaux,
 * texte a x=32 sur 3 lignes au pas de 16 px, indicateur de page en banque 8.
 * Sous-ensemble C++ sans STL : tableaux POD, aucun destructeur global.
 */
#include "dialogue_ps1.h"

#include "../platform/pf.h"
#include "a1_font_ps1.h"
#include "dialogue_data_ps1.h"
#include "window_ps1.h"

namespace {

// Geometrie mesuree sur la capture A1 lp11_21 et verrouillee par les tests PC
// validate_dialogue_layout / validate_a1_textures.
constexpr int kBX = 16, kBY = 170, kBW = 286, kBH = 54;
// La composition WIND est partagee avec l'intro via window_ps1.h.
constexpr int kArrowX = 176, kArrowY = 56, kArrowSize = 16;
constexpr int kArrowBank = 8;
constexpr int kWindBank = 0;
constexpr int kTextX = 32, kTextY = kBY + 4, kLineH = 16;

// Priorites : plus petit = plus proche. La fenetre est derriere le texte.
constexpr int kZWindow = 3;
constexpr int kZArrow = 2;
constexpr int kZText = 1;
constexpr uint32_t kBlack = 0x0000u;

int g_panel = 0;
int g_shown = 0;
int g_prevStart = 0;
int g_active = 0;
int g_dialogueId = 0;
int g_finished = 0;

int validId(int id) {
	return id >= 0 && id < kDlgDialogueCount ? id : 0;
}

int panelChars(int id, int panel) {
	const char (*lines)[64] = kDlgTable[id].lines[panel];
	int total = 0;
	for (int i = 0; i < 3; ++i) {
		const char* line = lines[i];
		while (*line++) ++total;
	}
	return total;
}

void drawGlyph(unsigned char c, int x, int y) {
	const int idx = c >= 32 ? int(c) : int('?');
	const int col = idx & 15;
	const int row = idx >> 4;
	pfDrawSprite16(x, y, PF_FONT_X + col * 4, PF_FONT_Y + row * 16, 16, 16,
	               PF_FONT_CLUT_X, PF_FONT_CLUT_Y, kZText);
}

}  // namespace

void dlgInit(void) {
	pfUpload4bpp(PF_FONT_X, PF_FONT_Y, PF_FONT_W, PF_FONT_H, kA1FontAtlas);
	constexpr int kFontBanks = int(sizeof(kA1FontCluts) / sizeof(kA1FontCluts[0]));
	for (int bank = 0; bank < kFontBanks; ++bank) {
		pfUpload16(PF_FONT_CLUT_X, PF_FONT_CLUT_Y + bank, 16, 1, kA1FontCluts[bank]);
	}
	pfUpload4bpp(PF_WIND_X, PF_WIND_Y, PF_WIND_W, PF_WIND_H, kA1WindAtlas);
	for (int bank = 0; bank < 16; ++bank) {
		pfUpload16(PF_WIND_CLUT_X, PF_WIND_CLUT_Y + bank, 16, 1, kA1WindCluts[bank]);
	}
	g_panel = 0;
	g_shown = 0;
	g_prevStart = 1;  // exige un nouveau front START apres l'ouverture
	g_active = 0;
	g_dialogueId = 0;
	g_finished = 0;
}

void dlgOpen(int dialogueId) {
	g_dialogueId = validId(dialogueId);
	g_panel = 0;
	g_shown = 0;
	g_prevStart = 1;  // un START deja appuye ne doit pas avancer le panneau
	g_active = 1;
	g_finished = 0;
}

int dlgActive(void) { return g_active; }

int dlgId(void) { return g_active ? g_dialogueId : -1; }

int dlgConsumeFinished(void) {
	if (!g_finished) return 0;
	g_finished = 0;
	return 1;
}

void dlgClose(void) {
	g_active = 0;
	g_finished = 0;
	g_prevStart = 1;
}

void dlgUpdate(uint32_t pad) {
	const int start = (pad & PF_BTN_START) != 0 ? 1 : 0;
	if (!g_active) {
		g_prevStart = start;
		return;
	}

	const int total = panelChars(g_dialogueId, g_panel);
	// Deux glyphes par frame : proche de l'apparition A1 a 60 Hz tout en
	// restant observable dans une capture native sous interpreteur.
	if (g_shown < total) g_shown += 2;

	if (start && !g_prevStart) {
		if (g_shown < total) {
			g_shown = total;
		} else if (g_panel + 1 < kDlgTable[g_dialogueId].panels) {
			++g_panel;
			g_shown = 0;
		} else {
			// Fin du dialogue : on ne boucle plus, on rend la main a la quete.
			g_active = 0;
			g_finished = 1;
		}
	}
	g_prevStart = start;
}

void dlgDraw(void) {
	if (!g_active) return;
	a1window::draw(kBX, kBY, kBW, kBH, kWindBank, kZWindow);

	const char (*lines)[64] = kDlgTable[g_dialogueId].lines[g_panel];
	int remaining = g_shown;
	for (int line = 0; line < 3; ++line) {
		int x = kTextX;
		for (int i = 0; lines[line][i] != '\0' && remaining > 0; ++i) {
			const unsigned char c = static_cast<unsigned char>(lines[line][i]);
			const int idx = c >= 32 ? int(c) : int('?');
			drawGlyph(c, x, kTextY + line * kLineH);
			x += kA1FontAdvance[idx];
			--remaining;
		}
	}

	if (g_shown >= panelChars(g_dialogueId, g_panel) &&
	    g_panel + 1 < kDlgTable[g_dialogueId].panels) {
		a1window::windRect(kBX + kBW - 14, kBY + 31, kArrowSize, kArrowSize,
		                   kArrowX, kArrowY, kArrowBank, kZArrow);
	}
}

int dlgPanel(void) { return g_panel; }

void gameOverDrawPS1(int frame) {
	// Chaque voile semi-transparent approxime un palier du voile progressif du
	// PC (src/main.cpp:1280) sans nouvelle CLUT ni rectangle opaque invente.
	// Le 62 passe a pfDrawRectSemi est la priorite OT (z), pas l'alpha : la
	// translucidite vient de setSemiTrans() dans pf_ps1.cpp.
	int layers = frame / 8;
	if (layers > 3) layers = 3;
	for (int i = 0; i < layers; ++i)
		pfDrawRectSemi(0, 0, PF_W, PF_H, kBlack, 62);

	if (frame <= 20) return;
	a1window::draw(16, 92, 288, 54, kWindBank, kZWindow);

	// FONT3 est un atlas Latin-1 : \351 est le 'e' accentue, pas l'UTF-8 C3 A9
	// (deja observe en titre/menu, voir menu_ps1.cpp::drawMenu).
	const char* lines[2] = {"Flint est tomb\351...", "Valider : se relever"};
	for (int line = 0; line < 2; ++line) {
		if (line == 1 && frame <= 40) break;
		const char* text = lines[line];
		int width = 0;
		for (int i = 0; text[i] != '\0'; ++i) {
			const int idx = static_cast<unsigned char>(text[i]);
			width += kA1FontAdvance[idx];
		}
		int x = 160 - width / 2;
		const int y = line == 0 ? 96 : 112;
		for (int i = 0; text[i] != '\0'; ++i) {
			drawGlyph(static_cast<unsigned char>(text[i]), x, y);
			x += kA1FontAdvance[static_cast<unsigned char>(text[i])];
		}
	}
}

void endingDrawPS1(int frame) {
	(void)frame;
	pfDrawRect(0, 0, PF_W, PF_H, kBlack, 63);
	a1window::draw(16, 24, 288, 136, kWindBank, kZWindow);

	// Les deux panneaux de [alexia_finale_mephisto_vaincu] font exactement six
	// lignes A2 : aucune phrase inventee, aucune reduction de police.
	const char (*lines)[3][64] = kDlgLines_alexia_finale_mephisto_vaincu;
	for (int p = 0; p < 2; ++p) {
		for (int line = 0; line < 3; ++line) {
			const char* text = lines[p][line];
			int x = 32;
			const int y = 36 + (p * 3 + line) * 16;
			for (int i = 0; text[i] != '\0' && i < 64; ++i) {
				const unsigned char c = static_cast<unsigned char>(text[i]);
				const int idx = c >= 32 ? int(c) : int('?');
				drawGlyph(c, x, y);
				x += kA1FontAdvance[idx];
			}
		}
	}
}
