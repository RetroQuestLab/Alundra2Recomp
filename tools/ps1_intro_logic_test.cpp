/*
 * Test hote de la logique d'introduction PS1 (TDD).
 * Compilation : clang++ -std=c++20 -o qa\ps1_intro_logic_test.exe tools\ps1_intro_logic_test.cpp
 */
#include "../src/ps1/intro_ps1_logic.h"

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

void advance(IntroState& s, int frames) {
	for (int i = 0; i < frames; ++i) introStep(&s, 0);
}

// v0.8.18 : les decors doivent changer exactement quand le panneau de texte
// change. Litteraux volontaires (pas les constantes sous test) pour rester
// compilable et detecter une frontiere fausse avant le correctif.
void checkScenePanelSync() {
	constexpr int kSkyEnd = 648;    // 3 panneaux x 216 frames
	constexpr int kShipEnd = 1728;  // 8 panneaux x 216 frames
	constexpr int kFallEnd = 1944;  // 9 panneaux x 216 frames
	constexpr int kTotal = 2160;
	constexpr int kPanels = 10;
	struct Case {
		int frame;
		int scene;
		int panel;
	};
	const Case cases[] = {
		{kSkyEnd - 1, INTRO_SCENE_SKY, 2},
		{kSkyEnd, INTRO_SCENE_SHIP, 3},
		{kShipEnd - 1, INTRO_SCENE_SHIP, 7},
		{kShipEnd, INTRO_SCENE_FALL, 8},
		{kFallEnd - 1, INTRO_SCENE_FALL, 8},
		{kFallEnd, INTRO_SCENE_BEACH, 9},
	};
	for (const Case& c : cases) {
		IntroState s;
		introInit(&s);
		advance(s, c.frame);
		CHECK(s.frame == c.frame);
		CHECK(s.scene == c.scene);
		CHECK((c.frame * kPanels) / kTotal == c.panel);
	}
}
}  // namespace

int main() {
	// --- etat initial ---
	{
		IntroState s;
		introInit(&s);
		CHECK(s.frame == 0);
		CHECK(s.scene == INTRO_SCENE_SKY);
		CHECK(introDone(&s) == 0);
		CHECK(s.skipped == 0);
	}

	// --- deroulement borne, sans ressource externe ---
	{
		IntroState s;
		introInit(&s);
		advance(s, 500);
		CHECK(s.scene == INTRO_SCENE_SKY);
		advance(s, 800);  // frame 1300 : texte du navire, decor du navire
		CHECK(s.scene == INTRO_SCENE_SHIP);
		advance(s, 500);  // frame 1800 : texte de la chute, decor de la chute
		CHECK(s.scene == INTRO_SCENE_FALL);
		advance(s, 200);  // frame 2000 : texte de la plage, decor de la plage
		CHECK(s.scene == INTRO_SCENE_BEACH);
		advance(s, INTRO_TOTAL_FRAMES + 10);
		CHECK(introDone(&s) == 1);
		CHECK(s.frame == INTRO_TOTAL_FRAMES);
	}

	// --- START passe l'introduction immediatement, sans double declenchement ---
	{
		IntroState s;
		introInit(&s);
		introStep(&s, PF_BTN_START);
		CHECK(s.skipped == 1);
		CHECK(introDone(&s) == 1);
		CHECK(s.scene == INTRO_SCENE_DONE);
		for (int i = 0; i < 30; ++i) introStep(&s, PF_BTN_START);
		CHECK(introDone(&s) == 1);
	}

	// --- plusieurs nouvelles parties : reinitialisation complete ---
	for (int game = 0; game < 3; ++game) {
		IntroState s;
		introInit(&s);
		advance(s, 700);
		introStep(&s, PF_BTN_START);
		CHECK(introDone(&s) == 1);
		introInit(&s);
		CHECK(s.frame == 0);
		CHECK(s.scene == INTRO_SCENE_SKY);
		CHECK(s.skipped == 0);
	}

	checkScenePanelSync();

	std::printf("PS1_INTRO_LOGIC: %d/%d checks PASS\n", g_checks - g_fail, g_checks);
	return g_fail == 0 ? 0 : 1;
}
