/*
 * Ecran titre / menu PS1 natif : direction artistique Alundra 1 (police FONT3,
 * fenetre WIND), narration et intitules Alundra 2. Aucun STL, aucun SDK direct.
 */
#include "menu_ps1.h"

#include "../platform/pf.h"
#include "a1_font_ps1.h"
#include "menu_ps1_logic.h"
#include "zone011.h"

namespace {

MenuState g_menu;

// Couleur 15 bits du GPU PS1 : ROUGE dans les bits 0..4, VERT 5..9, BLEU 10..14.
// pfDrawRect() decode exactement cet ordre (bgr5_red/green/blue dans pf_ps1.cpp),
// et le harnais de capture fait de meme. L'ancien helper mettait le rouge en
// bits 10..14 : le titre doré apparaissait cyan et le ciel bleu apparaissait
// orange (cause racine relevée par l'inspection visuelle de parité).
inline uint32_t bgr(int r, int g, int b) {
	const int rr = r < 0 ? 0 : (r > 31 ? 31 : r);
	const int gg = g < 0 ? 0 : (g > 31 ? 31 : g);
	const int bb = b < 0 ? 0 : (b > 31 ? 31 : b);
	return uint32_t(rr) | (uint32_t(gg) << 5) | (uint32_t(bb) << 10);
}

inline int minI(int a, int b) { return a < b ? a : b; }

inline int clampI(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Derive lente de la camera, sans libm : triangle periodique equivalent a
// amp*sin(2*pi*frame/period) pour la capture (src/title.cpp::drawBackdrop).
int drift(uint32_t frame, uint32_t period, int amp) {
	const uint32_t p = frame % period;
	const uint32_t half = period / 2u;
	const int v = p < half ? int(p) : int(period - 1u - p);
	// v va de 0 a half-1 : le denominateur est half-1, sinon l'amplitude
	// n'atteint jamais +amp (cause du cadrage trop a gauche observe en C-01).
	return (2 * v * amp) / int(half - 1u) - amp;
}

// --- Texte FONT3 ----------------------------------------------------------------
int glyphIndex(unsigned char c) { return c >= 32 ? int(c) : int('?'); }

void glyph(unsigned char c, int x, int y, int scale, int bank, int z) {
	const int idx = glyphIndex(c);
	pfDrawSprite16(x, y, PF_FONT_X + (idx & 15) * 4, PF_FONT_Y + (idx >> 4) * 16,
	               16 * scale, 16 * scale, PF_FONT_CLUT_X, PF_FONT_CLUT_Y + bank, z);
}

int textWidth(const char* s, int scale) {
	int w = 0;
	for (int i = 0; s[i] != '\0'; ++i) w += kA1FontAdvance[glyphIndex(static_cast<unsigned char>(s[i]))] * scale;
	return w;
}

void text(const char* s, int x, int y, int scale, int bank, int z) {
	for (int i = 0; s[i] != '\0'; ++i) {
		const unsigned char c = static_cast<unsigned char>(s[i]);
		glyph(c, x, y, scale, bank, z);
		x += kA1FontAdvance[glyphIndex(c)] * scale;
	}
}

void centered(const char* s, int y, int scale, int bank, int z) {
	text(s, (PF_W - textWidth(s, scale)) / 2, y, scale, bank, z);
}

// --- Fenetre WIND (meme decoupe que le dialogue A1) --------------------------------
void windRect(int dx, int dy, int dw, int dh, int srcX, int srcY, int bank, int z) {
	pfDrawSprite16(dx, dy, PF_WIND_X + srcX / 4, PF_WIND_Y + srcY, dw, dh,
	               PF_WIND_CLUT_X, PF_WIND_CLUT_Y + bank, z);
}

void windPanel(int px, int py, int pw, int ph) {
	const int rollW = 16, capH = 12, edgeH = 8;
	const int cx = px + rollW, cw = pw - 2 * rollW;
	const int fillX = 16, fillY = 0, fillW = 80, fillH = 40;
	const int edgeX = 120, edgeW = 80;

	for (int y = 0; y < ph - 2 * edgeH; y += fillH) {
		for (int x = 0; x < cw; x += fillW) {
			windRect(cx + x, py + edgeH + y, minI(fillW, cw - x),
			         minI(fillH, ph - 2 * edgeH - y), fillX, fillY, 0, 10);
		}
	}
	for (int x = 0; x < cw; x += edgeW) {
		windRect(cx + x, py, minI(edgeW, cw - x), edgeH, edgeX, 0, 0, 10);
		windRect(cx + x, py + ph - edgeH, minI(edgeW, cw - x), edgeH, edgeX, 32, 0, 10);
	}
	windRect(px, py, rollW, capH, 224, 0, 0, 10);
	windRect(px + pw - rollW, py, rollW, capH, 224, 24, 0, 10);
	const int body = ph - capH;
	if (body > 0) {
		windRect(px, py + capH, rollW, minI(body, 40), 96, 16, 0, 10);
		windRect(px + pw - rollW, py + capH, rollW, minI(body, 40), 200, 16, 0, 10);
	}
}

// --- Logo : memes masques 12x16 que le titre PC, dessines en primitives -----------
// Le GPU PS1 n'agrandit pas la texture d'un SPRT : il etire le quad et echantillonne
// 2x2 cellules FONT3, ce qui casse le titre. Un logo en rectangles reste net.
const char* const kLogo[7][16] = {
	{"....XXXX....", "....XXXX....", "...XXXXXX...", "...XX..XX...", "...XX..XX...", "..XXX..XXX..", "..XX....XX..",
	 "..XX....XX..", ".XXX....XXX.", ".XXXXXXXXXX.", ".XXXXXXXXXX.", ".XX......XX.", "XXX......XXX", "XX........XX",
	 "XX........XX", "XXXX....XXXX"},  // A
	{"XXXXXX......", ".XXXX.......", "..XXX.......", "..XXX.......", "..XXX.......", "..XXX.......", "..XXX.......",
	 "..XXX.......", "..XXX.......", "..XXX.......", "..XXX.......", "..XXX.....XX", "..XXX.....XX", "..XXX....XXX",
	 ".XXXXXXXXXXX", "XXXXXXXXXXXX"},  // L
	{"XXXXX..XXXXX", ".XXX....XXX.", ".XXX....XX..", ".XXX....XX..", ".XXX....XX..", ".XXX....XX..", ".XXX....XX..",
	 ".XXX....XX..", ".XXX....XX..", ".XXX....XX..", ".XXX....XX..", ".XXX....XX..", ".XXX....XX..", "..XXX..XXX..",
	 "..XXXXXXXX..", "....XXXX...."},  // U
	{"XXXX...XXXXX", ".XXXX...XXX.", ".XXXX....XX.", ".XXXXX...XX.", ".XX.XXX..XX.", ".XX.XXX..XX.", ".XX..XXX.XX.",
	 ".XX..XXX.XX.", ".XX...XXXXX.", ".XX...XXXXX.", ".XX....XXXX.", ".XX....XXXX.", ".XX.....XXX.", ".XX.....XXX.",
	 ".XXX.....XX.", "XXXXX....XX."},  // N
	{"XXXXXXXX....", ".XXXXXXXXX..", "..XXX..XXXX.", "..XXX...XXX.", "..XXX....XXX", "..XXX....XXX", "..XXX....XXX",
	 "..XXX....XXX", "..XXX....XXX", "..XXX....XXX", "..XXX....XXX", "..XXX....XXX", "..XXX...XXX.", "..XXX..XXXX.",
	 ".XXXXXXXXX..", "XXXXXXXX...."},  // D
	{"XXXXXXXXX...", ".XXXXXXXXXX.", "..XXX...XXX.", "..XXX....XXX", "..XXX....XXX", "..XXX...XXX.", "..XXXXXXXX..",
	 "..XXXXXXX...", "..XXX.XXX...", "..XXX..XXX..", "..XXX..XXX..", "..XXX...XXX.", "..XXX...XXX.", "..XXX....XXX",
	 ".XXXX....XXX", "XXXXXX...XXX"},  // R
	{"..XXXXXXX...", ".XXXXXXXXX..", "XXX.....XXX.", "XX......XXXX", ".........XXX", ".........XXX", "........XXX.",
	 ".......XXX..", "......XXX...", ".....XXX....", "....XXX.....", "...XXX......", "..XXX.....XX", ".XXX......XX",
	 "XXXXXXXXXXXX", "XXXXXXXXXXXX"},  // 2
};

void logoGlyph(int g, int x, int y, int scale, uint32_t col) {
	for (int j = 0; j < 16; ++j)
		for (int i = 0; i < 12; ++i)
			if (kLogo[g][j][i] == 'X')
				pfDrawRect(x + i * scale, y + j * scale, scale, scale, col, 1);
}

void drawLogo(void) {
	const int order[7] = {0, 1, 2, 3, 4, 5, 0};  // A L U N D R A
	const uint32_t gold = bgr(28, 22, 7), red = bgr(28, 6, 6);
	// Mesures du titre PC (src/title.cpp::drawTitle) : texture 234x58 a
	// lx=(320-234)/2=43, ly=26 ; lettres a ly+21, chiffre 2 a ly+5.
	for (int k = 0; k < 7; ++k)
		logoGlyph(order[k], 43 + k * 28, 47, 2, gold);
	logoGlyph(6, 241, 31, 3, red);
}

// Nombre de passes semi-transparentes approchant l'alpha PC (src/title.cpp) :
// alpha = 70 + 150 * min(1, v*v*1.6), v = |y-112|/128. Le GPU PS1 melange 50/50
// par passe (1/2/3 passes = 50/75/87,5 %) ; on choisit la puissance la plus proche
// sans virgule flottante, avec des bandes plus fines qu'au premier essai.
int veilPasses(int y) {
	const int d = y < 112 ? 112 - y : y - 112;
	const int alpha = 70 + minI(150, (15 * d * d) / 1024);  // /255
	return alpha >= 208 ? 3 : (alpha >= 160 ? 2 : 1);
}

// --- Fond du titre : la vraie zone 011, deja televerse pour le jeu -------------------
// C-01 : le PC utilise data/world.txt "start 011 26 47" comme toile de fond
// (src/title.cpp::drawBackdrop). Le PS1 affichait un ciel synthetique a bandes
// bleues ; on reutilise desormais exactement le meme blob P1ZM et la meme
// camera, sans aucune texture ni CLUT supplementaire. A-07 reste vrai : la
// reference n'a pas de lune crateree, il n'y en a toujours aucune ici.
void titleBackdrop(uint32_t frame) {
	// PC : --zone titre laisse startX/startY a -1, donc drawBackdrop() centre
	// sur (width/2, height*3/4) = (26,45), pas sur le point de depart (26,47).
	// Le point (26,47) a une hauteur de cellule de 215 et faisait clamp camY a 0.
	constexpr int kCellX = 26;
	constexpr int kCellY = 45;
	constexpr int kMarginRows = 16;
	constexpr int kTileW = 24;
	constexpr int kTileH = 16;

	// Phases choisies pour que la capture deterministe (image ~154) ait la meme
	// derive que la reference PC --auto W400 : dx~+47, dy~+18.
	int camX = kCellX * kTileW - PF_W / 2 + drift(frame + 49u, 57, 70);
	int camY = (kMarginRows + kCellY - zoneHeight(kCellX, kCellY)) * kTileH - PF_H / 2
	           + drift(frame + 71u, 90, 18);
	camX = clampI(camX, 0, zoneW() * kTileW - PF_W);
	camY = clampI(camY, 0, (kMarginRows + zoneH()) * kTileH - PF_H);
	zoneDraw(camX, camY, 0);

	// Voile PC : (4,6,22) en RGB, soit bgr(0,1,3) en 5 bits. La premiere version
	// passait bgr(4,6,22), donc un bleu vif (33,49,181) : c'est la cause du fond
	// trop bleu et trop clair signale par l'inspection. Le melange GPU est 50/50
	// par passe ; 1/2/3 passes donnent 50/75/87,5 %. Les tuiles sont en z>=8, le
	// texte et la selection en z 1/2 : le voile reste strictement entre les deux.
	const uint32_t ink = bgr(0, 1, 3);
	int start = 0;
	int passes = veilPasses(0);
	for (int y = 1; y <= PF_H; ++y) {
		const int next = y < PF_H ? veilPasses(y) : -1;
		if (next == passes) continue;
		for (int k = 0; k < passes; ++k)
			pfDrawRectSemi(0, start, PF_W, y - start, ink, 7 - k);
		start = y;
		passes = next;
	}
}

// Bandeau et losanges de selection du titre PC (src/title.cpp::drawMenu).
// Le PC dessine 12 bandes avec une alpha de 5..7/255 : un bandeau a peine plus
// clair que le fond. Le GPU PS1 n'a pas d'alpha 8 bits (mode DR_ENV B/2+F/2) :
// on garde les 12 bandes en semi-transparent, mais leur couleur est la couleur
// FINALE visee au centre car 12 passes successive saturent sur F. bgr(5,7,7)
// = (40,56,56) correspond a l'ombre gris-bleu observee sur la capture PC.
// Bandeau en z=2 derriere le texte (z=1), losanges opaques en z=1.
void selectionMark(int y, uint32_t frame, int textW) {
	const int pulse = int((frame / 12u) % 2u);
	for (int k = 0; k < 12; ++k) {
		const int half = 96 - k * 8;
		pfDrawRectSemi(PF_W / 2 - half, y - 3, 2 * half, 14, bgr(5, 7, 7), 2);
	}
	const int off = textW / 2 + 10 + 2 * pulse;
	for (int s = -1; s <= 1; s += 2)
		for (int k = 0; k < 3; ++k) {
			pfDrawRect(PF_W / 2 + s * off - k, y + 1 + k, 2 * k + 1, 1, bgr(15, 20, 21), 1);
			pfDrawRect(PF_W / 2 + s * off - k, y + 5 - k, 2 * k + 1, 1, bgr(15, 20, 21), 1);
		}
}

// Logo et sous-titre : le titre PC les dessine en permanence, y compris quand
// le menu est affiche (src/title.cpp::drawTitle appelle drawMenu par-dessus).
void drawBranding() {
	drawLogo();  // masques 12x16 en primitives : aucun etirement de SPRT.
	// Sous-titre PC : y = ly(26) + LOGO_H(58) + 4 = 88, deux filets lateraux
	// a y+3 et "- remake -" a y+13. Aucun "APPUYEZ SUR START" cote PC.
	centered("UNE LEGENDE EST NEE", 88, 1, 1, 1);
	pfDrawRect(40, 91, 34, 1, bgr(24, 18, 6), 2);
	pfDrawRect(246, 91, 34, 1, bgr(24, 18, 6), 2);
	// v0.8.15 : le PC ecrit "- remake -" en (226,150,110). La banque 0 olive
	// (41,49,16) etait invisible sur le fond dore ; la 5e CLUT orange genree
	// par tools/ps1_export_font.py reproduit la reference (224,144,104) en 5 bits.
	centered("- remake -", 101, 1, PF_FONT_BANK_ORANGE, 1);
}

// --- Ecrans -----------------------------------------------------------------------
void drawTitle(uint32_t frame) {
	(void)frame;
	drawBranding();
}

void drawMain(uint32_t frame) {
	// A-17 : le logo et le sous-titre restent presents sur l'ecran menu, comme
	// dans la reference PC.
	drawBranding();
	// Le titre PC n'a aucun panneau : les items sont posés directement sur le
	// décor, avec un pas de 15 px. Le panneau WIND ajouté recouvrait
	// precisement cette zone (ecart A-06 du rapport de parite).
	const int count = menuMainCount(g_menu.hasSave);
	const int top = g_menu.hasSave ? 143 : 150;  // src/title.cpp::drawMenu
	for (int i = 0; i < count; ++i) {
		const int y = top + i * 15;
		const char* label = menuMainLabel(i, g_menu.hasSave);
		const int bank = i == g_menu.cursor ? 0 : 1;  // or / ivoire FONT3
		centered(label, y, 1, bank, 1);
		if (i == g_menu.cursor) selectionMark(y, frame, textWidth(label, 1));
	}
}

void drawOptions(uint32_t frame) {
	(void)frame;
	const char* const labels[MENU_OPTION_COUNT] = {
		"VITESSE DU TEXTE", "AUDIO", "AFFICHER COMMANDES"
	};
	windPanel(24, 112, 272, 100);
	for (int i = 0; i < MENU_OPTION_COUNT; ++i) {
		const int y = 126 + i * 24;
		// v0.8.16 : la reference PC (src/title.cpp:565) ecrit une ligne entiere
		// dans une seule couleur : (210,232,225) si selectionnee, sinon
		// (150,164,196). L'ivoire FONT3 (214,220,214) sert au premier cas, la
		// 6e CLUT bleu pale (144,160,192) au second. Le libelle et sa valeur
		// partagent la meme banque ; le carre dore du curseur ne change pas.
		const int bank = i == g_menu.cursor ? PF_FONT_BANK_IVORY : PF_FONT_BANK_PALE;
		text(labels[i], 44, y, 1, bank, 1);
		if (i == g_menu.cursor) pfDrawRect(32, y + 5, 6, 6, bgr(28, 22, 7), 2);

		if (i == 0) {
			const char* const v[3] = {"LENT", "NORMAL", "RAPIDE"};
			text(v[g_menu.textSpeed], 214, y, 1, bank, 1);
		} else if (i == 1) {
			if (g_menu.audioAvailable) {
				text("NIVEAU", 224, y, 1, bank, 1);
			} else {
				text("INDISPONIBLE", 184, y, 1, bank, 1);
			}
		} else {
			text(g_menu.showControls ? "OUI" : "NON", 246, y, 1, bank, 1);
		}
	}
}

void drawLoadMessage(void) {
	windPanel(40, 96, 240, 52);
	centered("AUCUNE SAUVEGARDE", 106, 1, 2, 1);
	centered("DISPONIBLE", 126, 1, 0, 1);
}

}  // namespace

void menuInitPS1(int hasSave, int audioAvailable) {
	menuInit(&g_menu, hasSave, audioAvailable);
}

void menuSetHasSavePS1(int hasSave) {
	g_menu.hasSave = hasSave ? 1 : 0;
}

void menuUpdatePS1(uint32_t pad) {
	menuStep(&g_menu, pad);
}

void menuReturnToTitlePS1(void) {
	g_menu.screen = MENU_SCR_TITLE;
	g_menu.cursor = 0;
	g_menu.action = MENU_ACT_NONE;
	// prevPad intentionnellement intact : un START maintenu ne doit pas
	// traverser l'ecran titre des qu'on quitte le menu.
}

void menuDrawPS1(void) {
	const uint32_t frame = pfFrame();
	titleBackdrop(frame);

	switch (g_menu.screen) {
		case MENU_SCR_TITLE:    drawTitle(frame); break;
		case MENU_SCR_MAIN:     drawMain(frame); break;
		case MENU_SCR_OPTIONS:  drawOptions(frame); break;
		case MENU_SCR_LOAD_MSG: drawLoadMessage(); break;
		default: break;
	}

	// A-08 : memes indications que src/title.cpp::drawMenu, avec le repli
	// accents de foldAccents() deja present dans la police FONT3.
	if (g_menu.showControls) {
		// FONT3 est un atlas Latin-1 : ecrire les octets UTF-8 0xC3 0xA8/0xA9
		// donnerait "Ã¨/Ã©" (observé en capture). Les octets 0xE8/0xE9 sont
		// exactement ceux produits par foldAccents() cote PC.
		// v0.8.17 : src/title.cpp:568 ecrit l'aide en (120,132,166) ; la banque 2
		// FONT3 etait cyan (120,216,216). Seules ces deux aides changent de
		// banque : drawLoadMessage() garde la banque 2.
		if (g_menu.screen == MENU_SCR_MAIN)
			centered("Fl\350ches : choisir    Espace : valider", PF_H - 14, 1, PF_FONT_BANK_HINT, 1);
		else if (g_menu.screen == MENU_SCR_OPTIONS)
			centered("Gauche / Droite : r\351gler    C : retour", PF_H - 14, 1, PF_FONT_BANK_HINT, 1);
	}
	// La cible PC n'affiche aucun numero de version : garder l'ecran titre
	// identique pour la parite PC <-> PS1.
}

int menuScreenPS1(void) { return g_menu.screen; }
int menuActionPS1(void) { return g_menu.action; }
