#pragma once
/*
 * Duel LOCAL avec l'ogre de Kindra — logique POD partagee entre le pilote MIPS
 * (interaction_ps1.cpp) et le test hote (ps1_ogre_duel_logic_test.cpp).
 *
 * Cette logique est pure : elle ne modifie NI QuestState, NI bossDown, NI
 * les dialogues, NI les sauvegardes. Le pilote MIPS appelle questOgreDefeated()
 * uniquement sur la transition reelle vers DOWN (ogreDuelJustWon) ; CROIX ne
 * peut plus contourner le combat (ogreDuelInteractAllowed). Aucun degat au
 * heros, aucun deplacement, aucune allocation, STL ou flottant.
 */
#include <stdint.h>

#define OGRE_DUEL_MAX_HP             14
#define OGRE_DUEL_HIT_DAMAGE          2
#define OGRE_DUEL_HIT_COOLDOWN       18
#define OGRE_DUEL_STUN_TICKS         24
#define OGRE_DUEL_FLASH_TICKS        14
#define OGRE_DUEL_ABSORB_FLASH_TICKS  8
#define OGRE_DUEL_GUARD_TICKS       120
#define OGRE_DUEL_OPEN_TICKS         60

#define OGRE_DUEL_GUARD    0
#define OGRE_DUEL_OPEN     1
#define OGRE_DUEL_STUNNED  2
#define OGRE_DUEL_DOWN     3

/* Identifiants A2 deja presents dans la banque VAG : aucun nouvel asset. */
#define OGRE_DUEL_SFX_NONE  0
#define OGRE_DUEL_SFX_GUARD 20  /* bloque */
#define OGRE_DUEL_SFX_HIT   66  /* coup_fort */
#define OGRE_DUEL_SFX_DOWN  41  /* ogre_mort */

struct PS1OgreDuelState {
	uint16_t hp;         /* PV locaux, 0 = a terre (etat DOWN) */
	uint16_t cooldown;   /* ticks avant un nouveau coup */
	uint16_t stun;       /* ticks d'etourdissement restants */
	uint16_t flash;      /* ticks de flash restants, 8 = absorbe, 14 = porte */
	uint16_t phaseTimer; /* ticks dans la phase GARDE ou OUVERT */
	uint16_t state;      /* OGRE_DUEL_* */
	uint16_t hits;       /* coups ports (diagnostic sonde) */
	uint16_t prevSquare; /* etat CARRE de la frame precedente (front montant) */
	uint32_t steps;      /* avances effectuees (diagnostic sonde) */
};

inline void ogreDuelInit(PS1OgreDuelState* s) {
	s->hp = OGRE_DUEL_MAX_HP;
	s->cooldown = 0;
	s->stun = 0;
	s->flash = 0;
	s->phaseTimer = 0;
	s->state = OGRE_DUEL_GUARD;
	s->hits = 0;
	s->prevSquare = 0;
	s->steps = 0;
}

inline void ogreDuelReset(PS1OgreDuelState* s) { ogreDuelInit(s); }

inline int ogreDuelVulnerable(const PS1OgreDuelState* s) {
	return s->state == OGRE_DUEL_OPEN && s->hp != 0 ? 1 : 0;
}

inline int ogreDuelStunned(const PS1OgreDuelState* s) {
	return s->state == OGRE_DUEL_STUNNED || s->state == OGRE_DUEL_DOWN ? 1 : 0;
}

/* Une avance par image presentee. bossDown gele tout, y compris le flash. */
inline void ogreDuelStep(PS1OgreDuelState* s, int bossDown) {
	if (bossDown || s->state == OGRE_DUEL_DOWN) return;
	if (s->cooldown != 0) --s->cooldown;
	if (s->flash != 0) --s->flash;
	if (s->stun != 0) {
		--s->stun;
		if (s->stun == 0) {
			s->state = OGRE_DUEL_GUARD;
			s->phaseTimer = 0;
		}
	} else {
		++s->phaseTimer;
		if (s->state == OGRE_DUEL_GUARD) {
			if (s->phaseTimer >= OGRE_DUEL_GUARD_TICKS) {
				s->state = OGRE_DUEL_OPEN;
				s->phaseTimer = 0;
			}
		} else if (s->state == OGRE_DUEL_OPEN) {
			if (s->phaseTimer >= OGRE_DUEL_OPEN_TICKS) {
				s->state = OGRE_DUEL_GUARD;
				s->phaseTimer = 0;
			}
		}
	}
	++s->steps;
}

/* Front montant de CARRE. Le front est memorise meme hors de portee pour
 * qu'un bouton maintenu ne frappe pas en entrant dans la zone. */
inline int ogreDuelTryHit(PS1OgreDuelState* s, int bossDown, int squareDown, int inRange) {
	if (bossDown || s->state == OGRE_DUEL_DOWN) return 0;
	const int pressed = (squareDown && !s->prevSquare) ? 1 : 0;
	s->prevSquare = squareDown ? 1u : 0u;
	if (!pressed || !inRange || s->cooldown != 0) return 0;
	if (ogreDuelVulnerable(s)) {
		int hp = (int)s->hp - OGRE_DUEL_HIT_DAMAGE;
		if (hp < 0) hp = 0;
		s->hp = (uint16_t)hp;
		s->stun = OGRE_DUEL_STUN_TICKS;
		s->flash = OGRE_DUEL_FLASH_TICKS;
		++s->hits;
		s->phaseTimer = 0;
		if (hp > 0) {
			s->state = OGRE_DUEL_STUNNED;
			s->cooldown = OGRE_DUEL_HIT_COOLDOWN;
		} else {
			// DOWN gele tous les timers : les remettre a zero avant d'y entrer,
			// sinon le flash et le voile d'etourdissement resteraient a l'ecran.
			s->state = OGRE_DUEL_DOWN;
			s->flash = 0;
			s->stun = 0;
			s->cooldown = 0;
		}
	} else {
		s->flash = OGRE_DUEL_ABSORB_FLASH_TICKS;
	}
	return 1;
}

/* Choix du bruitage a partir d'une copie avant/apres. Pure et sans effet de
 * bord : le pilote MIPS peut appeler l'API audio seulement si le resultat est
 * different de OGRE_DUEL_SFX_NONE. La taille du POD reste 20 octets. */
inline int ogreDuelSfxForHit(const PS1OgreDuelState* before,
                             const PS1OgreDuelState* after, int accepted) {
	if (!accepted) return OGRE_DUEL_SFX_NONE;
	if (after->hp == 0) return OGRE_DUEL_SFX_DOWN;
	if (after->hits != before->hits) return OGRE_DUEL_SFX_HIT;
	return OGRE_DUEL_SFX_GUARD;
}

/* Transition exacte vers DOWN. Le pilote MIPS l'utilise pour declencher
 * questOgreDefeated() une seule fois : la consequence de la quete suit la
 * victoire reelle au COMBAT, jamais une simple interaction CROIX. Les deux
 * etats sont lus avant/apres le meme ogreDuelTryHit(). */
inline int ogreDuelJustWon(const PS1OgreDuelState* before,
                           const PS1OgreDuelState* after, int accepted) {
	return accepted && before != after && before->state != OGRE_DUEL_DOWN &&
	       after->state == OGRE_DUEL_DOWN ? 1 : 0;
}

/* CROIX sur l'acteur ogre est refusee tant que le duel n'est pas termine.
 * Apres DOWN, bossDown gele le POD et le dialogue de victoire s'ouvre. */
inline int ogreDuelInteractAllowed(const PS1OgreDuelState* s) {
	return s->state == OGRE_DUEL_DOWN ? 1 : 0;
}
