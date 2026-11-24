/*
 * HUD de la cible PS1 : gemmes de vie native A1.
 *
 * Les quatre indicateurs etaient auparavant des rectangles rouges pleins,
 * inventes pour le diagnostic ; la comparaison avec la reference A1 a montre
 * que le HUD utilise des gemmes contournees de a1_hud.a1s (deja employees par
 * le HUD PC dans src/main.cpp::drawHud). Le blob P1HU est genere par
 * tools/ps1_export_hud.py : 15 frames (4 gemmes, 10 chiffres 0..9 et slash)
 * rangees dans une seule page VRAM, avec le suffixe "/MM" du maximum de PV.
 * L'en-tete est valide avant tout upload (hudBlobValidate).
 */
#include "hud_ps1.h"

#include "hud_ps1_logic.h"

#include "../platform/pf.h"

#include <stddef.h>

extern "C" {
extern const uint8_t hud_dat[];
extern const uint32_t hud_dat_size;
}

namespace {

struct HudGem {
	const uint8_t* pixels;
	const uint8_t* clut;
	int w, h;
	int stride, uploadH;
	int atlasY, clutY;
};

/* 0/1 = grosses gemmes pleine/vide, 2/3 = petites gemmes pleine/vide,
 * 4..13 = chiffres 0..9 (frames A1 9..18), 14 = slash (frame A1 19). */
HudGem g_gems[15];
int g_hp = 10;
int g_maxHp = 10;

inline uint16_t rd16(const uint8_t* p) {
	return (uint16_t)(p[0] | (p[1] << 8));
}

}  // namespace

void hudInitPS1(void) {
	/* Un blob tronque ou invalide ne doit jamais faire lire le MIPS hors du
	 * tableau : dans ce cas le HUD reste vide (g_gems a zero). */
	if (!hudBlobValidate(hud_dat, hud_dat_size, PF_HUD_TEX_W, PF_HUD_TEX_H)) return;
	const uint8_t* p = hud_dat + 4;  // magic P1HU déjà vérifié
	const int count = rd16(p);
	p += 4;  // count + reserved
	for (int i = 0; i < count && i < 15; ++i) {
		HudGem& g = g_gems[i];
		g.w = rd16(p);
		g.h = rd16(p + 2);
		// ox/oy (p+4, p+6) ne sont pas utilises : le HUD place ses gemmes
		// explicitement, sans origine issue de l'atlas A1.
		g.stride = rd16(p + 8);
		g.uploadH = rd16(p + 10);
		g.atlasY = rd16(p + 12);
		g.clutY = rd16(p + 14);
		p += 16;
		g.clut = p;
		p += 32;
		g.pixels = p;
		p += (size_t)g.stride * 2u * (size_t)g.uploadH;

		pfUpload16(PF_HUD_CLUT_X, g.clutY, 16, 1,
		           reinterpret_cast<const uint16_t*>(g.clut));
		pfUpload4bpp(PF_HUD_TEX_X, g.atlasY, g.stride, g.uploadH, g.pixels);
	}
}

void hudSetVitalsPS1(int hp, int maxHp) {
	g_maxHp = maxHp < 0 ? 0 : maxHp;
	g_hp = hp < 0 ? 0 : (hp > g_maxHp ? g_maxHp : hp);
}

void hudDrawPS1(void) {
	// Petites gemmes : une par PV, deux rangees de cinq, comme la cible PC.
	for (int i = 0; i < 10; ++i) {
		const HudGem& g = g_gems[hudSmallFull(g_hp, g_maxHp, i) ? 2 : 3];
		pfDrawSprite16(hudSmallGemX(i), hudSmallGemY(i), PF_HUD_TEX_X,
		               g.atlasY, g.w, g.h, PF_HUD_CLUT_X, g.clutY, 0);
	}
	// Grosses gemmes uniquement quand le maximum depasse 10 PV (mapping PC).
	const int nBig = hudBigGemCount(g_maxHp);
	for (int k = 0; k < nBig; ++k) {
		const HudGem& g = g_gems[hudBigGemFull(g_hp, g_maxHp, k) ? 0 : 1];
		pfDrawSprite16(104 - 8 * k, 16, PF_HUD_TEX_X, g.atlasY, g.w, g.h,
		               PF_HUD_CLUT_X, g.clutY, 0);
	}
	// Suffixe "/MM" : memes frames A1 et memes coordonnees que src/main.cpp::drawHud.
	const HudGem& slash = g_gems[14];
	pfDrawSprite16(160, 16, PF_HUD_TEX_X, slash.atlasY, slash.w, slash.h,
	               PF_HUD_CLUT_X, slash.clutY, 0);
	for (int place = 0; place < 2; ++place) {
		const HudGem& digit = g_gems[4 + hudMaxDigit(g_maxHp, place)];
		pfDrawSprite16(168 + 8 * place, 16, PF_HUD_TEX_X, digit.atlasY, digit.w,
		               digit.h, PF_HUD_CLUT_X, digit.clutY, 0);
	}
}
