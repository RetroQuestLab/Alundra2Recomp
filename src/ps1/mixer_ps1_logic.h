#pragma once
/*
 * Mixage PS1 — fondus musicaux et priorite des bruitages, sans SDK ni STL.
 *
 * Ce header est partage entre le pilote MIPS (audio_ps1.cpp) et le test hote
 * (tools/ps1_mixer_logic_test.cpp). Aucune fonction ne touche au materiel :
 * l'appelant applique `volume` sur la voix musique, demarre le flux demande par
 * `action`, et remute les voix SFX indiquees par `expired`.
 *
 * Le fondu reproduit la semantique de src/audio.cpp : descente a zero,
 * changement de piste a zero, remontee. 30 images a 60 Hz = 0,5 s dans
 * chaque sens. Le ducking ne s'applique qu'a la voix musique.
 */
#include <stdint.h>

#define PS1_MIX_SFX_VOICES  23   /* voix 0..22 ; la voix 23 est le flux CD */
#define PS1_MIX_FADE_FRAMES 30   /* 0,5 s a 60 Hz */
#define PS1_MIX_DUCK_DEN    10
#define PS1_MIX_DUCK_MIN    6    /* la musique tombe a 60 % sous un bruitage */
#define PS1_MIX_DUCK_STEPS  10   /* attaque et release : 10 images */
#define PS1_MIX_SFX_TAIL    4    /* 4 images de maintien apres la fin du clip */
#define PS1_MIX_VOL_MAX     0x3fffu

#define PS1_FADE_IDLE 0
#define PS1_FADE_OUT  1
#define PS1_FADE_IN   2

#define PS1_MIX_ACT_NONE  0
#define PS1_MIX_ACT_START 1

struct PS1MixAction {
	int action;        /* PS1_MIX_ACT_* */
	int theme;         /* theme a demarrer quand action == START */
	uint32_t expired;  /* masque 1u << voix des clips SFX termines */
	uint16_t volume;   /* volume SPU a appliquer a la voix musique */
};

struct PS1MixState {
	int fade;                              /* PS1_FADE_* */
	int level;                             /* 0..PS1_MIX_FADE_FRAMES */
	int activeTheme;                       /* -1 si aucun */
	int pendingTheme;                      /* -1 si aucun */
	int duck;                              /* 0..PS1_MIX_DUCK_STEPS */
	int hold;                              /* images restantes de priorite SFX */
	int nextVoice;                         /* curseur round-robin SFX */
	uint16_t sfxFrames[PS1_MIX_SFX_VOICES];/* duree restante par voix */
	uint32_t fades;                        /* changements de piste a zero */
	uint32_t fadesIn;                      /* remontees terminees */
	uint32_t sfxStarts;
	uint32_t sfxSteals;
	uint32_t sfxPreserved;                 /* demarrages de flux sans couper un SFX */
};

inline void ps1MixInit(PS1MixState* s) {
	s->fade = PS1_FADE_IDLE;
	s->level = PS1_MIX_FADE_FRAMES;
	s->activeTheme = -1;
	s->pendingTheme = -1;
	s->duck = 0;
	s->hold = 0;
	s->nextVoice = 0;
	for (int i = 0; i < PS1_MIX_SFX_VOICES; ++i) s->sfxFrames[i] = 0;
	s->fades = 0;
	s->fadesIn = 0;
	s->sfxStarts = 0;
	s->sfxSteals = 0;
	s->sfxPreserved = 0;
}

/* Renvoie PS1_MIX_ACT_START si le flux doit etre demarre immediatement
 * (premier theme, aucun flux en cours). Sinon le changement est programme
 * dans la machine a etats et sera execute par ps1MixStep a volume zero. */
inline int ps1MixRequestTheme(PS1MixState* s, int theme) {
	if (theme < 0) return PS1_MIX_ACT_NONE;
	if (s->activeTheme == theme && s->pendingTheme < 0 &&
	    s->fade == PS1_FADE_IDLE)
		return PS1_MIX_ACT_NONE;
	if (s->activeTheme < 0) {
		s->activeTheme = theme;
		s->pendingTheme = -1;
		s->fade = PS1_FADE_IN;
		s->level = 0;
		return PS1_MIX_ACT_START;
	}
	if (s->fade == PS1_FADE_OUT) {
		s->pendingTheme = theme;  /* derniere demande gagne */
		return PS1_MIX_ACT_NONE;
	}
	s->pendingTheme = theme;
	s->fade = PS1_FADE_OUT;
	return PS1_MIX_ACT_NONE;
}

/* Arret explicite : annule tout fondu pendentif et remet le volume a plein
 * pour le jingle resident (voix 23). */
