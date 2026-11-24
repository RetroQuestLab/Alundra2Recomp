#pragma once
/*
 * Fenetre de dialogue A1 partagee (PS1 natif).
 *
 * Geometrie verrouillee par les captures A1 et deja validée pour le dialogue :
 * bx=16, by=170, bw=286, bh=54. Le point sensible est le CORPS des rouleaux :
 * il vient des 40 dernieres lignes de la colonne source (srcY = kRollH - tail),
 * pas des lignes 16..53. Une divergence de cet offset recouvre le texte avec
 * des rows etrangeres de WIND.TX ; c'est la cause racine du "Voyons" lu
 * "Oyons" dans la premiere version de l'introduction.
 *
 * Aucun STL, aucun etat global : uniquement des primitives GPU POD.
 */
#include "../platform/pf.h"

namespace a1window {

constexpr int kRollW = 24;
constexpr int kTopH = 8, kBottomH = 8;
constexpr int kFillX = 16, kFillY = 0, kFillW = 80, kFillH = 40;
constexpr int kEdgeX = 120, kTopY = 0, kEdgeW = 80, kBottomY = 32;
constexpr int kCapLeftX = 224, kCapLeftY = 0;
constexpr int kCapRightX = 224, kCapRightY = 24;
constexpr int kCapW = 24, kCapH = 16;
constexpr int kRollLeftX = 96, kRollRightX = 200;
constexpr int kRollH = 40, kRollMidY = 16, kRollMidH = 8;

inline int minInt(int a, int b) { return a < b ? a : b; }

inline void windRect(int dx, int dy, int dw, int dh,
                     int srcX, int srcY, int bank, int z) {
	if (dw <= 0 || dh <= 0) return;
	pfDrawSprite16(dx, dy,
	               PF_WIND_X + srcX / 4, PF_WIND_Y + srcY,
	               dw, dh,
	               PF_WIND_CLUT_X, PF_WIND_CLUT_Y + bank,
	               z);
}

inline void draw(int bx, int by, int bw, int bh, int bank, int z) {
	const int cx = bx + kRollW;
	const int cw = bw - 2 * kRollW;
	const int fillY = by + kTopH;
	const int fillH = bh - kTopH - kBottomH;

	for (int y = 0; y < fillH; y += kFillH) {
		for (int x = 0; x < cw; x += kFillW) {
			windRect(cx + x, fillY + y,
			         minInt(kFillW, cw - x), minInt(kFillH, fillH - y),
			         kFillX, kFillY, bank, z);
		}
	}
	for (int x = 0; x < cw; x += kEdgeW) {
		windRect(cx + x, by, minInt(kEdgeW, cw - x), kTopH,
		         kEdgeX, kTopY, bank, z);
		windRect(cx + x, by + bh - kBottomH, minInt(kEdgeW, cw - x), kBottomH,
		         kEdgeX, kBottomY, bank, z);
	}

	const int capH = minInt(kCapH, bh);
	const int body = bh - capH;
	struct Roll {
		int capX, capY, colX, dstX;
	};
	const Roll rolls[2] = {
		{kCapLeftX,  kCapLeftY,  kRollLeftX,  bx},
		{kCapRightX, kCapRightY, kRollRightX, bx + bw - kRollW},
	};
	for (int i = 0; i < 2; ++i) {
		const Roll& roll = rolls[i];
		windRect(roll.dstX, by, kRollW, capH,
		         roll.capX, roll.capY, bank, z);
		if (body <= 0) continue;
		const int tail = minInt(body, kRollH);
		// LIGNE CRITIQUE : les 38 dernieres lignes de la colonne du rouleau.
		windRect(roll.dstX, by + bh - tail, kRollW, tail,
		         roll.colX, kRollH - tail, bank, z);
		for (int y = 0; y < body - tail; y += kRollMidH) {
			windRect(roll.dstX, by + capH + y, kRollW,
			         minInt(kRollMidH, body - tail - y),
			         roll.colX, kRollMidY, bank, z);
		}
	}
}

}  // namespace a1window
