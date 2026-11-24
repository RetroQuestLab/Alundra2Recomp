#pragma once
/*
 * Correspondance des gemmes de vie du HUD PS1 — logique POD partagee entre le
 * pilote MIPS et le test hote. Les valeurs sont celles de
 * src/main.cpp::drawHud (lignes 1075-1080) : une petite gemme = 1 PV, dix
 * petites gemmes en deux rangees de cinq, une grosse gemme par dizaine au-dela
 * de 10 PV. Aucune SDL, allocation ou etat global ici.
 */
#include <stdint.h>

/* Lecture little-endian d'un entete P1HU, sans dependance ni allocation. */
inline uint16_t hudBlobRead16(const uint8_t* p) {
	return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

/* Valide la structure du blob P1HU avant tout upload : magic, count exact,
 * dimensions positives, limites d'upload et taille totale exacte. La fonction
 * est pure, independante de pf.h et ne touche aucun peripherique. */
inline int hudBlobValidate(const uint8_t* data, unsigned size,
                           int maxStride, int maxUploadH) {
	if (data == nullptr || size < 8 || maxStride <= 0 || maxUploadH <= 0) return 0;
	if (data[0] != 'P' || data[1] != '1' || data[2] != 'H' || data[3] != 'U') return 0;
	if (hudBlobRead16(data + 4) != 15) return 0;
	unsigned cursor = 8;
	for (int i = 0; i < 15; ++i) {
		if (cursor + 16u > size) return 0;
		const uint8_t* frame = data + cursor;
		const unsigned w = hudBlobRead16(frame);
		const unsigned h = hudBlobRead16(frame + 2);
		const unsigned stride = hudBlobRead16(frame + 8);
		const unsigned uploadH = hudBlobRead16(frame + 10);
		/* stride en demi-mots VRAM (4 pixels 4bpp/unite, voir pad_rows) ; w <= 2*stride reste conservateur (capacite reelle 4*stride). */
		if (w == 0 || h == 0 || stride * 2u < w) return 0;
		if (stride > static_cast<unsigned>(maxStride)) return 0;
		if (uploadH < h || uploadH > static_cast<unsigned>(maxUploadH)) return 0;
		const unsigned long long end = static_cast<unsigned long long>(cursor) + 16u + 32u
		                               + static_cast<unsigned long long>(stride) * 2u * uploadH;
		if (end > size) return 0;
		cursor = static_cast<unsigned>(end);
	}
	return cursor == size ? 1 : 0;
}

inline int hudBigGemCount(int maxHp) {
	const int bounded = maxHp < 0 ? 0 : (maxHp > 99 ? 99 : maxHp);
	const int n = (bounded + 9) / 10 - 1;
	return n > 0 ? n : 0;
}

inline int hudBigGemFull(int hp, int maxHp, int k) {
	const int nBig = hudBigGemCount(maxHp);
	if (k < 0 || k >= nBig || hp <= 0) return 0;
	const int fullBig = (hp - 1) / 10 < nBig ? (hp - 1) / 10 : nBig;
	return k < fullBig ? 1 : 0;
}

inline int hudSmallFull(int hp, int maxHp, int i) {
	const int nBig = hudBigGemCount(maxHp);
	const int fullBig = hp > 0 ? ((hp - 1) / 10 < nBig ? (hp - 1) / 10 : nBig) : 0;
	const int small = hp - 10 * fullBig;
	return i >= 0 && i < 10 && i < small ? 1 : 0;
}

/* Suffixe "/MM" du HUD PC : deux chiffres avec zero initial, valeurs bornees
 * a 99 comme src/main.cpp::drawHud (digits(x, y, h.maxHp, 2)). */
inline int hudMaxDigit(int maxHp, int place) {
	if (place != 0 && place != 1) return 0;
	int value = maxHp < 0 ? 0 : maxHp;
	if (value > 99) value = 99;
	return place == 0 ? value / 10 : value % 10;
}

inline int hudSmallGemX(int i) { return 120 + 8 * (i % 5); }
inline int hudSmallGemY(int i) { return 16 + 8 * (i / 5); }
