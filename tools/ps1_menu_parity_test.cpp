/*
 * Test hote de parite du titre/menu PS1 (TDD).
 *
 * Il compile le VRAI src/ps1/menu_ps1.cpp avec un backend pf minimal qui
 * enregistre les primitives. Aucune image n'est lue : on verifie le texte
 * effectivement emis par la police FONT3, la presence du logo et l'absence
 * de la lune ajoutee, dans les deux etats de sauvegarde.
 *
 *   external\llvm-mingw-20261020-ucrt-x86_64\bin\clang++.exe -std=c++20 ^
 *       -o ps1\build\menu_parity_test.exe tools\ps1_menu_parity_test.cpp
 *   .\ps1\build\menu_parity_test.exe
 */
#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {
struct Rect {
	int x, y, w, h;
	uint32_t color, z;
};
struct Sprite {
	int x, y, u, v, w, h, clutX, clutY, z;
};

std::vector<Rect> g_rects;
std::vector<Sprite> g_sprites;
uint32_t g_frame = 1;
int g_zoneDrawCalls = 0;
int g_zoneCamX = 0;
int g_zoneCamY = 0;

uint32_t parityColor(int r, int g, int b) {
	return uint32_t(r) | (uint32_t(g) << 5) | (uint32_t(b) << 10);
}

int failures = 0;
int checks = 0;

#define CHECK(cond)                                                       \
	do {                                                                    \
		++checks;                                                             \
		if (!(cond)) {                                                        \
			std::printf("FAIL ligne %d : %s\n", __LINE__, #cond);               \
			++failures;                                                       \
		}                                                                     \
	} while (0)
}  // namespace

extern "C" void pfDrawRect(int x, int y, int w, int h, uint32_t color, int z) {
	g_rects.push_back({x, y, w, h, color, uint32_t(z)});
}

extern "C" void pfDrawRectSemi(int x, int y, int w, int h, uint32_t color, int z) {
	g_rects.push_back({x, y, w, h, color | 0x80000000u, uint32_t(z)});
}

// Le fond du titre utilise la vraie zone 011 (deja embarquee) : stubs POD
// pour que le test hote compile menu_ps1.cpp sans editer de VRAM.
int zoneW(void) { return 100; }
int zoneH(void) { return 80; }
int zoneHeight(int cx, int cy) { return cx == 26 && cy == 45 ? 12 : 2; }
void zoneDraw(int camX, int camY, int) {
	++g_zoneDrawCalls;
	g_zoneCamX = camX;
	g_zoneCamY = camY;
}

extern "C" void pfDrawSprite16(int x, int y, int u, int v, int w, int h, int clutX, int clutY, int z) {
	g_sprites.push_back({x, y, u, v, w, h, clutX, clutY, z});
}

extern "C" uint32_t pfFrame(void) { return g_frame; }

#include "../src/ps1/menu_ps1.cpp"

