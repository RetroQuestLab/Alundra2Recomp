/*
 * Test hote de la logique d'introduction PS1 : comportement du prologue PC
 * (src/title.cpp : FADE = 50, 0,55 caractere par tick, depart automatique
 * 200 ticks apres la frappe, attaque = completer puis passer).
 */
#include "../src/ps1/intro_ps1_logic.h"
#include "../src/ps1/intro_data_ps1.h"

#include <cstdio>

namespace {
int g_fail = 0;
int g_checks = 0;
#define CHECK(cond)                                                     \
	do {                                                                  \
		++g_checks;                                                         \
		if (!(cond)) {                                                      \
			std::printf("FAIL ligne %d : %s\n", __LINE__, #cond);            \
			++g_fail;                                                       \
		}                                                                   \
	} while (0)

IntroCardMeta g_meta[16];

void initFromData(IntroState& s) {
	for (int i = 0; i < kIntroCardCount; ++i) {
		g_meta[i].scene = uint8_t(kIntroCards[i].scene);
		g_meta[i].len = uint16_t(kIntroCards[i].len);
	}
	introInit(&s, g_meta, kIntroCardCount);
}
}  // namespace

int main() {
	// Donnees : une carte par replique du prologue, scenes du PC.
	CHECK(kIntroCardCount == 9);
	CHECK(kIntroCards[0].scene == INTRO_SCENE_ZONE && kIntroCards[0].zone == 42);
	CHECK(kIntroCards[1].scene == INTRO_SCENE_ZONE && kIntroCards[1].zone == 18);
	CHECK(kIntroCards[2].scene == INTRO_SCENE_SHIP && kIntroCards[2].shipFirst == 2 &&
	      kIntroCards[2].shipLast == 6);
	CHECK(kIntroCards[3].name[0] == '-');  // "- Zeppo -"
	CHECK(kIntroCards[7].scene == INTRO_SCENE_FALL);
	CHECK(kIntroCards[8].scene == INTRO_SCENE_ZONE && kIntroCards[8].zone == 26);
	for (int i = 0; i < kIntroCardCount; ++i) CHECK(kIntroCards[i].nlines >= 1 && kIntroCards[i].nlines <= 4);

	// Frappe : rien pendant le fondu, puis 0,55 caractere par tick.
	CHECK(introShown(INTRO_FADE, 100) == 0);
	CHECK(introShown(INTRO_FADE + 20, 100) == 11);
	CHECK(introShown(INTRO_FADE + 1000, 100) == 100);
	CHECK(introTypedAt(11) == INTRO_FADE + 20);

	// Sans appui : depart automatique, fondu de sortie, carte suivante.
	IntroState s;
	initFromData(s);
	CHECK(s.card == 0 && s.scene == INTRO_SCENE_ZONE && !introDone(&s));
	const int typed = introTypedAt(kIntroCards[0].len);
	for (int i = 0; i < typed + INTRO_AUTO_WAIT; ++i) introStep(&s, 0);
	CHECK(s.card == 0 && s.leaving == 0);
	introStep(&s, 0);
	CHECK(s.leaving == INTRO_FADE);
	for (int i = 0; i < INTRO_FADE; ++i) introStep(&s, 0);
	CHECK(s.card == 1 && s.tick == 0 && s.scene == INTRO_SCENE_ZONE);

	// CROIX : complete le texte, puis passe a la carte suivante.
	introStep(&s, PF_BTN_CROSS);
	CHECK(s.tick == introTypedAt(kIntroCards[1].len));
	introStep(&s, 0);
	introStep(&s, PF_BTN_CROSS);
	CHECK(s.leaving == INTRO_FADE);
	// Un appui maintenu n'avance pas deux fois.
	introStep(&s, PF_BTN_CROSS);
	CHECK(s.leaving == INTRO_FADE - 1);

	// START : passe toute l'introduction immediatement.
	introStep(&s, 0);
	introStep(&s, PF_BTN_START);
	CHECK(introDone(&s) && s.skipped == 1);
	const int f = s.frame;
	introStep(&s, 0);
	CHECK(s.frame == f);  // plus rien n'avance une fois terminee

	// Parcours complet sans appui : toutes les cartes, puis fin.
	initFromData(s);
	int guard = 0;
	while (!introDone(&s) && guard++ < 20000) introStep(&s, 0);
	CHECK(introDone(&s) && s.skipped == 0 && s.card == kIntroCardCount);

	if (g_fail) {
		std::printf("PS1_INTRO_LOGIC: %d/%d checks FAIL\n", g_checks - g_fail, g_checks);
		return 1;
	}
	std::printf("PS1_INTRO_LOGIC: %d/%d checks PASS\n", g_checks, g_checks);
	return 0;
}
