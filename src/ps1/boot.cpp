/*
 * Point d'entree PS1 : premiere tranche jouable de la zone 011.
 *
 * Le stub de diagnostic (carre mobile + barre) reste disponible avec
 * -DPS1_DIAG : il a servi a prouver le boot natif depuis le BIN/CUE.
 *
 * Aucun appel a AddSIO, printf, CdInit ou recherche CD-ROM ici : la preuve
 * d'execution est apportee par les points d'arret et les captures natives.
 */
#include "../platform/pf.h"

#include "hero_ps1.h"
#include "zone011.h"

#ifdef PS1_DIAG

namespace {
const uint32_t kBg = 0x1021u;
const uint32_t kSquare = 0x001Fu;
const uint32_t kBar = 0x03E0u;
const uint32_t kWhite = 0x7FFFu;
}  // namespace

int main(void) {
	pfInit();
	for (;;) {
		const uint32_t frame = pfFrame();
		pfDrawRect(0, 0, PF_W, PF_H, kBg, 15);
		const int x = (int)((frame / 8u) % (PF_W - 32u));
		pfDrawRect(x, 40, 32, 32, kSquare, 8);
		pfDrawRect(8, 200, (int)((frame % 240u) + 1u), 8, kBar, 4);
		if (pfPad() & PF_BTN_CROSS) pfDrawRect(140, 110, 40, 40, kWhite, 0);
		pfPresent();
	}
}

#else

namespace {

constexpr int kMapW = 52;
constexpr int kMapRows = 60;
constexpr int kMarginRows = 16;
constexpr int kTileW = 24;
constexpr int kTileH = 16;
constexpr uint32_t kBlack = 0x0000u;
constexpr uint32_t kHeart = 0x001Fu;  // BGR555 : rouge pur
constexpr uint32_t kHeartBg = 0x4208u;

inline int clampi(int v, int lo, int hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

}  // namespace

int main(void) {
	pfInit();
	zoneInit();
	heroInit();

	for (;;) {
		heroUpdate(pfPad(), pfFrame());

		int hx = 0;
		int hy = 0;
		heroPos(&hx, &hy);
		const int camX = clampi(hx - PF_W / 2, 0, kMapW * kTileW - PF_W);
		const int camY = clampi(hy - PF_H / 2, 0, (kMapRows + kMarginRows) * kTileH - PF_H);

		pfDrawRect(0, 0, PF_W, PF_H, kBlack, 63);
		zoneDraw(camX, camY, 0);
		heroDraw(camX, camY);

		// HUD : quatre coeurs pleins, en premier plan.
		for (int i = 0; i < 4; ++i) {
			pfDrawRect(8 + i * 12, 8, 10, 10, kHeartBg, 1);
			pfDrawRect(9 + i * 12, 9, 8, 8, kHeart, 0);
		}

		pfPresent();
	}
}

#endif
