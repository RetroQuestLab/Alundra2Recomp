/*
 * Test hote de la logique de mixage PS1 (TDD).
 * Compilation : clang++ -std=c++20 -o ps1\build\ps1_mixer_logic_test.exe tools\ps1_mixer_logic_test.cpp
 */
#include "../src/ps1/mixer_ps1_logic.h"

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

PS1MixAction step(PS1MixState& s) {
	PS1MixAction a{};
	ps1MixStep(&s, &a);
	return a;
}
}  // namespace

int main() {
	// --- etat initial : aucune musique, volume plein ---
	{
		PS1MixState s;
		ps1MixInit(&s);
		CHECK(s.fade == PS1_FADE_IDLE);
		CHECK(s.level == PS1_MIX_FADE_FRAMES);
		CHECK(s.activeTheme == -1);
		CHECK(s.pendingTheme == -1);
		const PS1MixAction a = step(s);
		CHECK(a.action == PS1_MIX_ACT_NONE);
		CHECK(a.volume == PS1_MIX_VOL_MAX);
	}

	// --- premier theme : demarrage immediat, montee en 30 images ---
	{
		PS1MixState s;
		ps1MixInit(&s);
		CHECK(ps1MixRequestTheme(&s, 1) == PS1_MIX_ACT_START);
		CHECK(s.level == 0);
		CHECK(s.fade == PS1_FADE_IN);
		CHECK(step(s).volume == PS1_MIX_VOL_MAX / PS1_MIX_FADE_FRAMES);
		for (int i = 1; i < PS1_MIX_FADE_FRAMES; ++i) step(s);
		const PS1MixAction a = step(s);
		CHECK(s.level == PS1_MIX_FADE_FRAMES);
		CHECK(s.fade == PS1_FADE_IDLE);
		CHECK(s.fadesIn == 1);
		CHECK(a.volume == PS1_MIX_VOL_MAX);
		CHECK(ps1MixRequestTheme(&s, 1) == PS1_MIX_ACT_NONE);
	}

	// --- changement de theme : descente a zero, swap unique, remontee ---
	{
		PS1MixState s;
		ps1MixInit(&s);
		ps1MixRequestTheme(&s, 1);
		for (int i = 0; i < PS1_MIX_FADE_FRAMES; ++i) step(s);
		CHECK(ps1MixRequestTheme(&s, 17) == PS1_MIX_ACT_NONE);
		CHECK(s.fade == PS1_FADE_OUT);
		int starts = 0;
		int startedTheme = -1;
		for (int i = 0; i < PS1_MIX_FADE_FRAMES; ++i) {
			const PS1MixAction a = step(s);
			if (a.action == PS1_MIX_ACT_START) {
				++starts;
				startedTheme = a.theme;
				CHECK(a.volume == 0);
			}
		}
		CHECK(starts == 1);
		CHECK(startedTheme == 17);
		CHECK(s.activeTheme == 17);
		CHECK(s.fades == 1);
		for (int i = 0; i < PS1_MIX_FADE_FRAMES; ++i) step(s);
		CHECK(s.level == PS1_MIX_FADE_FRAMES);
		CHECK(s.fadesIn == 2);
	}

	// --- nouvelle demande pendant une descente : la derniere gagne ---
	{
		PS1MixState s;
		ps1MixInit(&s);
		ps1MixRequestTheme(&s, 1);
		for (int i = 0; i < 5; ++i) step(s);
		CHECK(ps1MixRequestTheme(&s, 25) == PS1_MIX_ACT_NONE);
		CHECK(s.fade == PS1_FADE_OUT);
		CHECK(ps1MixRequestTheme(&s, 21) == PS1_MIX_ACT_NONE);
		CHECK(s.pendingTheme == 21);
		int startedTheme = -1;
		for (int i = 0; i < 5; ++i) {
			const PS1MixAction a = step(s);
			if (a.action == PS1_MIX_ACT_START) startedTheme = a.theme;
		}
		CHECK(startedTheme == 21);
		CHECK(s.fades == 1);
		const uint32_t before = s.fades;
		for (int i = 0; i < 20; ++i) step(s);
		CHECK(s.fades == before);
	}

	// --- voix SFX : 23 voix libres, puis vol de la plus proche de la fin ---
	{
		PS1MixState s;
		ps1MixInit(&s);
		for (int v = 0; v < PS1_MIX_SFX_VOICES; ++v) {
			CHECK(ps1MixAllocSfx(&s, 20) == v);
		}
		CHECK(s.sfxSteals == 0);
		CHECK(ps1MixAllocSfx(&s, 20) == 0);
		CHECK(s.sfxSteals == 1);
		CHECK(s.sfxStarts == PS1_MIX_SFX_VOICES + 1);
		for (int i = 0; i < 19; ++i) CHECK(step(s).expired == 0u);
		const PS1MixAction last = step(s);
		CHECK(last.expired == (1u << PS1_MIX_SFX_VOICES) - 1u);
		CHECK(s.hold == 0);
	}

	// --- priorite SFX : le demarrage du flux ne remute pas les voix actives ---
	{
		PS1MixState s;
		ps1MixInit(&s);
		CHECK(ps1MixAllocSfx(&s, 30) == 0);
		CHECK(ps1MixAllocSfx(&s, 30) == 1);
		const uint32_t mask = ps1MixNeutralizeMask(&s);
		CHECK((mask & ((1u << 0) | (1u << 1))) == 0u);
		CHECK((mask & (1u << 2)) != 0u);
		CHECK((mask & ~(1u << PS1_MIX_SFX_VOICES)) == mask);
		CHECK(s.sfxPreserved == 2);
		// Le pilote n'appelle cette fonction qu'une fois (s_voicesNeutralized).
	}

	// --- ducking : attaque, maintien, release, sans toucher au fondu ---
	{
		PS1MixState s;
		ps1MixInit(&s);
		step(s);
		const uint16_t full = step(s).volume;
		CHECK(full == PS1_MIX_VOL_MAX);
		CHECK(ps1MixAllocSfx(&s, 20) == 0);
		for (int i = 0; i < PS1_MIX_DUCK_STEPS; ++i) step(s);
		CHECK(s.duck == PS1_MIX_DUCK_STEPS);
		const uint16_t ducked = step(s).volume;
		CHECK(ducked < full);
		CHECK(ducked == (PS1_MIX_VOL_MAX * PS1_MIX_DUCK_MIN) / PS1_MIX_DUCK_DEN);
		for (int i = 0; i < 10; ++i) step(s);
		CHECK(s.hold == 0);
		CHECK(s.duck < PS1_MIX_DUCK_STEPS);
		for (int i = 0; i < PS1_MIX_DUCK_STEPS; ++i) step(s);
		CHECK(s.duck == 0);
		CHECK(step(s).volume == full);
	}

	// --- fondu et ducking se combinent sans depasser la plage 16 bits ---
	{
		PS1MixState s;
		ps1MixInit(&s);
		ps1MixRequestTheme(&s, 17);
		ps1MixAllocSfx(&s, 15);
		for (int i = 0; i < PS1_MIX_FADE_FRAMES + PS1_MIX_DUCK_STEPS + 20; ++i) {
			const PS1MixAction a = step(s);
			CHECK(a.volume <= PS1_MIX_VOL_MAX);
		}
		CHECK(s.level == PS1_MIX_FADE_FRAMES);
	}

	// --- duree et annulation ---
	{
		PS1MixState s;
		ps1MixInit(&s);
		const int frames = ps1MixSfxFrames(4160);
		CHECK(frames > 0 && frames < 600);
		CHECK(ps1MixSfxFrames(0) == PS1_MIX_SFX_TAIL);
		CHECK(ps1MixSfxFrames(0xffffffffu) == 600);
		ps1MixRequestTheme(&s, 1);
		step(s);
		ps1MixRequestTheme(&s, 17);
		CHECK(s.fade == PS1_FADE_OUT);
		ps1MixCancel(&s);
		CHECK(s.fade == PS1_FADE_IDLE);
		CHECK(s.level == PS1_MIX_FADE_FRAMES);
		CHECK(s.activeTheme == -1);
		CHECK(s.pendingTheme == -1);
	}

	std::printf("PS1_MIXER_LOGIC: %d/%d checks PASS\n", g_checks - g_fail, g_checks);
	return g_fail == 0 ? 0 : 1;
}
