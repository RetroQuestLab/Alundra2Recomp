/*
 * Test hote de la fenetre WIND partagee (dialogue + introduction).
 * Une seule variable est verrouillee : la source du corps des rouleaux.
 * Le bug detecte en capture etait srcY=16 avec h=38 (v+h=54) au lieu de
 * srcY=2 (v+h=40), ce qui recouvrait les glyphes avec des rows etrangeres.
 */
#include "../src/ps1/window_ps1.h"

#include <cstdio>

namespace {

struct Call {
	int x, y, w, h, u, srcY, z;
};

Call g_calls[64];
int g_count = 0;
bool g_overflow = false;

}  // namespace

extern "C" void pfDrawSprite16(int x, int y, int u, int v, int w, int h,
                               int clutX, int clutY, int z) {
	(void)clutX;
	(void)clutY;
	const int srcY = v - PF_WIND_Y;  // v est une ligne VRAM, pas une ligne source
	if (g_count < 64) {
		g_calls[g_count++] = Call{x, y, w, h, u, srcY, z};
	}
	// La colonne source du rouleau occupe 40 lignes (0..39) dans WIND.TX.
	if (x == 16 || x == 278) {
		if (srcY < 0 || srcY + h > 40) g_overflow = true;
	}
}

int main() {
	a1window::draw(16, 170, 286, 54, 0, 4);

	int checks = 0;
	int failures = 0;
	auto check = [&](bool ok, const char* label) {
		++checks;
		if (!ok) {
			++failures;
			std::printf("FAIL %s\n", label);
		}
	};

	check(g_count == 13, "13 quads pour une fenetre 286x54");
	check(!g_overflow, "aucun corps de rouleau hors des lignes 0..39");

	int leftTail = -1, rightTail = -1;
	for (int i = 0; i < g_count; ++i) {
		const Call& c = g_calls[i];
		if (c.y == 186 && c.h == 38) {
			if (c.x == 16) leftTail = c.srcY;
			if (c.x == 278) rightTail = c.srcY;
		}
	}
	check(leftTail == 2, "rouleau gauche : srcY = kRollH - tail = 2");
	check(rightTail == 2, "rouleau droit : srcY = kRollH - tail = 2");
	check(leftTail != 16 && rightTail != 16, "pas de source 16 sur les corps");

	int uOk = 0;
	for (int i = 0; i < g_count; ++i) {
		const Call& c = g_calls[i];
		if (c.u >= PF_WIND_X && c.u < PF_WIND_X + PF_WIND_W &&
		    c.srcY >= 0 && c.srcY + c.h <= PF_WIND_H) {
			++uOk;
		}
	}
	check(uOk == g_count, "tous les quads dans l atlas WIND");

	std::printf("PS1_WINDOW_LAYOUT: %d/%d checks %s\n",
	            checks - failures, checks, failures ? "FAIL" : "PASS");
	return failures ? 1 : 0;
}
