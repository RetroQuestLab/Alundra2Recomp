/*
 * Test hote de la logique d'inventaire / equipement PS1 (TDD).
 * Compile le VRAI header utiliser par le build MIPS ; aucun mock.
 *
 *   external\llvm-mingw-20261020-ucrt-x86_64\bin\clang++.exe -std=c++20 ^
 *       -o qa\ps1_inventory_logic_test.exe tools\ps1_inventory_logic_test.cpp
 *   .\qa\ps1_inventory_logic_test.exe
 */
#include "../src/ps1/inventory_ps1_logic.h"

#include <cstdio>
#include <cstring>

namespace {
int g_fail = 0;
int g_checks = 0;
#define CHECK(cond)                                                                    \
	do {                                                                                 \
		++g_checks;                                                                        \
		if (!(cond)) {                                                                     \
			std::printf("FAIL ligne %d : %s\n", __LINE__, #cond);                           \
			++g_fail;                                                                      \
		}                                                                                  \
	} while (0)

void def(PSIItemDb& db, const char* code, const char* name, int kind, int maxQty,
         int consumable, int power) {
	PSIItemDef& d = db.defs[db.count++];
	std::memset(&d, 0, sizeof(d));
	std::strncpy(d.code, code, PSI_MAX_CODE - 1);
	std::strncpy(d.name, name, PSI_MAX_NAME - 1);
	d.kind = static_cast<uint8_t>(kind);
	d.maxQty = static_cast<int16_t>(maxQty);
	d.consumable = static_cast<uint8_t>(consumable);
	d.power = static_cast<int16_t>(power);
}

PSIItemDb makeDb() {
	PSIItemDb db;
	std::memset(&db, 0, sizeof(db));
	def(db, "long_sword", "Epee longue", PSI_KIND_WEAPON, 1, 0, 1);
	def(db, "herb", "Herbe", PSI_KIND_CONSUMABLE, 9, 1, 0);
	def(db, "strange_key", "Cle etrange", PSI_KIND_KEY, 1, 0, 0);
	def(db, "pixie_ring", "Anneau de Pixie", PSI_KIND_RING, 1, 0, 0);
	def(db, "compass", "Boussole", PSI_KIND_POCKET, 1, 0, 0);
	def(db, "siren_ring", "Anneau de Sirene", PSI_KIND_RING, 1, 0, 0);
	return db;
}

void tap(PSIInventory& inv, const PSIItemDb& db, uint32_t bit) {
	psiMenuStep(&inv, &db, bit);
	psiMenuStep(&inv, &db, 0);
}
}  // namespace

int main() {
	PSIItemDb db = makeDb();

	// --- etat initial : epee longue equipee, page armes, rien d'autre ---------------
	{
		PSIInventory inv;
		psiInit(&inv, &db);
		CHECK(psiFindDef(&db, "long_sword") == 0);
		CHECK(psiFindDef(&db, "absent") < 0);
		CHECK(psiQty(&inv, &db, "long_sword") == 1);
		CHECK(psiQty(&inv, &db, "herb") == 0);
		CHECK(inv.weapon == 0);
		CHECK(inv.ring < 0);
		CHECK(inv.pocket < 0);
		CHECK(inv.page == PSI_PAGE_WEAPONS);
		CHECK(inv.open == 0);
	}

	// --- consommable : quantite, plafond 9, puis decrementation --------------------
	{
		PSIInventory inv;
		psiInit(&inv, &db);
		for (int i = 0; i < 12; ++i) psiAdd(&inv, &db, "herb", 1);
		CHECK(psiQty(&inv, &db, "herb") == 9);
		CHECK(psiUse(&inv, &db, "herb") == 1);
		CHECK(psiQty(&inv, &db, "herb") == 8);
		for (int i = 0; i < 8; ++i) psiUse(&inv, &db, "herb");
		CHECK(psiQty(&inv, &db, "herb") == 0);
		CHECK(psiUse(&inv, &db, "herb") == 0);  // plus rien a consommer
		CHECK(psiAdd(&inv, &db, "inconnu", 1) < 0);
	}

	// --- objet cle : jamais consommable, jamais empilable --------------------------
	{
		PSIInventory inv;
		psiInit(&inv, &db);
		CHECK(psiAdd(&inv, &db, "strange_key", 1) == 1);
		CHECK(psiAdd(&inv, &db, "strange_key", 5) == 1);
		CHECK(psiUse(&inv, &db, "strange_key") == 0);
		CHECK(psiQty(&inv, &db, "strange_key") == 1);
	}

	// --- equipement : types respectes, possession requise, desequipement ------------
	{
		PSIInventory inv;
		psiInit(&inv, &db);
		CHECK(psiEquip(&inv, &db, "herb") == 0);        // consommable non equippable
		CHECK(psiEquip(&inv, &db, "rising_sword") == 0); // inconnu
		CHECK(psiEquip(&inv, &db, "pixie_ring") == 0);   // pas encore possede
		psiAdd(&inv, &db, "pixie_ring", 1);
		psiAdd(&inv, &db, "compass", 1);
		CHECK(psiEquip(&inv, &db, "pixie_ring") == 1);
		CHECK(inv.ring == 3);
		CHECK(psiEquip(&inv, &db, "compass") == 1);
		CHECK(inv.pocket == 4);
		CHECK(psiEquip(&inv, &db, "strange_key") == 0);  // cle non equippable
		CHECK(psiUnequip(&inv, &db, "pixie_ring") == 1);
		CHECK(inv.ring < 0);
		CHECK(psiQty(&inv, &db, "pixie_ring") == 1);     // desequiper ne consomme pas
	}

	// --- cycle d'equipement : seulement les objets possedes, avec retour a vide ----
	{
		PSIInventory inv;
		psiInit(&inv, &db);
		psiAdd(&inv, &db, "pixie_ring", 1);
		psiAdd(&inv, &db, "siren_ring", 1);
		CHECK(psiCycleEquip(&inv, &db, PSI_KIND_RING, 1) == 3);
		CHECK(inv.ring == 3);
		CHECK(psiCycleEquip(&inv, &db, PSI_KIND_RING, 1) == 5);
		CHECK(inv.ring == 5);
		CHECK(psiCycleEquip(&inv, &db, PSI_KIND_RING, 1) == -1);  // apres le dernier : rien
		CHECK(inv.ring < 0);
		CHECK(psiCycleEquip(&inv, &db, PSI_KIND_RING, 1) == 3);   // on reprend au debut
		CHECK(psiCycleEquip(&inv, &db, PSI_KIND_RING, -1) == -1); // avant le premier : vide
		CHECK(psiCycleEquip(&inv, &db, PSI_KIND_WEAPON, 1) == -1); // epee deja equipee : vide
		CHECK(psiCycleEquip(&inv, &db, PSI_KIND_WEAPON, 1) == 0);  // puis revient a l'epee
	}

	// --- navigation : pages, curseur borne, front montant, ouverture/fermeture ------
	{
		PSIInventory inv;
		psiInit(&inv, &db);
		tap(inv, db, PF_BTN_START);  // SELECT/START ouvre l'inventaire en jeu
		CHECK(inv.open == 1);
		tap(inv, db, PF_BTN_RIGHT);
		CHECK(inv.page == PSI_PAGE_ITEMS);
		tap(inv, db, PF_BTN_RIGHT);
		CHECK(inv.page == PSI_PAGE_EQUIPMENT);
		for (int i = 0; i < 8; ++i) tap(inv, db, PF_BTN_RIGHT);
		CHECK(inv.page == PSI_PAGE_EQUIPMENT);  // borne haute
		tap(inv, db, PF_BTN_DOWN);
		CHECK(inv.cursor == 1);
		psiMenuStep(&inv, &db, PF_BTN_DOWN);  // nouveau front : 1 -> 0 (2 entrees)
		const int held = inv.cursor;
		for (int i = 0; i < 12; ++i) psiMenuStep(&inv, &db, PF_BTN_DOWN);
		CHECK(inv.cursor == held);  // maintien : aucun repeat
		tap(inv, db, PF_BTN_LEFT);
		CHECK(inv.page == PSI_PAGE_ITEMS);
		tap(inv, db, PF_BTN_CIRCLE);
		CHECK(inv.open == 0);
	}

	// --- sauvegarde : round-trip exact des quantites, equippement et page -----------
	{
		PSIInventory a, b;
		psiInit(&a, &db);
		psiAdd(&a, &db, "herb", 4);
		psiAdd(&a, &db, "strange_key", 1);
		psiAdd(&a, &db, "pixie_ring", 1);
		psiEquip(&a, &db, "pixie_ring");
		a.page = PSI_PAGE_ITEMS;
		a.cursor = 2;
		uint8_t buf[PSI_SAVE_SIZE];
		std::memset(buf, 0, sizeof(buf));
		CHECK(psiSave(&a, buf) == 1);
		std::memset(&b, 0xCD, sizeof(b));
		CHECK(psiLoad(&b, &db, buf) == 1);
		CHECK(psiQty(&b, &db, "herb") == 4);
		CHECK(psiQty(&b, &db, "strange_key") == 1);
		CHECK(b.ring == a.ring);
		CHECK(b.page == PSI_PAGE_ITEMS);
		CHECK(b.cursor == 2);
		buf[0] ^= 0xFF;
		CHECK(psiLoad(&b, &db, buf) == 0);  // magic invalide : refuse
	}

	if (g_fail == 0) std::printf("PS1_INVENTORY_LOGIC %d/%d\n", g_checks, g_checks);
	return g_fail == 0 ? 0 : 1;
}
