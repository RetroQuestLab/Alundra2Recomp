#pragma once
/*
 * Machine a etats du menu de depart PS1, sans STL ni dependance SDK.
 *
 * Ce header est compile deux fois :
 *   - par le build MIPS (via menu_ps1.cpp), avec les bits PF_BTN_* de pf.h ;
 *   - par le test hote tools/ps1_menu_logic_test.cpp (clang++ x86_64).
 * Il ne doit contenir que des POD et des fonctions inline : aucune allocation,
 * aucun destructeur global, aucune bibliotheque.
 *
 * Le front montant est calcule dans menuStep() : un bouton maintenu ne peut
 * donc pas traverser plusieurs ecrans ni revalider apres une transition.
 */
#include <stdint.h>

#include "../platform/pf.h"

#define MENU_SCR_TITLE    0
#define MENU_SCR_MAIN     1
#define MENU_SCR_OPTIONS  2
#define MENU_SCR_LOAD_MSG 3
#define MENU_SCR_DONE     4

#define MENU_ACT_NONE      0
#define MENU_ACT_NEW_GAME  1
#define MENU_ACT_LOAD_GAME 2
#define MENU_ACT_QUIT      3

#define MENU_OPTION_COUNT    3
#define MENU_LOAD_MSG_FRAMES 90

// Le menu PC (src/title.cpp::mainCount/mainLabel) affiche 4 entrees avec
// sauvegarde et 3 sans. Les libelles sont identiques sur les deux cibles :
// seule la manette remplace le clavier dans les indications de commande.
inline int menuMainCount(int hasSave) { return hasSave ? 4 : 3; }

inline const char* menuMainLabel(int item, int hasSave) {
	if (hasSave) {
		switch (item) {
			case 0: return "Continuer";
			case 1: return "Nouvelle partie";
			case 2: return "Options";
			default: return "Quitter";
		}
	}
	switch (item) {
		case 0: return "Nouvelle partie";
		case 1: return "Options";
		default: return "Quitter";
	}
}

struct MenuState {
	int screen;         // MENU_SCR_*
	int cursor;         // entree selectionnee dans l'ecran courant
	int textSpeed;      // 0=lent, 1=normal, 2=rapide
	int audio;          // 0..10, reglage effectif seulement si audioAvailable
	int showControls;   // 0/1 : affichage des commandes a l'ecran
	int hasSave;        // 0/1 : fourni par le systeme de sauvegarde
	int audioAvailable; // 0/1 : fourni par le sous-systeme SPU
	int messageTimer;   // frames restantes du message "aucune sauvegarde"
	uint32_t prevPad;   // masque PF_BTN_* de la frame precedente
	int action;         // MENU_ACT_* produit, remis a NONE par menuInit()
};

inline int menuClamp(int v, int lo, int hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

inline void menuInit(MenuState* s, int hasSave, int audioAvailable) {
	s->screen = MENU_SCR_TITLE;
	s->cursor = 0;
	s->textSpeed = 1;
	s->audio = 5;
	s->showControls = 1;
	s->hasSave = hasSave ? 1 : 0;
	s->audioAvailable = audioAvailable ? 1 : 0;
	s->messageTimer = 0;
	s->prevPad = 0;
	s->action = MENU_ACT_NONE;
}

inline void menuStep(MenuState* s, uint32_t pad) {
	const uint32_t pressed = pad & ~s->prevPad;
	s->prevPad = pad;

	switch (s->screen) {
		case MENU_SCR_TITLE:
			if (pressed & PF_BTN_START) {
				s->screen = MENU_SCR_MAIN;
				s->cursor = 0;
			}
			break;

		case MENU_SCR_MAIN: {
			const int count = menuMainCount(s->hasSave);
			if (pressed & (PF_BTN_UP | PF_BTN_DOWN)) {
				const int dir = (pressed & PF_BTN_DOWN) ? 1 : -1;
				s->cursor = (s->cursor + dir + count) % count;
			}
			if (pressed & (PF_BTN_CROSS | PF_BTN_START)) {
				// Meme decalage que mainAction() cote PC : l'entree "Continuer"
				// n'existe que lorsqu'une sauvegarde est disponible.
				const int act = s->hasSave ? s->cursor : s->cursor + 1;
				switch (act) {
					case 0:  // Continuer
						s->action = MENU_ACT_LOAD_GAME;
						s->screen = MENU_SCR_DONE;
						break;
					case 1:  // Nouvelle partie
						s->action = MENU_ACT_NEW_GAME;
						s->screen = MENU_SCR_DONE;
						break;
					case 2:  // Options
						s->screen = MENU_SCR_OPTIONS;
						s->cursor = 0;
						break;
					default:  // Quitter
						s->action = MENU_ACT_QUIT;
						s->screen = MENU_SCR_DONE;
						break;
				}
			}
			break;
		}

		case MENU_SCR_OPTIONS: {
			if (pressed & (PF_BTN_UP | PF_BTN_DOWN)) {
				const int dir = (pressed & PF_BTN_DOWN) ? 1 : -1;
				s->cursor = (s->cursor + dir + MENU_OPTION_COUNT) % MENU_OPTION_COUNT;
			}
			const int adjust = (pressed & PF_BTN_RIGHT) ? 1 : (pressed & PF_BTN_LEFT) ? -1 : 0;
			if (adjust) {
				if (s->cursor == 0) {
					s->textSpeed = menuClamp(s->textSpeed + adjust, 0, 2);
				} else if (s->cursor == 1 && s->audioAvailable) {
					s->audio = menuClamp(s->audio + adjust, 0, 10);
				} else if (s->cursor == 2) {
					s->showControls = s->showControls ? 0 : 1;
				}
			}
			if (pressed & (PF_BTN_CIRCLE | PF_BTN_SQUARE | PF_BTN_START)) {
				s->screen = MENU_SCR_MAIN;
				s->cursor = s->hasSave ? 2 : 1;  // retour sur "Options"
			}
			break;
		}

		case MENU_SCR_LOAD_MSG:
			if (pressed & (PF_BTN_CIRCLE | PF_BTN_START | PF_BTN_CROSS)) {
				s->screen = MENU_SCR_MAIN;
				s->cursor = 0;
			} else if (--s->messageTimer <= 0) {
				s->screen = MENU_SCR_MAIN;
				s->cursor = 0;
			}
			break;

		case MENU_SCR_DONE:
		default:
			break;
	}
}