inline void ps1MixCancel(PS1MixState* s) {
	s->fade = PS1_FADE_IDLE;
	s->level = PS1_MIX_FADE_FRAMES;
	s->activeTheme = -1;
	s->pendingTheme = -1;
}

/* Duree d'un clip PCM_ADPCM 22050 Hz en images 60 Hz, avec queue. */
inline int ps1MixSfxFrames(uint32_t clipBytes) {
	const long long samples = static_cast<long long>(clipBytes / 16u) * 28;
	long long frames = (samples * 60) / 22050 + PS1_MIX_SFX_TAIL;
	if (frames > 600) frames = 600;
	return static_cast<int>(frames);
}

/* Allocation d'une voix SFX : voix libre en priorite, sinon vol de la voix
 * qui lui reste le moins de temps. La voix 23 n'est jamais utilisee. */
inline int ps1MixAllocSfx(PS1MixState* s, int frames) {
	if (frames <= 0) return -1;
	int chosen = -1;
	for (int n = 0; n < PS1_MIX_SFX_VOICES; ++n) {
		const int v = (s->nextVoice + n) % PS1_MIX_SFX_VOICES;
		if (s->sfxFrames[v] == 0) {
			chosen = v;
			s->nextVoice = (v + 1) % PS1_MIX_SFX_VOICES;
			break;
		}
	}
	if (chosen < 0) {
		int best = 0x7fff;
		for (int v = 0; v < PS1_MIX_SFX_VOICES; ++v) {
			if (s->sfxFrames[v] < best) {
				best = s->sfxFrames[v];
				chosen = v;
			}
		}
		++s->sfxSteals;
		s->nextVoice = (chosen + 1) % PS1_MIX_SFX_VOICES;
	}
	s->sfxFrames[chosen] = static_cast<uint16_t>(frames);
	++s->sfxStarts;
	return chosen;
}

/* Masque des voix SFX que le demarrage du flux peut remettre au bloc muet.
 * Les voix encore actives en sont exclues : c'est la priorite SFX sur le
 * streaming. Chaque voix preservee est comptee pour la sonde. */
inline uint32_t ps1MixNeutralizeMask(PS1MixState* s) {
	uint32_t mask = (1u << PS1_MIX_SFX_VOICES) - 1u;
	for (int ch = 0; ch < PS1_MIX_SFX_VOICES; ++ch) {
		if (s->sfxFrames[ch] == 0) continue;
		mask &= ~(1u << ch);
		++s->sfxPreserved;
	}
	return mask;
}

inline void ps1MixStep(PS1MixState* s, PS1MixAction* out) {
	out->action = PS1_MIX_ACT_NONE;
	out->theme = s->pendingTheme;
	out->expired = 0u;
	out->volume = 0u;

	uint32_t expired = 0u;
	int hold = 0;
	for (int v = 0; v < PS1_MIX_SFX_VOICES; ++v) {
		uint16_t left = s->sfxFrames[v];
		if (left > 0) {
			--left;
			s->sfxFrames[v] = left;
			if (left == 0) expired |= (1u << v);
			else if (left > static_cast<uint16_t>(hold)) hold = left;
		}
	}
	s->hold = hold;

	const int duckTarget = hold > 0 ? PS1_MIX_DUCK_STEPS : 0;
	if (s->duck < duckTarget) ++s->duck;
	else if (s->duck > duckTarget) --s->duck;

	if (s->fade == PS1_FADE_OUT) {
		if (s->level > 0) --s->level;
		if (s->level == 0 && s->pendingTheme >= 0) {
			out->action = PS1_MIX_ACT_START;
			out->theme = s->pendingTheme;
			s->activeTheme = s->pendingTheme;
			s->pendingTheme = -1;
			s->fade = PS1_FADE_IN;
			++s->fades;
		}
	} else if (s->fade == PS1_FADE_IN) {
		if (s->level < PS1_MIX_FADE_FRAMES) ++s->level;
		if (s->level == PS1_MIX_FADE_FRAMES) {
			s->fade = PS1_FADE_IDLE;
			++s->fadesIn;
		}
	}

	const uint32_t fadeVol =
	    (static_cast<uint32_t>(PS1_MIX_VOL_MAX) *
	     static_cast<uint32_t>(s->level)) / PS1_MIX_FADE_FRAMES;
	const int duckNum = PS1_MIX_DUCK_DEN -
	                    (s->duck * (PS1_MIX_DUCK_DEN - PS1_MIX_DUCK_MIN)) /
	                        PS1_MIX_DUCK_STEPS;
	out->volume = static_cast<uint16_t>((fadeVol * static_cast<uint32_t>(duckNum)) /
	                                    PS1_MIX_DUCK_DEN);
	out->expired = expired;
}
