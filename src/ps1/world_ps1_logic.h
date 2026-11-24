#pragma once
/*
 * Passages A→Z subset pour la cible PS1 (011 ↔ 013 ↔ 019 ↔ 119), plus la
 * salle du trone (120) et son retour libre vers le manoir (086).
 * Table POD, aucun STL : compilee par le build MIPS et par le test hote.
 * Coordonnees en cases de 24x16 px, telles que data/warps.txt. Les cinq gates
 * de la chaine PC sont portes ici uniquement pour conditionner 086→120 ;
 * aucun passage existant de la route A→Z n'est modifie.
 */
#include <stdint.h>

#define QST_ZONE_VALLEE    26
#define QST_ZONE_MANOIR    86
#define QST_ZONE_TRONE    120

/* Bits de QuestState::gates (masque local PS1, aucun offset Lua déplace). */
#define QST_GATE_EPEE_SUPREME (1u << 0)
#define QST_GATE_LAMPE        (1u << 1)
#define QST_GATE_PRISME       (1u << 2)
#define QST_GATE_BOSS_MEDUSA  (1u << 3)
#define QST_GATE_CRISTAL      (1u << 4)
/* Drapeaux de progression ranges dans le meme mot sauvegarde (offset 64) :
 * aucun changement de format. Ils ne sont pas des gates de la chaine du trone. */
#define QST_FLAG_ORDRE        (1u << 8)   // ordres de Diaz trouves a Toroledo
#define QST_FLAG_VALLEE_VUE   (1u << 9)   // narration d'arrivee en vallee lue
#define QST_GATE_TRONE_MASK   (QST_GATE_EPEE_SUPREME | QST_GATE_LAMPE | \
                               QST_GATE_PRISME | QST_GATE_BOSS_MEDUSA | \
                               QST_GATE_CRISTAL)

struct WorldWarp {
	int fromZone;
	int x, y, w, h;
	int toZone;
	int toX, toY;
};

#define QST_WARP_COUNT 10

static const WorldWarp kWorldWarps[QST_WARP_COUNT] = {
	{11, 25, 45, 3, 1, 13, 8, 54},    // Paco -> Kindra
	{13, 8, 57, 5, 1, 11, 26, 47},    // Kindra -> Paco
	{13, 14, 50, 1, 1, 13, 41, 54},   // Kindra : entree -> clairiere de Finrod
	{13, 31, 54, 1, 1, 13, 14, 52},   // Kindra : clairiere -> entree
	{13, 41, 53, 1, 1, 19, 10, 12},   // Kindra -> Mont Epervier / epave
	{19, 13, 50, 1, 1, 13, 31, 54},   // Epave -> Kindra
	{19, 14, 52, 1, 1, 119, 25, 50},  // Epave -> Toroledo
	{119, 25, 49, 1, 1, 19, 14, 52},  // Toroledo -> Epave
	{120, 14, 52, 2, 1, 86, 27, 8},   // Trone -> Manoir : retour libre
	{26, 26, 52, 2, 1, 119, 26, 44},  // Vallee -> Toroledo : retour libre
};

/* data/warps.txt : 119 26 44 2 1 -> 026 26 52 haut epee_supreme. Hors de
 * kWorldWarps : il ne s'ouvre qu'avec l'epee remise par Jeehan. */
static const WorldWarp kWarpVallee = {119, 26, 44, 2, 1, QST_ZONE_VALLEE, 26, 52};

/* data/warps.txt : 086 26 8 2 1 -> 120 14 52 haut cristal. Volontairement
 * absent de kWorldWarps : il ne doit jamais etre franchi sans la chaine. */
static const WorldWarp kWarpTrone = {86, 26, 8, 2, 1, QST_ZONE_TRONE, 14, 52};

inline int worldTroneGatesComplete(uint32_t gates) {
	return (gates & QST_GATE_TRONE_MASK) == QST_GATE_TRONE_MASK;
}

inline int worldTryWarp120(uint32_t gates, int fromZone, int cx, int cy,
                           int* toZone, int* toX, int* toY) {
	if (fromZone != QST_ZONE_MANOIR || !worldTroneGatesComplete(gates)) return 0;
	if (cx < kWarpTrone.x || cy < kWarpTrone.y ||
	    cx >= kWarpTrone.x + kWarpTrone.w || cy >= kWarpTrone.y + kWarpTrone.h) return 0;
	if (toZone) *toZone = kWarpTrone.toZone;
	if (toX) *toX = kWarpTrone.toX;
	if (toY) *toY = kWarpTrone.toY;
	return 1;
}

inline int worldWarpAt(int zone, int cx, int cy, int* toZone, int* toX, int* toY) {
	for (int i = 0; i < QST_WARP_COUNT; ++i) {
		const WorldWarp& w = kWorldWarps[i];
		if (w.fromZone != zone) continue;
		if (cx < w.x || cy < w.y || cx >= w.x + w.w || cy >= w.y + w.h) continue;
		if (toZone) *toZone = w.toZone;
		if (toX) *toX = w.toX;
		if (toY) *toY = w.toY;
		return 1;
	}
	return 0;
}

/* Certaines arrivees tombent sur la case de depart d'un autre passage
 * (ex. Toroledo -> Epave arrive sur Epave -> Toroledo). Sans verrou, le heros
 * redeclenche le passage des qu'il change de frame. Le verrou est une simple
 * case (zone,x,y) : il saute des que le heros quitte cette case. */
struct WorldWarpState {
	int lockZone, lockX, lockY;
};

inline void worldWarpStateInit(WorldWarpState* s) {
	s->lockZone = -1;
	s->lockX = 0;
	s->lockY = 0;
}

inline void worldWarpStateLock(WorldWarpState* s, int zone, int x, int y) {
	s->lockZone = zone;
	s->lockX = x;
	s->lockY = y;
}

inline int worldTryWarp(WorldWarpState* s, int zone, int cx, int cy,
                        int* toZone, int* toX, int* toY) {
	if (s->lockZone == zone && s->lockX == cx && s->lockY == cy) return 0;
	s->lockZone = -1;  // case d'arrivee quittee : le passage est re-armee
	return worldWarpAt(zone, cx, cy, toZone, toX, toY);
}

/* Passages conditionnes par un gate de la route (hors trone), avec le meme
 * verrou d'arrivee que worldTryWarp. */
inline int worldTryWarpGated(WorldWarpState* s, uint32_t gates, int zone, int cx, int cy,
                             int* toZone, int* toX, int* toY) {
	if (s->lockZone == zone && s->lockX == cx && s->lockY == cy) return 0;
	const WorldWarp& w = kWarpVallee;
	if (zone != w.fromZone || !(gates & QST_GATE_EPEE_SUPREME)) return 0;
	if (cx < w.x || cy < w.y || cx >= w.x + w.w || cy >= w.y + w.h) return 0;
	s->lockZone = -1;
	if (toZone) *toZone = w.toZone;
	if (toX) *toX = w.toX;
	if (toY) *toY = w.toY;
	return 1;
}
