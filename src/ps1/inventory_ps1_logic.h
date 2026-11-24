#pragma once
/*
 * Logique d'inventaire / equipement PS1, sans STL ni allocation.
 *
 * Ce header est compile deux fois :
 *   - par le build MIPS (via menu_inventory_ps1.cpp), avec les bits PF_BTN_* ;
 *   - par le test hote tools/ps1_inventory_logic_test.cpp (clang++ x86_64).
 * Il ne doit contenir que des POD et des fonctions inline : aucune allocation,
 * aucun destructeur global, aucune bibliotheque C++.
 *
 * Le contenu A2 (six epees, consommables, anneaux, objets de poche, cles) vient
 * de data/items.txt via tools/ps1_export_items.py. La presentation reste A1
 * (fenetre WIND, police FONT3) dans menu_inventory_ps1.cpp.
 */
#include <stdint.h>

#include "../platform/pf.h"

#define PSI_PAGE_WEAPONS   0
#define PSI_PAGE_ITEMS     1
#define PSI_PAGE_EQUIPMENT 2
#define PSI_PAGE_COUNT     3

#define PSI_KIND_CONSUMABLE 0
#define PSI_KIND_KEY        1
#define PSI_KIND_WEAPON     2
#define PSI_KIND_RING       3
#define PSI_KIND_POCKET     4

#define PSI_MAX_ITEMS 64
#define PSI_MAX_CODE  24
#define PSI_MAX_NAME  36
#define PSI_MAX_DESC  96
#define PSI_SAVE_SIZE 256

#define PSI_SAVE_MAGIC   0x50414956u  /* "PAIV" */
#define PSI_SAVE_VERSION 1u

struct PSIItemDef {
	char code[PSI_MAX_CODE];
	char name[PSI_MAX_NAME];
	char desc[PSI_MAX_DESC];
	uint8_t kind;
	uint8_t consumable;
	int16_t maxQty;
	int16_t power;
};

struct PSIItemDb {
	PSIItemDef defs[PSI_MAX_ITEMS];
	int count;
};

struct PSIItemEntry {
	uint8_t def;  /* 0xFF : emplacement vide */
	int16_t qty;
};

struct PSIInventory {
	PSIItemEntry entries[PSI_MAX_ITEMS];
	int8_t weapon;   /* index de definition, -1 = rien */
	int8_t ring;
	int8_t pocket;
	uint8_t page;
	uint8_t cursor;
	uint8_t open;
	uint32_t prevPad;
};

inline int psiLen(const char* s) {
	int n = 0;
	while (s[n] != '\0' && n < 256) ++n;
	return n;
}

inline int psiSame(const char* a, const char* b) {
	int i = 0;
	while (a[i] != '\0' && b[i] != '\0') {
		if (a[i] != b[i]) return 0;
		++i;
	}
	return a[i] == b[i];
}

inline int psiFindDef(const PSIItemDb* db, const char* code) {
	if (db == 0 || code == 0) return -1;
	for (int i = 0; i < db->count; ++i)
		if (psiSame(db->defs[i].code, code)) return i;
	return -1;
}

inline PSIItemEntry* psiEntry(PSIInventory* inv, int def) {
	if (inv == 0 || def < 0 || def >= PSI_MAX_ITEMS) return 0;
	for (int i = 0; i < PSI_MAX_ITEMS; ++i)
		if (inv->entries[i].def == static_cast<uint8_t>(def)) return &inv->entries[i];
	return 0;
}

inline const PSIItemEntry* psiEntryConst(const PSIInventory* inv, int def) {
	return psiEntry(const_cast<PSIInventory*>(inv), def);
}

inline void psiInit(PSIInventory* inv, const PSIItemDb* db) {
	for (int i = 0; i < PSI_MAX_ITEMS; ++i) {
		inv->entries[i].def = 0xFF;
		inv->entries[i].qty = 0;
	}
	inv->weapon = -1;
	inv->ring = -1;
	inv->pocket = -1;
	inv->page = PSI_PAGE_WEAPONS;
	inv->cursor = 0;
	inv->open = 0;
	inv->prevPad = 0;
	const int sword = psiFindDef(db, "long_sword");
	if (sword >= 0) {
		PSIItemEntry* e = 0;
		for (int i = 0; i < PSI_MAX_ITEMS; ++i) {
			if (inv->entries[i].def == 0xFF) { e = &inv->entries[i]; break; }
		}
		if (e != 0) {
			e->def = static_cast<uint8_t>(sword);
			e->qty = 1;
			inv->weapon = static_cast<int8_t>(sword);
		}
	}
}

inline int psiQty(const PSIInventory* inv, const PSIItemDb* db, const char* code) {
	const int def = psiFindDef(db, code);
	const PSIItemEntry* e = psiEntryConst(inv, def);
	return e == 0 ? 0 : e->qty;
}

