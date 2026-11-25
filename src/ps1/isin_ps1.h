#pragma once
/*
 * Sinus entier pour la cible PS1 (R3000A sans FPU) : table calculee a la
 * COMPILATION (constexpr), aucun flottant a l'execution.
 * isin4096(p) : p en 1/4096 de tour, resultat dans [-4096, 4096].
 */
#include <stdint.h>

namespace isin_detail {
constexpr double kPi = 3.14159265358979323846;
constexpr double sinSeries(double x) {
	// x dans [-pi, pi] : serie de Taylor a 9 termes, largement suffisante.
	double term = x, sum = x;
	for (int n = 1; n < 10; ++n) {
		term *= -x * x / double((2 * n) * (2 * n + 1));
		sum += term;
	}
	return sum;
}
struct Table {
	int16_t v[1025];
	constexpr Table() : v() {
		for (int i = 0; i <= 1024; ++i) {
			const double x = (double(i) / 4096.0) * 2.0 * kPi;  // quart de tour
			const double s = sinSeries(x);
			v[i] = int16_t(s * 4096.0 + (s >= 0 ? 0.5 : -0.5));
		}
	}
};
constexpr Table kTable{};
}  // namespace isin_detail

inline int isin4096(int p) {
	p &= 4095;
	if (p <= 1024) return isin_detail::kTable.v[p];
	if (p <= 2048) return isin_detail::kTable.v[2048 - p];
	if (p <= 3072) return -isin_detail::kTable.v[p - 2048];
	return -isin_detail::kTable.v[4096 - p];
}

/* sin(t * w + phi) avec t = tick/60 s, w en milliradians/s, phi en 1/4096 tour.
 * 2*pi*60*1000 / 4096 ~= 92.04 : un tick vaut w/92 unites de phase. */
inline int isinTick(int tick, int wMilli, int phi4096 = 0) {
	// 32 bits : tick < 20 000 et w < 20 000 mrad/s restent sous 2^31 (pas de
	// division 64 bits logicielle, couteuse sur le R3000A).
	return isin4096(tick * wMilli / 92 + phi4096);
}

/* Hachage PC (src/title.cpp::hash01) en entier : fraction sur 16 bits. */
inline uint32_t hash16(unsigned n) {
	n = (n ^ 61u) ^ (n >> 16);
	n *= 9u;
	n ^= n >> 4;
	n *= 0x27d4eb2du;
	n ^= n >> 15;
	return (n & 0xFFFFFFu) >> 8;
}
