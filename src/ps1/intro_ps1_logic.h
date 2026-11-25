#pragma once
/*
 * Logique d'introduction PS1 (partagee avec le test hote), calquee sur le
 * prologue PC (src/title.cpp : enterCard / frontTick / drawPrologue) :
 *   - une carte par replique de [prologue] ;
 *   - fondu d'entree FADE ticks, texte tape a 0,55 caractere par tick ;
 *   - CROIX complete le texte, puis passe a la carte suivante (fondu FADE) ;
 *   - sans appui, la carte part seule 200 ticks apres la fin de la frappe ;
 *   - START passe toute l'introduction (le PC fond au noir, la PS1 sort de
 *     suite : les sondes existantes en dependent).
 * L'introduction ne modifie aucun etat de partie.
 */
#include <stdint.h>

#include "../platform/pf.h"

#define INTRO_SCENE_ZONE   0  /* carte "042", "018", "026" : vraie zone en travelling */
#define INTRO_SCENE_SKY    1  /* ciel de nuit seul */
#define INTRO_SCENE_SHIP   2  /* ciel de nuit + navire volant ("navire") */
#define INTRO_SCENE_FALL   3  /* chute de Flint ("chute") */
#define INTRO_SCENE_DONE   4

#define INTRO_FADE      50  /* src/title.cpp : FADE */
#define INTRO_AUTO_WAIT 200 /* depart automatique apres la frappe */

struct IntroCardMeta {
	uint8_t scene;
	uint16_t len;  /* longueur du texte replie (octets Latin-1) */
};

/* `frame` reste le PREMIER champ : d'anciennes sondes lisent g_intro.frame. */
struct IntroState {
	int frame;     /* ticks ecoules depuis le debut de l'introduction */
	int scene;     /* INTRO_SCENE_* de la carte courante */
	int skipped;
	uint32_t prevPad;
	int card;
	int tick;      /* ticks dans la carte courante */
	int leaving;   /* > 0 : fondu de sortie en cours */
	int count;
	const IntroCardMeta* cards;
};

/* Caracteres visibles : (tick - FADE) * 0,55, borne a la longueur. */
inline int introShown(int tick, int len) {
	int n = (tick - INTRO_FADE) * 11 / 20;
	if (n < 0) n = 0;
	return n > len ? len : n;
}

inline int introTypedAt(int len) { return INTRO_FADE + (len * 20 + 10) / 11; }

inline void introInit(IntroState* s, const IntroCardMeta* cards, int count) {
	s->frame = 0;
	s->skipped = 0;
	s->prevPad = 0;
	s->card = 0;
	s->tick = 0;
	s->leaving = 0;
	s->count = count;
	s->cards = cards;
	s->scene = count > 0 ? cards[0].scene : INTRO_SCENE_DONE;
}

inline int introDone(const IntroState* s) { return s->scene == INTRO_SCENE_DONE ? 1 : 0; }

inline void introStep(IntroState* s, uint32_t pad) {
	if (introDone(s)) return;
	const uint32_t pressed = pad & ~s->prevPad;
	s->prevPad = pad;
	++s->frame;

	if (pressed & PF_BTN_START) {  // passer toute l'introduction
		s->skipped = 1;
		s->card = s->count;
		s->scene = INTRO_SCENE_DONE;
		return;
	}
	if (s->leaving > 0) {
		if (--s->leaving == 0) {
			++s->card;
			s->tick = 0;
			s->scene = s->card < s->count ? s->cards[s->card].scene : INTRO_SCENE_DONE;
		}
		return;
	}
	++s->tick;
	const int typedAt = introTypedAt(s->cards[s->card].len);
	if (pressed & PF_BTN_CROSS) {
		if (s->tick < typedAt) s->tick = typedAt;  // complete le texte
		else s->leaving = INTRO_FADE;
	} else if (s->tick > typedAt + INTRO_AUTO_WAIT) {
		s->leaving = INTRO_FADE;
	}
}
