/*
 * Test hote d'integration : la table GENEREE data/items.txt -> POD PS1.
 *
 *   py -3.12 tools\ps1_export_items.py
 *   external\llvm-mingw-20261020-ucrt-x86_64\bin\clang++.exe -std=c++20 ^
 *       -o qa\ps1_inventory_data_test.exe tools\ps1_inventory_data_test.cpp
 *   .\qa\ps1_inventory_data_test.exe
 */
#include "../src/ps1/items_data_ps1.h"

#include <cstdio>
#include <initializer_list>

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
}  // namespace

int main() {
	CHECK(kPsiDb.count >= 55);
	CHECK(kPsiDb.count < PSI_MAX_ITEMS);
	CHECK(psiFindDef(&kPsiDb, "long_sword") == 0);
	CHECK(psiFindDef(&kPsiDb, "inconnu") < 0);

	// Six epees A2, puissances 1 a 6, sans aucune arme Alundra 1.
	const char* swords[6] = {"long_sword", "bronze_sword", "broad_sword",
	                         "rune_sword", "shining_sword", "rising_sword"};
	for (int i = 0; i < 6; ++i) {
		const int d = psiFindDef(&kPsiDb, swords[i]);
		CHECK(d >= 0);
		if (d >= 0) {
			CHECK(kPsiDb.defs[d].kind == PSI_KIND_WEAPON);
			CHECK(kPsiDb.defs[d].power == i + 1);
		}
	}
	for (int i = 0; i < kPsiDb.count; ++i) {
		CHECK(kPsiDb.defs[i].code[0] != '\0');
		CHECK(kPsiDb.defs[i].name[0] != '\0');
	}
	const int herb = psiFindDef(&kPsiDb, "herb");
	CHECK(herb >= 0 && kPsiDb.defs[herb].maxQty == 9 && kPsiDb.defs[herb].consumable == 1);
	const int key = psiFindDef(&kPsiDb, "strange_key");
	CHECK(key >= 0 && kPsiDb.defs[key].kind == PSI_KIND_KEY && kPsiDb.defs[key].consumable == 0);
	for (const char* gate : {"epee_supreme", "lampe", "prisme", "cristal"})
		CHECK(psiFindDef(&kPsiDb, gate) >= 0);

	// Le texte est repli en Latin-1 : un seul octet par glyphe FONT3.
	const int epee = psiFindDef(&kPsiDb, "long_sword");
	const unsigned char* n = reinterpret_cast<const unsigned char*>(kPsiDb.defs[epee].name);
	CHECK(n[0] == 0xC9 && n[1] == 'p' && n[2] == 0xE9);  // "Épée"

	// Les pages exposent une liste stable pour le rendu et le curseur.
	CHECK(psiPageCount(&kPsiDb, PSI_PAGE_EQUIPMENT) == 3);
	CHECK(psiPageDefAt(&kPsiDb, PSI_PAGE_WEAPONS, 0) == 0);
	CHECK(psiPageDefAt(&kPsiDb, PSI_PAGE_ITEMS, 0) == psiFindDef(&kPsiDb, "herb"));
	CHECK(psiPageDefAt(&kPsiDb, PSI_PAGE_ITEMS, 999) < 0);

	PSIInventory inv;
	psiInit(&inv, &kPsiDb);
	CHECK(psiQty(&inv, &kPsiDb, "long_sword") == 1);
	CHECK(inv.weapon == 0);
	psiAdd(&inv, &kPsiDb, "herb", 12);
	CHECK(psiQty(&inv, &kPsiDb, "herb") == 9);
	CHECK(psiUse(&inv, &kPsiDb, "herb") == 1);
	CHECK(psiQty(&inv, &kPsiDb, "herb") == 8);

	if (g_fail == 0) std::printf("PS1_INVENTORY_DATA %d/%d\n", g_checks, g_checks);
	return g_fail == 0 ? 0 : 1;
}
