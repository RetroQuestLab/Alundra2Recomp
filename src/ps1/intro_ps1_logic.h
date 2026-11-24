#pragma once
/*
 * Logique d'introduction PS1, sans STL ni SDK (partagee avec le test hote).
 * Progression uniquement indexee sur les frames : aucune ressource externe,
 * donc aucun risque d'attente infinie. START fixe la fin immediatement.
 * L'introduction ne modifie aucun etat de partie : vue ou passee, l'entree dans
 * le jeu est identique.
 */
#include <stdint.h>

#include "../platform/pf.h"

#define INTRO_SCENE_SKY    0
#define INTRO_SCENE_SHIP   1
#define INTRO_SCENE_FALL   2
#define INTRO_SCENE_BEACH  3
#define INTRO_SCENE_DONE   4

/* v0.8.18 : les decors changent exactement quand le panneau de texte change.
 * INTRO_TOTAL_FRAMES / kIntroPanelCount = 2160 / 10 = 216 frames par panneau. */
#define INTRO_SKY_END   648   /* 3 panneaux : fin de la narration Varuna / mers */
#define INTRO_SHIP_END  1728  /* 8 panneaux : fin des panneaux "navire" */
#define INTRO_FALL_END  1944  /* 9 panneaux : fin du panneau "chute" */
/* 2160 frames a 60 Hz = 36 secondes, dans la plage demandee 30-45 s. */
#define INTRO_TOTAL_FRAMES 2160

struct IntroState {
	int frame;
	int scene;
	int skipped;
	uint32_t prevPad;
};

inline void introInit(IntroState* s) {
	s->frame = 0;
	s->scene = INTRO_SCENE_SKY;
	s->skipped = 0;
	s->prevPad = 0;
}

inline int introDone(const IntroState* s) {
	return s->scene == INTRO_SCENE_DONE ? 1 : 0;
}

inline void introStep(IntroState* s, uint32_t pad) {
	if (introDone(s)) return;

	const uint32_t pressed = pad & ~s->prevPad;
	s->prevPad = pad;

	if (pressed & PF_BTN_START) {
		s->skipped = 1;
		s->frame = INTRO_TOTAL_FRAMES;
	} else if (s->frame < INTRO_TOTAL_FRAMES) {
		++s->frame;
	}

	if (s->frame >= INTRO_TOTAL_FRAMES) {
		s->scene = INTRO_SCENE_DONE;
	} else if (s->frame < INTRO_SKY_END) {
		s->scene = INTRO_SCENE_SKY;
	} else if (s->frame < INTRO_SHIP_END) {
		s->scene = INTRO_SCENE_SHIP;
	} else if (s->frame < INTRO_FALL_END) {
		s->scene = INTRO_SCENE_FALL;
	} else {
		s->scene = INTRO_SCENE_BEACH;
	}
}
