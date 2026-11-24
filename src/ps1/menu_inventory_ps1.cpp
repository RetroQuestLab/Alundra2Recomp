/*
 * Menu armes / objets / equipement PS1 natif : presentation Alundra 1 (FONT3,
 * fenetre WIND, icones a1_icons.a1s), contenu Alundra 2 (data/items.txt).
 * Aucun STL, aucune allocation ; toute la logique est dans le header POD.
 */
#include "menu_inventory_ps1.h"

#include "a1_font_ps1.h"
#include "hero_ps1.h"
#include "icons_data_ps1.h"
#include "inventory_ps1_logic.h"
#include "items_data_ps1.h"

extern const uint8_t icons_dat[];

/* Etat miroir pour les scripts de capture native (lecture memoire depuis Lua) :
 * bits 0..7 = open, bits 8..15 = page, bits 16..23 = curseur. */
extern "C" uint32_t g_invCapture = 0;

namespace {

PSIInventory g_inv;
uint8_t g_iconW[84];
uint8_t g_iconH[84];
uint32_t g_prevAction = 0;
int g_loaded = 0;

inline uint16_t rd16(const uint8_t* p) {
	return uint16_t(p[0] | (uint16_t(p[1]) << 8));
}

inline int minI(int a, int b) { return a < b ? a : b; }

inline uint32_t bgr(int r, int g, int b) {
	const int rr = r < 0 ? 0 : (r > 31 ? 31 : r);
	const int gg = g < 0 ? 0 : (g > 31 ? 31 : g);
	const int bb = b < 0 ? 0 : (b > 31 ? 31 : b);
	return uint32_t(rr) | (uint32_t(gg) << 5) | (uint32_t(bb) << 10);
}

/* --- Police FONT3 (identique a menu_ps1.cpp) ---------------------------------- */
inline int glyphIndex(unsigned char c) { return c >= 32 ? int(c) : int('?'); }

void glyph(unsigned char c, int x, int y, int scale, int bank, int z) {
	const int idx = glyphIndex(c);
	pfDrawSprite16(x, y, PF_FONT_X + (idx & 15) * 4, PF_FONT_Y + (idx >> 4) * 16,
	               16 * scale, 16 * scale, PF_FONT_CLUT_X, PF_FONT_CLUT_Y + bank, z);
}

int textWidth(const char* s, int scale) {
	int w = 0;
	for (int i = 0; s[i] != '\0'; ++i)
		w += kA1FontAdvance[glyphIndex(static_cast<unsigned char>(s[i]))] * scale;
	return w;
}

void text(const char* s, int x, int y, int scale, int bank, int z) {
	for (int i = 0; s[i] != '\0'; ++i) {
		const unsigned char c = static_cast<unsigned char>(s[i]);
		glyph(c, x, y, scale, bank, z);
		x += kA1FontAdvance[glyphIndex(c)] * scale;
	}
}

/* Texte coupe a une largeur maximale, avec les avances reelles de FONT3. */
void textClipped(const char* s, int x, int y, int maxW, int bank, int z) {
	for (int i = 0; s[i] != '\0'; ++i) {
		const unsigned char c = static_cast<unsigned char>(s[i]);
		const int adv = kA1FontAdvance[glyphIndex(c)];
		if (x + adv > maxW) break;
		glyph(c, x, y, 1, bank, z);
		x += adv;
	}
}

/* --- Fenetre WIND (meme decoupe que le dialogue A1) ------------------------------ */
void windRect(int dx, int dy, int dw, int dh, int srcX, int srcY, int bank, int z) {
	pfDrawSprite16(dx, dy, PF_WIND_X + srcX / 4, PF_WIND_Y + srcY, dw, dh,
	               PF_WIND_CLUT_X, PF_WIND_CLUT_Y + bank, z);
}

void windPanel(int px, int py, int pw, int ph) {
	const int rollW = 16, capH = 12, edgeH = 8;
	const int cx = px + rollW, cw = pw - 2 * rollW;
	const int fillX = 16, fillY = 0, fillW = 80, fillH = 40;
	const int edgeX = 120, edgeW = 80;
	for (int y = 0; y < ph - 2 * edgeH; y += fillH) {
		for (int x = 0; x < cw; x += fillW) {
			windRect(cx + x, py + edgeH + y, minI(fillW, cw - x),
			         minI(fillH, ph - 2 * edgeH - y), fillX, fillY, 0, 10);
		}
	}
	for (int x = 0; x < cw; x += edgeW) {
		windRect(cx + x, py, minI(edgeW, cw - x), edgeH, edgeX, 0, 0, 10);
		windRect(cx + x, py + ph - edgeH, minI(edgeW, cw - x), edgeH, edgeX, 32, 0, 10);
	}
	windRect(px, py, rollW, capH, 224, 0, 0, 10);
	windRect(px + pw - rollW, py, rollW, capH, 224, 24, 0, 10);
	const int body = ph - capH;
	if (body > 0) {
		windRect(px, py + capH, rollW, minI(body, 40), 96, 16, 0, 10);
		windRect(px + pw - rollW, py + capH, rollW, minI(body, 40), 200, 16, 0, 10);
	}
}

/* --- Icones natives ------------------------------------------------------------- */
void loadIcons(void) {
	const uint8_t* p = icons_dat;
	const int count = rd16(p + 4);
	p += 8;
	for (int i = 0; i < count && i < 84; ++i) {
		const int w = rd16(p + 0), h = rd16(p + 2);
		const int stride = rd16(p + 8), uploadH = rd16(p + 10);
		const int clutX = rd16(p + 16), clutY = rd16(p + 18);
		p += 12 + 8 + 4;  // dims/pivots + placement VRAM + reserved/slot
		if (i == 0) pfUpload16(clutX, clutY, 16, 1, reinterpret_cast<const uint16_t*>(p));
		p += 32;          // CLUT partagee (identique pour toutes les frames)
		pfUpload4bpp(PF_ICON_TEX_X + (i % 12) * PF_ICON_CELL_W,
		             PF_ICON_TEX_Y + (i / 12) * PF_ICON_CELL_H,
		             stride, uploadH, p);
		p += stride * 2 * uploadH;
		g_iconW[i] = static_cast<uint8_t>(w);
		g_iconH[i] = static_cast<uint8_t>(h);
	}
	g_loaded = 1;
}

void drawIcon(int frame, int x, int y, int z) {
	if (frame < 0 || frame >= 84 || g_loaded == 0) return;
	const int col = frame % 12, row = frame / 12;
	pfDrawSprite16(x, y, PF_ICON_TEX_X + col * PF_ICON_CELL_W,
	               PF_ICON_TEX_Y + row * PF_ICON_CELL_H,
	               g_iconW[frame], g_iconH[frame],
	               PF_ICON_CLUT_X, PF_ICON_CLUT_Y, z);
}

int iconFrame(const char* code) {
	for (int i = 0; i < kPsiIconCount; ++i)
		if (psiSame(kPsiIcons[i].code, code)) return kPsiIcons[i].frame;
	return -1;
}

/* --- Presentation --------------------------------------------------------------- */
/* Structure A1 : grille a gauche, portrait/cartouche a droite, description bas.
 * Toute l'encre est FONT3 banque 0 (CLUT 6 du menu PC, identique au dialogue). */
constexpr int kGridX = 8, kGridY = 32, kGridW = 176, kGridH = 132;
constexpr int kRightX = 192, kRightY = 32, kRightW = 120, kRightH = 132;
constexpr int kDescY = 168, kDescH = 72;

void selectionBox(int x, int y, int w, int h) {
	const uint32_t col = bgr(28, 24, 6);
	pfDrawRect(x, y, w, 1, col, 1);
	pfDrawRect(x, y + h - 1, w, 1, col, 1);
	pfDrawRect(x, y, 1, h, col, 1);
	pfDrawRect(x + w - 1, y, 1, h, col, 1);
}

const char* pageTitle(int page) {
	if (page == PSI_PAGE_WEAPONS) return "ARMES";
	if (page == PSI_PAGE_ITEMS) return "OBJETS";
	return "EQUIPEMENT";
}

/* Trace une plage de caracteres sans depasser [begin,end). */
void textRange(const char* begin, const char* end, int x, int y, int bank, int z) {
	for (const char* p = begin; p != end && *p != '\0'; ++p) {
		const unsigned char c = static_cast<unsigned char>(*p);
		glyph(c, x, y, 1, bank, z);
		x += kA1FontAdvance[glyphIndex(c)];
	}
}

/* Replie un texte au mot sur several lignes FONT3 (avances reelles). */
void drawWrapped(const char* s, int x, int y, int maxW, int maxLines, int bank, int z) {
	int pos = 0;
	for (int line = 0; line < maxLines && s[pos] != '\0'; ++line) {
		int end = pos, w = 0, lastSpace = -1;
		while (s[end] != '\0') {
			const unsigned char c = static_cast<unsigned char>(s[end]);
			const int adv = kA1FontAdvance[glyphIndex(c)];
			if (w + adv > maxW) break;
			if (c == ' ') lastSpace = end;
			w += adv;
			++end;
		}
		if (s[end] != '\0' && lastSpace > pos) end = lastSpace;
		if (end == pos) ++end;
		textRange(s + pos, s + end, x, y + line * 16, bank, z);
		pos = end;
		while (s[pos] == ' ') ++pos;
	}
}

void drawQuantity(int qty, int x, int y) {
	if (qty <= 0) return;
	char s[6] = {'x', '0', '0', '0', '\0', '\0'};
	int n = qty;
	if (n > 999) n = 999;
	int i = 4;
	while (n > 0 && i > 1) {
		s[--i] = static_cast<char>('0' + n % 10);
		n /= 10;
	}
	// Decale le nombre a droite de 'x' (les zeros inutiles ne sont pas dessines).
	const char* digits = s + i;
	char out[6];
	out[0] = 'x';
	int k = 1;
	while (*digits != '\0') out[k++] = *digits++;
	out[k] = '\0';
	text(out, x, y, 1, 0, 2);
}

/* Portraits et deux bandeaux de noms a droite, comme gp/images.png. */
void drawRightPanel(const char* name, const char* sub) {
	windPanel(kRightX, 4, kRightW, 24);
	textClipped(name, kRightX + 8, 10, kRightX + kRightW - 8, 0, 1);
	windPanel(kRightX, kRightY, kRightW, kRightH);
	heroDrawPortraitPS1(kRightX + kRightW / 2, kRightY + 62, 2);
	if (sub != 0 && sub[0] != '\0') {
		windPanel(kRightX + 8, kRightY + kRightH - 30, kRightW - 16, 24);
		text(sub, kRightX + (kRightW - textWidth(sub, 1)) / 2,
		     kRightY + kRightH - 24, 1, 0, 2);
	}
}

void drawDescription(int def) {
	windPanel(8, kDescY, 304, kDescH);
	if (def < 0 || def >= kPsiDb.count) {
		text("AUCUN OBJET SELECTIONNE", 20, kDescY + 8, 1, 0, 2);
	} else {
		const PSIItemDef& d = kPsiDb.defs[def];
		text(d.name, 20, kDescY + 8, 1, 0, 2);
		const PSIItemEntry* e = psiEntryConst(&g_inv, def);
		if (e != 0 && e->qty > 0)
			drawQuantity(e->qty, 20 + textWidth(d.name, 1) + 10, kDescY + 8);
		drawWrapped(d.desc, 20, kDescY + 24, 272, 2, 0, 2);
	}
	text("L/R:PAGE  HAUT/BAS:CHOIX  X:OK  O:FERMER", 20, kDescY + 56, 1, 0, 2);
}

void drawWeapons(void) {
	windPanel(kGridX, kGridY, kGridW, kGridH);
	const int n = psiPageCount(&kPsiDb, PSI_PAGE_WEAPONS);
	const int shown = n < 6 ? n : 6;
	for (int i = 0; i < shown; ++i) {
		const int def = psiPageDefAt(&kPsiDb, PSI_PAGE_WEAPONS, i);
		if (def < 0) continue;
		const int col = i % 3, row = i / 3;
		const int x = kGridX + 10 + col * 56, y = kGridY + 10 + row * 62;
		if (i == g_inv.cursor) selectionBox(x - 4, y - 4, 52, 58);
		drawIcon(iconFrame(kPsiDb.defs[def].code), x + 10, y + 2, 2);
		const PSIItemEntry* e = psiEntryConst(&g_inv, def);
		if (g_inv.weapon == def) text("EQUIPE", x + 2, y + 38, 1, 0, 2);
		else if (e != 0 && e->qty > 0) text("EN MAIN", x, y + 38, 1, 0, 2);
	}
}

void drawItems(void) {
	windPanel(kGridX, kGridY, kGridW, kGridH);
	const int n = psiPageCount(&kPsiDb, PSI_PAGE_ITEMS);
	const int cols = 5, rows = 3;
	for (int i = 0; i < n && i < cols * rows; ++i) {
		const int def = psiPageDefAt(&kPsiDb, PSI_PAGE_ITEMS, i);
		if (def < 0) continue;
		const PSIItemEntry* e = psiEntryConst(&g_inv, def);
		const int col = i % cols, row = i / cols;
		const int x = kGridX + 6 + col * 34, y = kGridY + 8 + row * 42;
		if (i == g_inv.cursor) selectionBox(x - 2, y - 2, 28, 40);
		drawIcon(iconFrame(kPsiDb.defs[def].code), x + 2, y, 2);
		if (e != 0 && e->qty > 0) drawQuantity(e->qty, x + 2, y + 26);
	}
}

void drawEquipment(void) {
	windPanel(kGridX, kGridY, kGridW, kGridH);
	const char* labels[3] = {"EPEE", "ANNEAU", "POCHE"};
	const int8_t* slots[3] = {&g_inv.weapon, &g_inv.ring, &g_inv.pocket};
	for (int i = 0; i < 3; ++i) {
		const int y = kGridY + 8 + i * 42;
		if (i == g_inv.cursor) selectionBox(kGridX + 4, y - 4, kGridW - 8, 40);
		text(labels[i], kGridX + 10, y, 1, 0, 2);
		const int def = *slots[i];
		if (def >= 0 && def < kPsiDb.count) {
			drawIcon(iconFrame(kPsiDb.defs[def].code), kGridX + 66, y - 4, 2);
			textClipped(kPsiDb.defs[def].name, kGridX + 104, y + 8,
			            kGridX + kGridW - 8, 0, 2);
		} else {
			text("VIDE", kGridX + 70, y + 8, 1, 0, 2);
		}
	}
}

void pageAction(void) {
	if (g_inv.page == PSI_PAGE_WEAPONS) {
		const int def = psiPageDefAt(&kPsiDb, PSI_PAGE_WEAPONS, g_inv.cursor);
		if (def < 0) return;
		if (g_inv.weapon == def) psiUnequip(&g_inv, &kPsiDb, kPsiDb.defs[def].code);
		else psiEquip(&g_inv, &kPsiDb, kPsiDb.defs[def].code);
	} else if (g_inv.page == PSI_PAGE_ITEMS) {
		const int def = psiPageDefAt(&kPsiDb, PSI_PAGE_ITEMS, g_inv.cursor);
		if (def >= 0) psiUse(&g_inv, &kPsiDb, kPsiDb.defs[def].code);
	} else {
		const uint8_t kinds[3] = {PSI_KIND_WEAPON, PSI_KIND_RING, PSI_KIND_POCKET};
		if (g_inv.cursor < 3) psiCycleEquip(&g_inv, &kPsiDb, kinds[g_inv.cursor], 1);
	}
}

int currentDef(void) {
	if (g_inv.page == PSI_PAGE_WEAPONS) return psiPageDefAt(&kPsiDb, PSI_PAGE_WEAPONS, g_inv.cursor);
	if (g_inv.page == PSI_PAGE_ITEMS) return psiPageDefAt(&kPsiDb, PSI_PAGE_ITEMS, g_inv.cursor);
	const int8_t* slots[3] = {&g_inv.weapon, &g_inv.ring, &g_inv.pocket};
	return g_inv.cursor < 3 ? int(*slots[g_inv.cursor]) : -1;
}

}  // namespace

