/*
 * Test hote de l'animation de l'ogre PS1 (TDD).
 * Compilation : clang++ -std=c++20 -o ps1\build\ps1_enemy_logic_test.exe tools\ps1_enemy_logic_test.cpp
 */
#include "../src/ps1/enemy_ps1_logic.h"

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
}  // namespace

int main() {
	// --- etat initial deterministe ---
	{
		PS1EnemyAnimState s;
		enemyAnimInit(&s);
		CHECK(s.frame == 0);
		CHECK(s.tick == 0);
		CHECK(s.frames == PS1_ENEMY_ANIM_FRAMES);
		CHECK(s.frozen == 0);
		CHECK(s.steps == 0);
		CHECK(enemyAnimFrame(&s) == 0);
	}

	// --- une seule avance apres exactement PS1_ENEMY_TICKS_PER_FRAME pumps ---
	{
		PS1EnemyAnimState s;
		enemyAnimInit(&s);
		for (int i = 0; i < PS1_ENEMY_TICKS_PER_FRAME - 1; ++i) enemyAnimPump(&s, 0);
		CHECK(s.frame == 0);
		CHECK(s.steps == 0);
		enemyAnimPump(&s, 0);
		CHECK(s.frame == 1);
		CHECK(s.tick == 0);
		CHECK(s.steps == 1);
	}

	// --- boucle complete 0..5 -> 0, sans depassement ---
	{
		PS1EnemyAnimState s;
		enemyAnimInit(&s);
		for (int i = 0; i < PS1_ENEMY_ANIM_FRAMES * PS1_ENEMY_TICKS_PER_FRAME; ++i)
			enemyAnimPump(&s, 0);
		CHECK(s.frame == 0);
		CHECK(s.steps == PS1_ENEMY_ANIM_FRAMES);
		CHECK(s.frame < s.frames);
	}

	// --- bossDown gele l'animation, la reprise ne saute pas de frame ---
	{
		PS1EnemyAnimState s;
		enemyAnimInit(&s);
		for (int i = 0; i < PS1_ENEMY_TICKS_PER_FRAME + 3; ++i) enemyAnimPump(&s, 0);
		const int frame = s.frame;
		const uint32_t steps = s.steps;
		CHECK(s.frozen == 0);
		for (int i = 0; i < 100; ++i) enemyAnimPump(&s, 1);
		CHECK(s.frozen == 1);
		CHECK(s.frame == frame);
		CHECK(s.steps == steps);
		enemyAnimPump(&s, 0);
		CHECK(s.frozen == 0);
		CHECK(s.frame == frame);
		CHECK(s.steps == steps);
	}

	// --- reset : frame/tick/steps a zero, nombre de frames conserve ---
	{
		PS1EnemyAnimState s;
		enemyAnimInit(&s);
		for (int i = 0; i < 3 * PS1_ENEMY_TICKS_PER_FRAME; ++i) enemyAnimPump(&s, 0);
		enemyAnimReset(&s);
		CHECK(s.frame == 0);
		CHECK(s.tick == 0);
		CHECK(s.steps == 0);
		CHECK(s.frames == PS1_ENEMY_ANIM_FRAMES);
		CHECK(s.frozen == 0);
	}

	// --- nombre de frames borne : jamais de lecture hors blob ---
	{
		PS1EnemyAnimState s;
		enemyAnimInit(&s);
		enemyAnimSetFrames(&s, 0);
		CHECK(s.frames == 1);
		enemyAnimSetFrames(&s, 999);
		CHECK(s.frames == PS1_ENEMY_ANIM_FRAMES);
		enemyAnimSetFrames(&s, 3);
		CHECK(s.frames == 3);
		for (int i = 0; i < 500; ++i) enemyAnimPump(&s, 0);
		CHECK(s.frame < s.frames);
		enemyAnimSetFrames(&s, 1);
		CHECK(s.frame < s.frames);
	}

	// --- long retour : invariants sur 1000 images, aucun debordement ---
	{
		PS1EnemyAnimState s;
		enemyAnimInit(&s);
		for (int i = 0; i < 1000; ++i) {
			enemyAnimPump(&s, (i / 37) % 11 == 0 ? 1 : 0);
			CHECK(s.tick < PS1_ENEMY_TICKS_PER_FRAME);
			CHECK(s.frame < s.frames);
			CHECK((s.frozen == 0) || (s.frozen == 1));
		}
		CHECK(s.steps > 0);
		CHECK(s.steps < 1000);
	}

	std::printf("PS1_ENEMY_LOGIC: %d/%d checks PASS\n", g_checks - g_fail, g_checks);
	return g_fail == 0 ? 0 : 1;
}
