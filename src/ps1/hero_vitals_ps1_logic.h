#pragma once
/*
 * Constantes vitales LOCALES du heros PS1 — logique POD partagee entre le
 * pilote MIPS (interaction_ps1.cpp / boot.cpp) et le test hote.
 *
 * Ce systeme est parallele a la quete : il ne modifie NI QuestState, NI
 * bossDown, NI les dialogues, NI la sauvegarde. La seule source de degats est
 * l'ogre de la zone 013 en phase GARDE, a portee. Aucun deplacement, aucune
 * collision, aucune allocation, STL ou flottant.
 */
#include <stdint.h>

#define HERO_VITALS_MAX_HP                 10
#define HERO_VITALS_INVULN_TICKS           70
#define HERO_VITALS_RESPAWN_INVULN_TICKS   90
#define HERO_VITALS_ATTACK_PERIOD          60

struct PS1HeroVitalsState {
	uint16_t hp;         /* PV locaux, 0 = tombe (etat DOWN) */
	uint16_t invuln;     /* ticks d'invulnerabilite restants */
	uint16_t attackTimer;/* ticks avant une nouvelle attaque ogre */
	uint16_t hitsTaken;  /* coups recus (diagnostic sonde) */
	uint32_t steps;      /* avances effectuees (diagnostic sonde) */
	uint32_t reserved;   /* alignement POD 16 octets, toujours 0 */
};

inline void heroVitalsInit(PS1HeroVitalsState* s) {
	s->hp = HERO_VITALS_MAX_HP;
	s->invuln = 0;
	s->attackTimer = 0;
	s->hitsTaken = 0;
	s->steps = 0;
	s->reserved = 0;
}

inline void heroVitalsReset(PS1HeroVitalsState* s) { heroVitalsInit(s); }

/* Nouvelle partie apres Game Over : PV pleins et 90 ticks de grace. */
inline void heroVitalsRespawn(PS1HeroVitalsState* s) {
	heroVitalsInit(s);
	s->invuln = HERO_VITALS_RESPAWN_INVULN_TICKS;
}

/* Une avance par image presentee. Les compteurs s'arretent a zero. */
inline void heroVitalsTick(PS1HeroVitalsState* s) {
	if (s->invuln != 0) --s->invuln;
	if (s->attackTimer != 0) --s->attackTimer;
	++s->steps;
}

inline int heroVitalsDown(const PS1HeroVitalsState* s) {
	return s->hp == 0 ? 1 : 0;
}

/* Front d'attaque de l'ogre. Retourne 1 uniquement si le coup est encaisse :
 * le pilote peut alors jouer le SFX une seule fois. */
inline int heroVitalsTryHit(PS1HeroVitalsState* s, int damage, int ogreGuard,
                            int inRange) {
	if (damage <= 0 || ogreGuard == 0 || inRange == 0) return 0;
	if (s->invuln != 0 || s->attackTimer != 0 || s->hp == 0) return 0;
	const int hp = (int)s->hp - damage;
	s->hp = (uint16_t)(hp < 0 ? 0 : hp);
	s->invuln = HERO_VITALS_INVULN_TICKS;
	s->attackTimer = HERO_VITALS_ATTACK_PERIOD;
	++s->hitsTaken;
	return 1;
}
