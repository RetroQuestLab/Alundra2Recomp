/*
 * Test hote du duel local de l'ogre PS1 (TDD).
 * Compilation : clang++ -std=c++20 -o ps1\build\ps1_ogre_duel_logic_test.exe tools\ps1_ogre_duel_logic_test.cpp
 *
 * Aucun de ces tests ne touche a la quete : ils pinent uniquement le POD partage
 * avec le pilote MIPS.
 */
#include "../src/ps1/ogre_combat_ps1_logic.h"

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

void stepN(PS1OgreDuelState* s, int n) {
	for (int i = 0; i < n; ++i) ogreDuelStep(s, 0);
}
}  // namespace

int main() {
	// --- etat initial deterministe ---
	{
		PS1OgreDuelState s;
		ogreDuelInit(&s);
		CHECK(s.hp == OGRE_DUEL_MAX_HP);
		CHECK(s.cooldown == 0);
		CHECK(s.stun == 0);
		CHECK(s.flash == 0);
		CHECK(s.phaseTimer == 0);
		CHECK(s.state == OGRE_DUEL_GUARD);
		CHECK(s.hits == 0);
		CHECK(s.prevSquare == 0);
		CHECK(s.steps == 0);
		CHECK(ogreDuelVulnerable(&s) == 0);
		CHECK(ogreDuelStunned(&s) == 0);
	}

	// --- cycle GARDE 120 -> OUVERT 60 -> GARDE ---
	{
		PS1OgreDuelState s;
		ogreDuelInit(&s);
		stepN(&s, 119);
		CHECK(s.state == OGRE_DUEL_GUARD);
		CHECK(s.phaseTimer == 119);
		ogreDuelStep(&s, 0);
		CHECK(s.state == OGRE_DUEL_OPEN);
		CHECK(s.phaseTimer == 0);
		CHECK(ogreDuelVulnerable(&s) == 1);
		stepN(&s, 59);
		CHECK(s.state == OGRE_DUEL_OPEN);
		ogreDuelStep(&s, 0);
		CHECK(s.state == OGRE_DUEL_GUARD);
		CHECK(ogreDuelVulnerable(&s) == 0);
		CHECK(s.steps == 180);
	}

	// --- coup en GARDE : aucun PV retire, flash absorbe court ---
	{
		PS1OgreDuelState s;
		ogreDuelInit(&s);
		CHECK(ogreDuelTryHit(&s, 0, 1, 1) == 1);
		CHECK(s.hp == OGRE_DUEL_MAX_HP);
		CHECK(s.hits == 0);
		CHECK(s.flash == OGRE_DUEL_ABSORB_FLASH_TICKS);
		CHECK(s.state == OGRE_DUEL_GUARD);
		CHECK(s.cooldown == 0);
		stepN(&s, OGRE_DUEL_ABSORB_FLASH_TICKS);
		CHECK(s.flash == 0);
	}

	// --- maintien de CARRE = un seul coup, puis nouveau front apres relachement ---
	{
		PS1OgreDuelState s;
		ogreDuelInit(&s);
		for (int i = 0; i < 50; ++i) ogreDuelTryHit(&s, 0, 1, 1);
		CHECK(s.prevSquare == 1);
		CHECK(s.hits == 0);
		CHECK(s.hp == OGRE_DUEL_MAX_HP);
		ogreDuelTryHit(&s, 0, 0, 1);
		CHECK(s.prevSquare == 0);
		stepN(&s, 120);
		CHECK(s.state == OGRE_DUEL_OPEN);
		CHECK(ogreDuelTryHit(&s, 0, 1, 1) == 1);
		CHECK(s.hits == 1);
		CHECK(s.hp == OGRE_DUEL_MAX_HP - OGRE_DUEL_HIT_DAMAGE);
	}

	// --- hors de portee : le front est quand meme consomme ---
	{
		PS1OgreDuelState s;
		ogreDuelInit(&s);
		CHECK(ogreDuelTryHit(&s, 0, 1, 0) == 0);
		CHECK(s.prevSquare == 1);
		CHECK(s.hits == 0);
		CHECK(s.flash == 0);
	}

	// --- coup en OUVERTURE : PV, hits, stun, flash, cooldown, etat ---
	{
		PS1OgreDuelState s;
		ogreDuelInit(&s);
		stepN(&s, 120);
		CHECK(ogreDuelTryHit(&s, 0, 1, 1) == 1);
		CHECK(s.hp == 12);
		CHECK(s.hits == 1);
		CHECK(s.stun == OGRE_DUEL_STUN_TICKS);
		CHECK(s.flash == OGRE_DUEL_FLASH_TICKS);
		CHECK(s.cooldown == OGRE_DUEL_HIT_COOLDOWN);
		CHECK(s.state == OGRE_DUEL_STUNNED);
		CHECK(ogreDuelStunned(&s) == 1);
		CHECK(ogreDuelVulnerable(&s) == 0);
	}

	// --- cooldown : aucun coup pendant les 18 ticks, puis coup des que ouvert ---
	{
		PS1OgreDuelState s;
		ogreDuelInit(&s);
		s.state = OGRE_DUEL_OPEN;
		s.phaseTimer = 0;
		s.cooldown = 5;
		CHECK(ogreDuelTryHit(&s, 0, 1, 1) == 0);
		CHECK(s.hits == 0);
		CHECK(s.prevSquare == 1);
		stepN(&s, 5);
		CHECK(s.cooldown == 0);
		CHECK(s.state == OGRE_DUEL_OPEN);
		ogreDuelTryHit(&s, 0, 0, 1);
		CHECK(ogreDuelTryHit(&s, 0, 1, 1) == 1);
		CHECK(s.hits == 1);
		CHECK(s.hp == 12);
	}

	// --- expiration du stun : retour en GARDE, timer de phase remis a zero ---
	{
		PS1OgreDuelState s;
		ogreDuelInit(&s);
		stepN(&s, 120);
		ogreDuelTryHit(&s, 0, 1, 1);
		stepN(&s, OGRE_DUEL_STUN_TICKS);
		CHECK(s.stun == 0);
		CHECK(s.state == OGRE_DUEL_GUARD);
		CHECK(s.phaseTimer == 0);
		CHECK(ogreDuelStunned(&s) == 0);
	}

	// --- bossDown gele PV, timers, phase et compteur ---
	{
		PS1OgreDuelState s;
		ogreDuelInit(&s);
		stepN(&s, 30);
		const int hp = s.hp;
		const int state = s.state;
		const int phaseTimer = s.phaseTimer;
		const int flash = s.flash;
		const uint32_t steps = s.steps;
		for (int i = 0; i < 100; ++i) ogreDuelStep(&s, 1);
		CHECK(s.hp == hp);
		CHECK(s.state == state);
		CHECK(s.phaseTimer == phaseTimer);
		CHECK(s.flash == flash);
		CHECK(s.steps == steps);
		CHECK(ogreDuelTryHit(&s, 1, 1, 1) == 0);
		CHECK(s.hits == 0);
		ogreDuelStep(&s, 0);
		CHECK(s.steps == steps + 1);
		CHECK(s.phaseTimer == phaseTimer + 1);
	}

	// --- 7 coups portes -> DOWN terminal, sans toucher a la quete ---
	{
		PS1OgreDuelState s;
		ogreDuelInit(&s);
		for (int hit = 0; hit < 7; ++hit) {
			s.state = OGRE_DUEL_OPEN;
			s.phaseTimer = 0;
			s.cooldown = 0;
			s.stun = 0;
			s.prevSquare = 0;
			CHECK(ogreDuelTryHit(&s, 0, 1, 1) == 1);
			CHECK(ogreDuelTryHit(&s, 0, 0, 1) == 0);
			stepN(&s, 42);
		}
		CHECK(s.hp == 0);
		CHECK(s.hits == 7);
		CHECK(s.state == OGRE_DUEL_DOWN);
		// DOWN gele tous les timers : ils doivent avoir ete remis a zero avant
		// d'entrer dans cet etat, sinon le flash resterait affiche indefiniment.
		CHECK(s.flash == 0);
		CHECK(s.stun == 0);
		CHECK(s.cooldown == 0);
		CHECK(ogreDuelVulnerable(&s) == 0);
		CHECK(ogreDuelStunned(&s) == 1);
		const uint32_t steps = s.steps;
		stepN(&s, 100);
		CHECK(s.steps == steps);
		CHECK(ogreDuelTryHit(&s, 0, 1, 1) == 0);
		CHECK(s.hits == 7);
		CHECK(s.hp == 0);
	}

	// --- reset et stabilite sur 2000 images ---
	{
		PS1OgreDuelState s;
		ogreDuelInit(&s);
		stepN(&s, 500);
		ogreDuelReset(&s);
		CHECK(s.hp == OGRE_DUEL_MAX_HP);
		CHECK(s.state == OGRE_DUEL_GUARD);
		CHECK(s.steps == 0);
		CHECK(s.hits == 0);
		for (int i = 0; i < 2000; ++i) {
			ogreDuelStep(&s, (i / 37) % 11 == 0 ? 1 : 0);
			CHECK(s.hp <= OGRE_DUEL_MAX_HP);
			CHECK(s.state <= OGRE_DUEL_DOWN);
			CHECK(s.flash <= OGRE_DUEL_FLASH_TICKS);
			CHECK(s.stun <= OGRE_DUEL_STUN_TICKS);
			CHECK(s.cooldown <= OGRE_DUEL_HIT_COOLDOWN);
		}
	}

	// --- mapping SFX local : garde / ouverture / down / coup refuse ---
	{
		CHECK(sizeof(PS1OgreDuelState) == 20);

		PS1OgreDuelState s;
		ogreDuelInit(&s);
		PS1OgreDuelState before = s;
		const int accepted = ogreDuelTryHit(&s, 0, 1, 1);
		CHECK(accepted == 1);
		CHECK(ogreDuelSfxForHit(&before, &s, accepted) == OGRE_DUEL_SFX_GUARD);
	}
	{
		PS1OgreDuelState s;
		ogreDuelInit(&s);
		stepN(&s, OGRE_DUEL_GUARD_TICKS);
		CHECK(s.state == OGRE_DUEL_OPEN);
		PS1OgreDuelState before = s;
		const int accepted = ogreDuelTryHit(&s, 0, 1, 1);
		CHECK(accepted == 1);
		CHECK(ogreDuelSfxForHit(&before, &s, accepted) == OGRE_DUEL_SFX_HIT);
	}
	{
		PS1OgreDuelState s;
		ogreDuelInit(&s);
		for (int hit = 0; hit < 6; ++hit) {
			s.state = OGRE_DUEL_OPEN;
			s.phaseTimer = 0;
			s.cooldown = 0;
			s.stun = 0;
			s.prevSquare = 0;
			CHECK(ogreDuelTryHit(&s, 0, 1, 1) == 1);
			CHECK(ogreDuelTryHit(&s, 0, 0, 1) == 0);
			stepN(&s, 42);
		}
		s.state = OGRE_DUEL_OPEN;
		s.phaseTimer = 0;
		s.cooldown = 0;
		s.stun = 0;
		s.prevSquare = 0;
		CHECK(s.hp == 2);
		PS1OgreDuelState before = s;
		const int accepted = ogreDuelTryHit(&s, 0, 1, 1);
		CHECK(accepted == 1);
		CHECK(s.hp == 0);
		CHECK(s.state == OGRE_DUEL_DOWN);
		CHECK(ogreDuelSfxForHit(&before, &s, accepted) == OGRE_DUEL_SFX_DOWN);
	}
	{
		PS1OgreDuelState s;
		ogreDuelInit(&s);
		PS1OgreDuelState before = s;
		int accepted = ogreDuelTryHit(&s, 0, 1, 0);
		CHECK(accepted == 0);
		CHECK(ogreDuelSfxForHit(&before, &s, accepted) == OGRE_DUEL_SFX_NONE);
		accepted = ogreDuelTryHit(&s, 1, 1, 1);
		CHECK(accepted == 0);
		CHECK(ogreDuelSfxForHit(&before, &s, accepted) == OGRE_DUEL_SFX_NONE);
		s.state = OGRE_DUEL_DOWN;
		accepted = ogreDuelTryHit(&s, 0, 1, 1);
		CHECK(accepted == 0);
		CHECK(ogreDuelSfxForHit(&before, &s, accepted) == OGRE_DUEL_SFX_NONE);
	}

	// --- victoire reelle : transition DOWN et autorisation CROIX ---
	{
		PS1OgreDuelState s;
		ogreDuelInit(&s);
		for (int hit = 0; hit < 6; ++hit) {
			s.state = OGRE_DUEL_OPEN;
			s.phaseTimer = 0;
			s.cooldown = 0;
			s.stun = 0;
			s.prevSquare = 0;
			CHECK(ogreDuelTryHit(&s, 0, 1, 1) == 1);
			CHECK(ogreDuelTryHit(&s, 0, 0, 1) == 0);
			stepN(&s, 42);
		}
		CHECK(s.hp == 2);
		PS1OgreDuelState before = s;
		CHECK(ogreDuelInteractAllowed(&before) == 0);
		s.state = OGRE_DUEL_OPEN;
		s.phaseTimer = 0;
		s.cooldown = 0;
		s.stun = 0;
		s.prevSquare = 0;
		const int accepted = ogreDuelTryHit(&s, 0, 1, 1);
		CHECK(accepted == 1);
		CHECK(s.hp == 0);
		CHECK(s.state == OGRE_DUEL_DOWN);
		CHECK(ogreDuelJustWon(&before, &s, accepted) == 1);
		CHECK(ogreDuelInteractAllowed(&s) == 1);
	}
	{
		PS1OgreDuelState s;
		ogreDuelInit(&s);
		PS1OgreDuelState before = s;
		CHECK(ogreDuelJustWon(&before, &s, 1) == 0);
		CHECK(ogreDuelInteractAllowed(&s) == 0);
		const int accepted = ogreDuelTryHit(&s, 0, 1, 1);
		CHECK(accepted == 1);
		CHECK(ogreDuelJustWon(&before, &s, accepted) == 0);
		s.state = OGRE_DUEL_DOWN;
		const PS1OgreDuelState down = s;
		CHECK(ogreDuelJustWon(&down, &s, 1) == 0);
		CHECK(ogreDuelInteractAllowed(&s) == 1);
	}

	std::printf("PS1_OGRE_DUEL_LOGIC: %d/%d checks PASS\n", g_checks - g_fail, g_checks);
	return g_fail == 0 ? 0 : 1;
}