void inventoryInitPS1(void) {
	if (g_loaded == 0) loadIcons();
	psiInit(&g_inv, &kPsiDb);
	g_prevAction = 0;
}

void inventoryOpenPS1(uint32_t pad) {
	g_inv.open = 1;
	g_inv.page = PSI_PAGE_WEAPONS;
	g_inv.cursor = 0;
	g_inv.prevPad = pad;
	g_prevAction = pad;
	g_invCapture = 1u;  /* open=1, page=0, cursor=0 */
}

void inventoryUpdatePS1(uint32_t pad) {
	const uint32_t edge = pad & ~g_prevAction;
	g_prevAction = pad;
	psiMenuStep(&g_inv, &kPsiDb, pad);
	if (g_inv.open != 0 && (edge & PF_BTN_CROSS)) pageAction();
	g_invCapture = uint32_t(g_inv.open) | (uint32_t(g_inv.page) << 8) |
	               (uint32_t(g_inv.cursor) << 16);
}

void inventoryDrawPS1(void) {
	pfDrawRect(0, 0, PF_W, PF_H, 0x0000u, 63);
	windPanel(8, 4, kGridW, 24);
	text(pageTitle(g_inv.page), 16, 10, 1, 0, 1);

	const int def = currentDef();
	const char* name = def >= 0 && def < kPsiDb.count ? kPsiDb.defs[def].name : "";

	if (g_inv.page == PSI_PAGE_WEAPONS) drawWeapons();
	else if (g_inv.page == PSI_PAGE_ITEMS) drawItems();
	else drawEquipment();

	drawRightPanel(name, "FLINT");
	drawDescription(def);
}

int inventoryIsOpenPS1(void) { return g_inv.open; }

int inventoryAddPS1(const char* code, int qty) { return psiAdd(&g_inv, &kPsiDb, code, qty); }

int inventoryQtyPS1(const char* code) { return psiQty(&g_inv, &kPsiDb, code); }

int inventorySavePS1(uint8_t* buf) { return psiSave(&g_inv, buf); }

int inventoryLoadPS1(const uint8_t* buf) { return psiLoad(&g_inv, &kPsiDb, buf); }

const char* inventoryWeaponCodePS1(void) {
	return g_inv.weapon >= 0 ? kPsiDb.defs[g_inv.weapon].code : 0;
}

const char* inventoryPocketCodePS1(void) {
	return g_inv.pocket >= 0 ? kPsiDb.defs[g_inv.pocket].code : 0;
}

void inventoryDrawIconPS1(const char* code, int x, int y, int z) {
	if (code == 0) return;
	const int frame = iconFrame(code);
	if (frame >= 0) drawIcon(frame, x, y, z);
}
