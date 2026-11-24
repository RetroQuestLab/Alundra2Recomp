#pragma once
/*
 * Animation d'apparence de l'ogre de Kindra — logique POD partagee entre le
 * pilote MIPS (interaction_ps1.cpp) et le test hote (ps1_enemy_logic_test.cpp).
 *
 * Ce lot est purement visuel : aucun degat, deplacement, HP ni etat de quete
 * n'est modifie ici. Le seul signal externe est bossDown, qui gele la sequence
 * des que le boss est vaincu. Aucune allocation, STL ou flottant.
 */
#include <stdint.h>

#define PS1_ENEMY_ANIM_FRAMES 6       /* frames A1 reelles 0..5 de ogre.a1s */
#define PS1_ENEMY_TICKS_PER_FRAME 12  /* 12 images a 60 Hz = 5 images/s */

struct PS1EnemyAnimState {
	uint16_t frame;   /* index dans enemy_anim.dat */
	uint16_t tick;    /* 0 .. PS1_ENEMY_TICKS_PER_FRAME-1 */
	uint16_t frames;  /* nombre de frames valides, 1 .. PS1_ENEMY_ANIM_FRAMES */
	uint16_t frozen;  /* 1 des que bossDown != 0 */
	uint32_t steps;   /* nombre total d'avances (diagnostic sonde) */
};

inline void enemyAnimInit(PS1EnemyAnimState* s) {
	s->frame = 0;
	s->tick = 0;
	s->frames = PS1_ENEMY_ANIM_FRAMES;
	s->frozen = 0;
	s->steps = 0;
}

inline void enemyAnimReset(PS1EnemyAnimState* s) {
	s->frame = 0;
	s->tick = 0;
	s->frozen = 0;
	s->steps = 0;
}

inline void enemyAnimSetFrames(PS1EnemyAnimState* s, int count) {
	if (count < 1) count = 1;
	if (count > PS1_ENEMY_ANIM_FRAMES) count = PS1_ENEMY_ANIM_FRAMES;
	s->frames = (uint16_t)count;
	if (s->frame >= s->frames) s->frame = (uint16_t)(s->frames - 1);
	s->tick = 0;
}

inline void enemyAnimPump(PS1EnemyAnimState* s, int bossDown) {
	s->frozen = bossDown ? 1u : 0u;
	if (s->frozen) return;
	++s->tick;
	if (s->tick < PS1_ENEMY_TICKS_PER_FRAME) return;
	s->tick = 0;
	++s->frame;
	if (s->frame >= s->frames) s->frame = 0;
	++s->steps;
}

inline int enemyAnimFrame(const PS1EnemyAnimState* s) { return (int)s->frame; }