inline int psiAdd(PSIInventory* inv, const PSIItemDb* db, const char* code, int qty) {
	const int def = psiFindDef(db, code);
	if (def < 0 || qty <= 0) return -1;
	PSIItemEntry* e = psiEntry(inv, def);
	if (e == 0) {
		for (int i = 0; i < PSI_MAX_ITEMS; ++i) {
			if (inv->entries[i].def == 0xFF) {
				e = &inv->entries[i];
				e->def = static_cast<uint8_t>(def);
				break;
			}
		}
	}
	if (e == 0) return -1;
	const int cap = db->defs[def].maxQty;
	int next = e->qty + qty;
	if (cap > 0 && next > cap) next = cap;
	if (next > 32767) next = 32767;
	e->qty = static_cast<int16_t>(next);
	return e->qty;
}

inline int psiUse(PSIInventory* inv, const PSIItemDb* db, const char* code) {
	const int def = psiFindDef(db, code);
	if (def < 0 || db->defs[def].consumable == 0) return 0;
	PSIItemEntry* e = psiEntry(inv, def);
	if (e == 0 || e->qty <= 0) return 0;
	--e->qty;
	return 1;
}

inline int psiEquip(PSIInventory* inv, const PSIItemDb* db, const char* code) {
	const int def = psiFindDef(db, code);
	if (def < 0) return 0;
	const PSIItemEntry* e = psiEntryConst(inv, def);
	if (e == 0 || e->qty <= 0) return 0;
	int8_t* slot = 0;
	switch (db->defs[def].kind) {
		case PSI_KIND_WEAPON: slot = &inv->weapon; break;
		case PSI_KIND_RING:   slot = &inv->ring;   break;
		case PSI_KIND_POCKET: slot = &inv->pocket; break;
		default: return 0;
	}
	*slot = static_cast<int8_t>(def);
	return 1;
}

/* Fait passer l'emplacement a l'objet possede suivant (ou precedent) de meme
 * nature. Apres le dernier, l'emplacement revient a vide ; le cycle recommence
 * alors au premier objet. Renvoie l'index de definition, ou -1 si vide. */
inline int psiCycleEquip(PSIInventory* inv, const PSIItemDb* db, uint8_t kind, int dir) {
	int8_t* slot = 0;
	if (kind == PSI_KIND_WEAPON) slot = &inv->weapon;
	else if (kind == PSI_KIND_RING) slot = &inv->ring;
	else if (kind == PSI_KIND_POCKET) slot = &inv->pocket;
	else return -1;
	int owned[PSI_MAX_ITEMS];
	int n = 0;
	for (int i = 0; i < db->count && n < PSI_MAX_ITEMS; ++i) {
		if (db->defs[i].kind != kind) continue;
		const PSIItemEntry* e = psiEntryConst(inv, i);
		if (e != 0 && e->qty > 0) owned[n++] = i;
	}
	if (n == 0) {
		*slot = -1;
		return -1;
	}
	int pos = -1;
	for (int i = 0; i < n; ++i)
		if (owned[i] == *slot) pos = i;
	int next = pos < 0 ? (dir >= 0 ? 0 : n - 1) : pos + (dir >= 0 ? 1 : -1);
	if (next < 0 || next >= n) {
		*slot = -1;
		return -1;
	}
	*slot = static_cast<int8_t>(owned[next]);
	return owned[next];
}

inline int psiUnequip(PSIInventory* inv, const PSIItemDb* db, const char* code) {
	const int def = psiFindDef(db, code);
	if (def < 0) return 0;
	int8_t* slot = 0;
	switch (db->defs[def].kind) {
		case PSI_KIND_WEAPON: slot = &inv->weapon; break;
		case PSI_KIND_RING:   slot = &inv->ring;   break;
		case PSI_KIND_POCKET: slot = &inv->pocket; break;
		default: return 0;
	}
	if (*slot != static_cast<int8_t>(def)) return 0;
	*slot = -1;
	return 1;
}

inline int psiPageCount(const PSIItemDb* db, int page) {
	if (db == 0) return 0;
	if (page == PSI_PAGE_EQUIPMENT) return 3;  /* arme, anneau, poche */
	int n = 0;
	for (int i = 0; i < db->count; ++i) {
		const uint8_t k = db->defs[i].kind;
		if (page == PSI_PAGE_WEAPONS && k == PSI_KIND_WEAPON) ++n;
		else if (page == PSI_PAGE_ITEMS && (k == PSI_KIND_CONSUMABLE || k == PSI_KIND_KEY)) ++n;
	}
	return n;
}

/* Definition affichee a une position donnee de la page, ou -1. */
inline int psiPageDefAt(const PSIItemDb* db, int page, int cursor) {
	if (db == 0 || cursor < 0 || page == PSI_PAGE_EQUIPMENT) return -1;
	int k = 0;
	for (int i = 0; i < db->count; ++i) {
		const uint8_t kind = db->defs[i].kind;
		const bool shown = (page == PSI_PAGE_WEAPONS && kind == PSI_KIND_WEAPON) ||
		                   (page == PSI_PAGE_ITEMS &&
		                    (kind == PSI_KIND_CONSUMABLE || kind == PSI_KIND_KEY));
		if (shown && k++ == cursor) return i;
	}
	return -1;
}

