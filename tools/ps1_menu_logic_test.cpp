/*
 * Test hote de la machine a etats du menu PS1 (TDD).
 * Compile le VRAI header utilise par le build MIPS ; aucun mock.
 *
 *   external\llvm-mingw-20261020-ucrt-x86_64\bin\clang++.exe -std=c++20 ^
 *       -o qa\ps1_menu_logic_test.exe tools\ps1_menu_logic_test.cpp
 *   .\qa\ps1_menu_logic_test.exe
 */
#include "../src/ps1/menu_ps1_logic.h"

#include <cstdio>

namespace {
int g_fail = 0;
int g_checks = 0;
#define CHECK(cond)                                                                    \
	do {                                                                                 \
		++g_checks;                                                                        \
		if (!(cond)) {                                                                     \
			std::printf("FAIL ligne %d : %s\n", __LINE__, #cond);                           \
			++g_fail;                                                                      \
		}                                                                                  \
	} while (0)

// Appui complet : front montant puis relachement.
void tap(MenuState& s, uint32_t bit) {
	menuStep(&s, bit);
	menuStep(&s, 0);
}
}  // namespace

int main() {
	// --- etat initial -------------------------------------------------------------
	{
		MenuState s;
		menuInit(&s, 0, 0);
		CHECK(s.screen == MENU_SCR_TITLE);
		CHECK(s.action == MENU_ACT_NONE);
		CHECK(s.cursor == 0);
		CHECK(s.textSpeed == 1);
		CHECK(s.showControls == 1);
	}

	// --- START sur le titre ouvre le menu, sans le traverser si maintenu -----------
	{
		MenuState s;
		menuInit(&s, 0, 0);
		menuStep(&s, PF_BTN_START);  // front montant : titre -> menu
		for (int i = 0; i < 20; ++i) menuStep(&s, PF_BTN_START);  // maintien
		CHECK(s.screen == MENU_SCR_MAIN);
		CHECK(s.action == MENU_ACT_NONE);
	}

	// --- navigation haute/basse avec fronts montants ------------------------------
	{
		MenuState s;
		menuInit(&s, 0, 0);
		tap(s, PF_BTN_START);
		menuStep(&s, PF_BTN_DOWN);  // front : curseur 1
		CHECK(s.cursor == 1);
		for (int i = 0; i < 10; ++i) menuStep(&s, PF_BTN_DOWN);  // maintien : aucun repeat
		CHECK(s.cursor == 1);
		menuStep(&s, 0);  // relachement avant le nouvel appui
		tap(s, PF_BTN_DOWN);
		CHECK(s.cursor == 2);
		tap(s, PF_BTN_UP);
		CHECK(s.cursor == 1);
	}

	// --- Nouvelle partie (sans sauvegarde : premiere entree) -----------------------
	{
		MenuState s;
		menuInit(&s, 0, 1);
		tap(s, PF_BTN_START);
		tap(s, PF_BTN_CROSS);
		CHECK(s.action == MENU_ACT_NEW_GAME);
		CHECK(s.screen == MENU_SCR_DONE);
	}

	// --- Continuer avec sauvegarde : premiere entree -------------------------------
	{
		MenuState s;
		menuInit(&s, 1, 0);
		tap(s, PF_BTN_START);
		tap(s, PF_BTN_CROSS);
		CHECK(s.action == MENU_ACT_LOAD_GAME);
		CHECK(s.screen == MENU_SCR_DONE);
	}

	// --- Nouvelle partie avec sauvegarde : deuxieme entree -------------------------
	{
		MenuState s;
		menuInit(&s, 1, 0);
		tap(s, PF_BTN_START);
		tap(s, PF_BTN_DOWN);
		tap(s, PF_BTN_CROSS);
		CHECK(s.action == MENU_ACT_NEW_GAME);
		CHECK(s.screen == MENU_SCR_DONE);
	}

	// --- Quitter : derniere entree, dans les deux etats de sauvegarde --------------
	{
		MenuState s;
		menuInit(&s, 0, 0);
		tap(s, PF_BTN_START);
		tap(s, PF_BTN_DOWN);
		tap(s, PF_BTN_DOWN);
		tap(s, PF_BTN_CROSS);
		CHECK(s.action == MENU_ACT_QUIT);
		CHECK(s.screen == MENU_SCR_DONE);
	}
	{
		MenuState s;
		menuInit(&s, 1, 0);
		tap(s, PF_BTN_START);
		tap(s, PF_BTN_DOWN);
		tap(s, PF_BTN_DOWN);
		tap(s, PF_BTN_DOWN);
		tap(s, PF_BTN_CROSS);
		CHECK(s.action == MENU_ACT_QUIT);
		CHECK(s.screen == MENU_SCR_DONE);
	}

	// --- Options : ouverture, reglages, retour sans traverse -----------------------
	{
		MenuState s;
		menuInit(&s, 0, 0);
		tap(s, PF_BTN_START);
		tap(s, PF_BTN_DOWN);  // sans sauvegarde : "Options" est la 2e entree
		tap(s, PF_BTN_CROSS);
		CHECK(s.screen == MENU_SCR_OPTIONS);
		CHECK(s.cursor == 0);

		tap(s, PF_BTN_RIGHT);
		CHECK(s.textSpeed == 2);
		tap(s, PF_BTN_RIGHT);
		CHECK(s.textSpeed == 2);  // borne haute
		tap(s, PF_BTN_LEFT);
		CHECK(s.textSpeed == 1);

		// audio indisponible : le reglage reste inerte
		tap(s, PF_BTN_DOWN);
		const int audioBefore = s.audio;
		tap(s, PF_BTN_RIGHT);
		CHECK(s.audio == audioBefore);

		tap(s, PF_BTN_DOWN);
		tap(s, PF_BTN_RIGHT);
		CHECK(s.showControls == 0);

		tap(s, PF_BTN_CIRCLE);
		CHECK(s.screen == MENU_SCR_MAIN);
		for (int i = 0; i < 20; ++i) menuStep(&s, PF_BTN_CIRCLE);  // maintenu
		CHECK(s.screen == MENU_SCR_MAIN);
	}

	// --- START valide aussi l'entree courante -------------------------------------
	{
		MenuState s;
		menuInit(&s, 0, 0);
		tap(s, PF_BTN_START);  // titre -> menu
		for (int i = 0; i < 20; ++i) menuStep(&s, 0);
		tap(s, PF_BTN_START);  // valide "Nouvelle partie"
		CHECK(s.action == MENU_ACT_NEW_GAME);
	}

	std::printf("PS1_MENU_LOGIC: %d/%d checks PASS\n", g_checks - g_fail, g_checks);
	return g_fail == 0 ? 0 : 1;
}