namespace {

// Reconstruit les lignes de texte FONT3 a partir des SPRT emis par text().
std::vector<std::string> renderedLines() {
	std::map<int, std::vector<std::pair<int, char>>> byY;
	for (const Sprite& s : g_sprites) {
		if (s.w != 16 || s.h != 16 || s.clutX != PF_FONT_CLUT_X) continue;
		if (s.u < PF_FONT_X || s.u >= PF_FONT_X + PF_FONT_W * 2) continue;
		if (s.v < PF_FONT_Y || s.v >= PF_FONT_Y + 256) continue;
		const int idx = (s.u - PF_FONT_X) / 4 + ((s.v - PF_FONT_Y) / 16) * 16;
		byY[s.y].push_back({s.x, idx >= 32 ? char(idx) : '?'});
	}
	std::vector<std::string> lines;
	for (auto& entry : byY) {
		std::sort(entry.second.begin(), entry.second.end());
		std::string line;
		for (const auto& cell : entry.second) line += cell.second;
		lines.push_back(line);
	}
	return lines;
}

bool hasLine(const std::vector<std::string>& lines, const char* expected) {
	return std::find(lines.begin(), lines.end(), std::string(expected)) != lines.end();
}

// Les options dessinent le libelle et la valeur sur le meme y : renderedLines()
// les concatenate sans espace. On cherche donc un fragment, pas une ligne entiere.
bool hasLineContaining(const std::vector<std::string>& lines, const char* expected) {
	const std::string needle(expected);
	for (const std::string& line : lines)
		if (line.find(needle) != std::string::npos) return true;
	return false;
}

int countColor(uint32_t color) {
	int n = 0;
	for (const Rect& r : g_rects)
		if (r.color == color) ++n;
	return n;
}

void resetDraw() {
	g_rects.clear();
	g_sprites.clear();
	g_zoneDrawCalls = 0;
}

int countSemiVeil() {
	int n = 0;
	for (const Rect& r : g_rects)
		if ((r.color & 0x80000000u) && r.z >= 5 && r.z <= 7) ++n;
	return n;
}

void drawScreen(int hasSave, int screen) {
	menuInitPS1(hasSave, 0);
	if (screen != MENU_SCR_TITLE) {
		menuUpdatePS1(PF_BTN_START);
		menuUpdatePS1(0);
	}
	resetDraw();
	menuDrawPS1();
}

void checkMainNoSave() {
	drawScreen(0, MENU_SCR_MAIN);
	const auto lines = renderedLines();
	CHECK(hasLine(lines, "Nouvelle partie"));
	CHECK(hasLine(lines, "Options"));
	CHECK(hasLine(lines, "Quitter"));
	CHECK(hasLine(lines, "Haut / Bas : choisir    Croix : valider"));  // touches manette (v0.8.37)
	CHECK(!hasLine(lines, "NOUVELLE PARTIE"));
	CHECK(!hasLine(lines, "CHARGER UNE PARTIE"));
	CHECK(!hasLine(lines, "HAUT/BAS : CHOISIR   CROIX : VALIDER"));
	// A-17 : le logo fait partie de l'ecran menu, pas seulement du titre.
	CHECK(countColor(parityColor(28, 22, 7)) > 0);  // lettres dorees
	CHECK(countColor(parityColor(28, 6, 6)) > 0);   // "2" rouge
	// A-07 : aucun disque lunaire dans le fond du menu.
	CHECK(countColor(parityColor(28, 27, 24)) == 0);
	// C-01 : fond de vraie zone 011, voile semi-transparent en 5 bandes de 1..3 passes.
	CHECK(g_zoneDrawCalls > 0);
	CHECK(g_zoneCamX >= 390 && g_zoneCamX <= 540);  // 26*24-160 +/- 70 a frame=1
	CHECK(g_zoneCamY >= 650 && g_zoneCamY <= 710);  // centre (26,45), hauteur 12
	CHECK(countSemiVeil() == 11);  // 5 bandes x 1..3 passes, une prim par bande/passe
	CHECK(veilPasses(0) == 3);
	CHECK(veilPasses(30) == 2);
	CHECK(veilPasses(112) == 1);
	CHECK(veilPasses(239) == 3);
	// La camera de la capture ISO (~frame 154) doit rester dans la fenetre PC.
	const uint32_t savedFrame = g_frame;
	g_frame = 154u;
	resetDraw();
	menuDrawPS1();
	CHECK(g_zoneCamX >= 500 && g_zoneCamX <= 530);
	CHECK(g_zoneCamY >= 670 && g_zoneCamY <= 700);
	g_frame = savedFrame;
}

void checkMainWithSave() {
	drawScreen(1, MENU_SCR_MAIN);
	const auto lines = renderedLines();
	CHECK(hasLine(lines, "Continuer"));
	CHECK(hasLine(lines, "Nouvelle partie"));
	CHECK(hasLine(lines, "Options"));
	CHECK(hasLine(lines, "Quitter"));
	CHECK(countColor(parityColor(28, 22, 7)) > 0);
	CHECK(countColor(parityColor(28, 27, 24)) == 0);
}

int countFontSprites(int y, int bank) {
	int n = 0;
	for (const Sprite& s : g_sprites)
		if (s.y == y && s.h == 16 && s.clutX == PF_FONT_CLUT_X &&
		    s.clutY == PF_FONT_CLUT_Y + bank) ++n;
	return n;
}

bool hasRect(int x, int y, int w, int h, uint32_t color, int z) {
	for (const Rect& r : g_rects)
		if (r.x == x && r.y == y && r.w == w && r.h == h &&
		    r.color == color && r.z == uint32_t(z)) return true;
	return false;
}

void checkSelectionParity() {
	// v0.8.14 : la reference PC dessine le bandeau avec une alpha de 5..7/255
	// (src/title.cpp::drawMenu) : il est quasiment invisible. L'aplat cyan
	// opaque du PS1 (16,144,160) doit disparaitre, pas les losanges ni le texte.
	drawScreen(1, MENU_SCR_MAIN);
	CHECK(countColor(parityColor(2, 18, 20)) == 0);   // ancien bandeau opaque
	// Losanges : bgr(30,40,42) etait sature en (30,31,31) = (240,248,248).
	CHECK(countColor(parityColor(30, 31, 31)) == 0);
	CHECK(countColor(parityColor(15, 20, 21)) > 0);  // (120,160,168) reference PC
	// Le logo est dessine en rectangles gold : compter ces rectangles ne
	// prouverait pas que le libelle selectionne est en CLUT banque 0. On
	// verifie donc les SPRT FONT3 de la ligne "Continuer" (top=143) et
	// l'absence de la banque 1 ivoire sur cette meme ligne.
	const int top = 143;  // g_menu.hasSave == 1
	const int labelLen = 9;  // strlen("Continuer")
	// v0.8.37 : la banque 0 (encre olive) etait invisible sur le bandeau ; le PC
	// ecrit la selection en (210,232,225) (title.cpp:565) -> banque SELECT.
	CHECK(countFontSprites(top, PF_FONT_BANK_SELECT) >= labelLen);
	CHECK(countFontSprites(top, 0) == 0);
	CHECK(kA1FontCluts[PF_FONT_BANK_SELECT][1] == parityColor(26, 29, 28));
	// 12 bandes semi-transparentes bgr(5,7,7) : 12 passes saturant sur F,
	// F est la couleur finale visee au centre (~(40,54,53) cote PC). On
	// verifie les 12 largeurs exactes, en z=2, pour eviter un faux positif
	// donne par n'importe quel rectangle semi de la meme couleur.
	const uint32_t band = parityColor(5, 7, 7) | 0x80000000u;
	int semiBands = 0, opaqueBands = 0;
	for (const Rect& r : g_rects) {
		if (r.color == band && r.z == 2) ++semiBands;
		if (r.color == (band & ~0x80000000u) && r.y >= 130 && r.y <= 190) ++opaqueBands;
	}
	CHECK(semiBands == 12);
	CHECK(opaqueBands == 0);
	for (int k = 0; k < 12; ++k) {
		const int half = 96 - k * 8;
		CHECK(hasRect(PF_W / 2 - half, top - 3, 2 * half, 14, band, 2));
	}
}

void checkTitleSubtitle() {
	// v0.8.15 : la reference PC dessine "- remake -" en (226,150,110)
	// (src/title.cpp:616). La banque 0 olive (41,49,16) le rendait invisible
	// sur le fond dore du titre. Le harnais doit echouer tant que le SPRT n'est
	// pas emis avec la 5e CLUT orange.
	CHECK(kA1FontCluts[PF_FONT_BANK_ORANGE][1] == parityColor(28, 18, 13));
	menuInitPS1(0, 0);
	resetDraw();
	menuDrawPS1();
	CHECK(hasLine(renderedLines(), "- remake -"));
	const int subtitleLen = 10;  // strlen("- remake -")
	CHECK(countFontSprites(101, PF_FONT_BANK_ORANGE) == subtitleLen);
	CHECK(countFontSprites(101, PF_FONT_BANK_GOLD) == 0);
	CHECK(countFontSprites(101, PF_FONT_BANK_IVORY) == 0);
}

void checkOptionsContrast() {
	constexpr int kPale = 5;  // PF_FONT_BANK_PALE, ajoute a l'etape suivante
	CHECK(kA1FontCluts[kPale][1] == parityColor(18, 20, 24));  // (150,164,196) -> (144,160,192)
	menuInitPS1(0, 0);
	menuUpdatePS1(PF_BTN_START);
	menuUpdatePS1(0);
	menuUpdatePS1(PF_BTN_DOWN);
	menuUpdatePS1(0);
	menuUpdatePS1(PF_BTN_CROSS);
	menuUpdatePS1(0);
	CHECK(menuScreenPS1() == MENU_SCR_OPTIONS);
	resetDraw();
	menuDrawPS1();

	const auto lines = renderedLines();
	CHECK(hasLineContaining(lines, "VITESSE DU TEXTE"));
	CHECK(hasLineContaining(lines, "AUDIO"));
	CHECK(hasLineContaining(lines, "AFFICHER COMMANDES"));

	// Ligne selectionnee (y=126) : "VITESSE DU TEXTE" (16) + "NORMAL" (6).
	CHECK(countFontSprites(126, PF_FONT_BANK_IVORY) == 22);
	CHECK(countFontSprites(126, 0) == 0);
	CHECK(countFontSprites(126, 2) == 0);
	CHECK(countFontSprites(126, kPale) == 0);

	// "AUDIO" (5) + "INDISPONIBLE" (12), audioAvailable=0.
	CHECK(countFontSprites(150, kPale) == 17);
	CHECK(countFontSprites(150, 0) == 0);
	CHECK(countFontSprites(150, 2) == 0);

	// "AFFICHER COMMANDES" (18) + "NON" (3).
	CHECK(countFontSprites(174, kPale) == 21);
	CHECK(countFontSprites(174, 0) == 0);
	CHECK(countFontSprites(174, 2) == 0);

	// Le carre dore du curseur et le panneau WIND ne bougent pas.
	CHECK(hasRect(32, 131, 6, 6, parityColor(28, 22, 7), 2));
}

void checkOptionsFooter() {
	menuInitPS1(0, 0);
	menuUpdatePS1(PF_BTN_START);
	menuUpdatePS1(0);
	menuUpdatePS1(PF_BTN_DOWN);
	menuUpdatePS1(0);  // curseur 1 = Options sans sauvegarde
	menuUpdatePS1(PF_BTN_CROSS);
	menuUpdatePS1(0);
	CHECK(menuScreenPS1() == MENU_SCR_OPTIONS);
	resetDraw();
	menuDrawPS1();
	CHECK(hasLine(renderedLines(), "Gauche / Droite : r\351gler    C : retour"));
}

void checkMenuHintColor() {
	constexpr int kHint = 6;  // PF_FONT_BANK_HINT, ajoute a l'etape suivante
	constexpr int kBanks = int(sizeof(kA1FontCluts) / sizeof(kA1FontCluts[0]));
	CHECK(kBanks == 13);  // + menu et prologue PC (v0.8.37)
	// Evite une lecture hors tableau pendant la phase RED.
	if (kBanks > kHint)
		CHECK(kA1FontCluts[kHint][1] == parityColor(15, 16, 21));  // (120,132,166) -> (120,128,168)

	// src/title.cpp:568 ecrit l'aide en (120,132,166), pas en cyan (120,216,216).
	drawScreen(0, MENU_SCR_MAIN);
	CHECK(countFontSprites(PF_H - 14, kHint) == 39);  // "Haut / Bas : choisir    Croix : valider"
	CHECK(countFontSprites(PF_H - 14, 2) == 0);

	menuInitPS1(0, 0);
	menuUpdatePS1(PF_BTN_START);
	menuUpdatePS1(0);
	menuUpdatePS1(PF_BTN_DOWN);
	menuUpdatePS1(0);
	menuUpdatePS1(PF_BTN_CROSS);
	menuUpdatePS1(0);
	CHECK(menuScreenPS1() == MENU_SCR_OPTIONS);
	resetDraw();
	menuDrawPS1();
	CHECK(countFontSprites(PF_H - 14, kHint) == 38);  // "Gauche / Droite : r\351gler    C : retour"
	CHECK(countFontSprites(PF_H - 14, 2) == 0);
}

void checkQuitAction() {
	// Sans sauvegarde, "Quitter" est la troisieme entree.
	menuInitPS1(0, 0);
	menuUpdatePS1(PF_BTN_START);
	menuUpdatePS1(0);
	menuUpdatePS1(PF_BTN_DOWN);
	menuUpdatePS1(0);
	menuUpdatePS1(PF_BTN_DOWN);
	menuUpdatePS1(0);
	menuUpdatePS1(PF_BTN_CROSS);
	// Valeur attendue de la nouvelle action "Quitter" (1=new, 2=load).
	CHECK(menuActionPS1() == 3);
}

}  // namespace

int main() {
	checkMainNoSave();
	checkMainWithSave();
	checkSelectionParity();
	checkTitleSubtitle();
	checkOptionsFooter();
	checkMenuHintColor();
	checkOptionsContrast();
	checkQuitAction();
	std::printf("PS1_MENU_PARITY: %d/%d checks PASS\n", checks - failures, checks);
	return failures == 0 ? 0 : 1;
}