inline void psiMenuStep(PSIInventory* inv, const PSIItemDb* db, uint32_t pad) {
	const uint32_t edge = pad & ~inv->prevPad;
	inv->prevPad = pad;
	if (inv->open == 0) {
		if (edge & (PF_BTN_START | PF_BTN_SELECT | PF_BTN_L1)) {
			inv->open = 1;
			inv->page = PSI_PAGE_WEAPONS;
			inv->cursor = 0;
		}
		return;
	}
	if (edge & (PF_BTN_CIRCLE | PF_BTN_START | PF_BTN_SELECT)) {
		inv->open = 0;
		return;
	}
	if (edge & PF_BTN_LEFT) {
		inv->page = inv->page > 0 ? inv->page - 1 : 0;
		inv->cursor = 0;
	} else if (edge & PF_BTN_RIGHT) {
		inv->page = inv->page + 1 < PSI_PAGE_COUNT ? inv->page + 1 : PSI_PAGE_COUNT - 1;
		inv->cursor = 0;
	}
	const int count = psiPageCount(db, inv->page);
	if (count <= 0) {
		inv->cursor = 0;
		return;
	}
	if (edge & PF_BTN_UP) {
		inv->cursor = inv->cursor > 0 ? inv->cursor - 1 : static_cast<uint8_t>(count - 1);
	} else if (edge & PF_BTN_DOWN) {
		inv->cursor = inv->cursor + 1 < count ? inv->cursor + 1 : 0;
	}
	if (inv->cursor >= count) inv->cursor = static_cast<uint8_t>(count - 1);
}

inline void psiPut16(uint8_t* p, uint16_t v) {
	p[0] = static_cast<uint8_t>(v & 0xFFu);
	p[1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
}

inline uint16_t psiGet16(const uint8_t* p) {
	return static_cast<uint16_t>(p[0] | (uint16_t(p[1]) << 8));
}

inline void psiPut32(uint8_t* p, uint32_t v) {
	for (int i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>((v >> (8 * i)) & 0xFFu);
}

inline uint32_t psiGet32(const uint8_t* p) {
	uint32_t v = 0;
	for (int i = 0; i < 4; ++i) v |= uint32_t(p[i]) << (8 * i);
	return v;
}

inline int psiSave(const PSIInventory* inv, uint8_t* buf) {
	if (inv == 0 || buf == 0) return 0;
	for (int i = 0; i < PSI_SAVE_SIZE; ++i) buf[i] = 0;
	psiPut32(buf, PSI_SAVE_MAGIC);
	psiPut16(buf + 4, PSI_SAVE_VERSION);
	int n = 8;
	for (int i = 0; i < PSI_MAX_ITEMS; ++i) {
		if (inv->entries[i].def == 0xFF) continue;
		buf[n++] = inv->entries[i].def;
		psiPut16(buf + n, static_cast<uint16_t>(inv->entries[i].qty));
		n += 2;
	}
	psiPut16(buf + 6, static_cast<uint16_t>(n));  /* fin des entrees */
	buf[n++] = static_cast<uint8_t>(inv->weapon + 1);
	buf[n++] = static_cast<uint8_t>(inv->ring + 1);
	buf[n++] = static_cast<uint8_t>(inv->pocket + 1);
	buf[n++] = inv->page;
	buf[n++] = inv->cursor;
	return 1;
}

inline int psiLoad(PSIInventory* inv, const PSIItemDb* db, const uint8_t* buf) {
	if (inv == 0 || db == 0 || buf == 0) return 0;
	if (psiGet32(buf) != PSI_SAVE_MAGIC) return 0;
	if (psiGet16(buf + 4) != PSI_SAVE_VERSION) return 0;
	const int n = psiGet16(buf + 6);
	if (n < 8 || n > PSI_SAVE_SIZE - 8) return 0;
	psiInit(inv, db);
	for (int i = 0; i < PSI_MAX_ITEMS; ++i) {
		inv->entries[i].def = 0xFF;
		inv->entries[i].qty = 0;
	}
	int p = 8;
	while (p + 2 <= n) {
		const uint8_t d = buf[p++];
		const int16_t q = static_cast<int16_t>(psiGet16(buf + p));
		p += 2;
		if (d >= db->count) return 0;
		PSIItemEntry* e = psiEntry(inv, d);
		if (e == 0) {
			for (int i = 0; i < PSI_MAX_ITEMS; ++i) {
				if (inv->entries[i].def == 0xFF) { e = &inv->entries[i]; break; }
			}
		}
		if (e == 0) return 0;
		e->def = d;
		e->qty = q;
	}
	if (p + 5 > PSI_SAVE_SIZE) return 0;
	inv->weapon = static_cast<int8_t>(static_cast<int>(buf[p++]) - 1);
	inv->ring = static_cast<int8_t>(static_cast<int>(buf[p++]) - 1);
	inv->pocket = static_cast<int8_t>(static_cast<int>(buf[p++]) - 1);
	inv->page = buf[p++];
	inv->cursor = buf[p++];
	inv->open = 0;
	inv->prevPad = 0;
	return 1;
}
