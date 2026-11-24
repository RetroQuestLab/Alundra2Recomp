/*
 * Test hote des constantes vitales du heros PS1 (TDD).
 * Compilation : clang++ -std=c++20 -o ps1\build\ps1_hero_vitals_logic_test.exe tools\ps1_hero_vitals_logic_test.cpp
 *
 * Aucun de ces tests ne touche a la quete ni a la sauvegarde : ils pinent
 * uniquement le POD partage avec le pilote MIPS.
 */
#include "../src/ps1/hero_vitals_ps1_logic.h"

#include <cstddef>
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

void tickN(PS1HeroVitalsState* s, int n) {
	for (int i = 0; i < n; ++i) heroVitalsTick(s);
}
}  // namespace

void hero_vitals_init_has_ten_hp_and_zero_timers() {
	PS1HeroVitalsState s;
	heroVitalsInit(&s);
	CHECK(s.hp == HERO_VITALS_MAX_HP);
	CHECK(s.invuln == 0);
	CHECK(s.attackTimer == 0);
	CHECK(s.hitsTaken == 0);
	CHECK(s.steps == 0);
	CHECK(heroVitalsDown(&s) == 0);
}

void hero_vitals_try_hit_rejects_out_of_guard_and_out_of_range() {
	PS1HeroVitalsState s;
	heroVitalsInit(&s);
	CHECK(heroVitalsTryHit(&s, 1, 0, 1) == 0);
	CHECK(heroVitalsTryHit(&s, 1, 1, 0) == 0);
	CHECK(heroVitalsTryHit(&s, 0, 1, 1) == 0);
	CHECK(s.hp == HERO_VITALS_MAX_HP);
	CHECK(s.invuln == 0);
	CHECK(s.hitsTaken == 0);
}

void hero_vitals_invulnerability_blocks_repeat_hits() {
	PS1HeroVitalsState s;
	heroVitalsInit(&s);
	CHECK(heroVitalsTryHit(&s, 1, 1, 1) == 1);
	CHECK(s.hp == HERO_VITALS_MAX_HP - 1);
	CHECK(s.invuln == HERO_VITALS_INVULN_TICKS);
	CHECK(s.hitsTaken == 1);
	CHECK(heroVitalsTryHit(&s, 1, 1, 1) == 0);
	CHECK(s.hp == HERO_VITALS_MAX_HP - 1);
	CHECK(s.hitsTaken == 1);
	tickN(&s, HERO_VITALS_INVULN_TICKS);
	CHECK(s.invuln == 0);
	CHECK(heroVitalsTryHit(&s, 1, 1, 1) == 1);
	CHECK(s.hp == HERO_VITALS_MAX_HP - 2);
	CHECK(s.hitsTaken == 2);
}

void hero_vitals_zero_hp_is_terminal_until_reset() {
	PS1HeroVitalsState s;
	heroVitalsInit(&s);
	for (int hit = 0; hit < HERO_VITALS_MAX_HP; ++hit) {
		CHECK(heroVitalsTryHit(&s, 1, 1, 1) == 1);
		tickN(&s, HERO_VITALS_INVULN_TICKS);
	}
	CHECK(s.hp == 0);
	CHECK(s.hitsTaken == HERO_VITALS_MAX_HP);
	CHECK(heroVitalsDown(&s) == 1);
	CHECK(heroVitalsTryHit(&s, 1, 1, 1) == 0);
	CHECK(s.hp == 0);
	CHECK(s.hitsTaken == HERO_VITALS_MAX_HP);
}

void hero_vitals_reset_restores_full_hp() {
	PS1HeroVitalsState s;
	heroVitalsInit(&s);
	CHECK(heroVitalsTryHit(&s, 1, 1, 1) == 1);
	heroVitalsReset(&s);
	CHECK(s.hp == HERO_VITALS_MAX_HP);
	CHECK(s.invuln == 0);
	CHECK(s.attackTimer == 0);
	CHECK(s.hitsTaken == 0);
	CHECK(s.steps == 0);
	CHECK(heroVitalsDown(&s) == 0);
}

void hero_vitals_respawn_sets_ninety_ticks_of_invulnerability() {
	PS1HeroVitalsState s;
	heroVitalsInit(&s);
	heroVitalsTryHit(&s, 1, 1, 1);
	heroVitalsRespawn(&s);
	CHECK(s.hp == HERO_VITALS_MAX_HP);
	CHECK(s.invuln == HERO_VITALS_RESPAWN_INVULN_TICKS);
	CHECK(s.hitsTaken == 0);
	CHECK(heroVitalsTryHit(&s, 1, 1, 1) == 0);
	CHECK(s.hp == HERO_VITALS_MAX_HP);
	tickN(&s, HERO_VITALS_RESPAWN_INVULN_TICKS);
	CHECK(s.invuln == 0);
	CHECK(heroVitalsTryHit(&s, 1, 1, 1) == 1);
}

void hero_vitals_tick_decrements_timers_without_wrap() {
	PS1HeroVitalsState s;
	heroVitalsInit(&s);
	CHECK(heroVitalsTryHit(&s, 1, 1, 1) == 1);
	CHECK(s.invuln == HERO_VITALS_INVULN_TICKS);
	CHECK(s.attackTimer == HERO_VITALS_ATTACK_PERIOD);
	CHECK(s.steps == 0);
	tickN(&s, HERO_VITALS_ATTACK_PERIOD - 1);
	CHECK(s.attackTimer == 1);
	CHECK(s.invuln == HERO_VITALS_INVULN_TICKS - HERO_VITALS_ATTACK_PERIOD + 1);
	tickN(&s, 1);
	CHECK(s.attackTimer == 0);
	tickN(&s, HERO_VITALS_INVULN_TICKS - HERO_VITALS_ATTACK_PERIOD + 1);
	CHECK(s.invuln == 0);
	CHECK(s.steps == HERO_VITALS_INVULN_TICKS + 1);
}

void hero_vitals_state_is_sixteen_bytes() {
	CHECK(sizeof(PS1HeroVitalsState) == 16);
	CHECK(offsetof(PS1HeroVitalsState, hp) == 0);
	CHECK(offsetof(PS1HeroVitalsState, invuln) == 2);
	CHECK(offsetof(PS1HeroVitalsState, attackTimer) == 4);
	CHECK(offsetof(PS1HeroVitalsState, hitsTaken) == 6);
	CHECK(offsetof(PS1HeroVitalsState, steps) == 8);
	CHECK(offsetof(PS1HeroVitalsState, reserved) == 12);
}

int main() {
	hero_vitals_init_has_ten_hp_and_zero_timers();
	hero_vitals_try_hit_rejects_out_of_guard_and_out_of_range();
	hero_vitals_invulnerability_blocks_repeat_hits();
	hero_vitals_zero_hp_is_terminal_until_reset();
	hero_vitals_reset_restores_full_hp();
	hero_vitals_respawn_sets_ninety_ticks_of_invulnerability();
	hero_vitals_tick_decrements_timers_without_wrap();
	hero_vitals_state_is_sixteen_bytes();

	if (g_fail != 0) {
		std::printf("PS1_HERO_VITALS_LOGIC: %d/%d checks FAIL\n",
		            g_checks - g_fail, g_checks);
		return 1;
	}
	std::printf("PS1_HERO_VITALS_LOGIC: %d/%d checks PASS\n", g_checks, g_checks);
	return 0;
}
