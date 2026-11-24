/* Regression hote : table de passages PS1 (A→Z, sans modifier les gates). */
#include "../src/ps1/world_ps1_logic.h"

#include <stdio.h>

static int checks = 0;

static void expectWarp(int zone, int cx, int cy, int tz, int tx, int ty) {
	int oz = 0, ox = 0, oy = 0;
	const int hit = worldWarpAt(zone, cx, cy, &oz, &ox, &oy);
	if (!hit || oz != tz || ox != tx || oy != ty) {
		printf("FAIL warp zone=%d (%d,%d) -> %d/%d/%d (attendu %d/%d/%d)\n",
		       zone, cx, cy, hit, oz, ox, tz, tx, ty);
		return;
	}
	++checks;
}

static void expectNone(int zone, int cx, int cy) {
	if (worldWarpAt(zone, cx, cy, 0, 0, 0)) {
		printf("FAIL warp inattendu zone=%d (%d,%d)\n", zone, cx, cy);
		return;
	}
	++checks;
}

static void expectTracker(int value, const char* what) {
	if (!value) {
		printf("FAIL tracker : %s\n", what);
		return;
	}
	++checks;
}

int main() {
	// data/warps.txt, sans aucun gate : la route A→Z reste intacte.
	expectWarp(11, 25, 45, 13, 8, 54);
	expectWarp(11, 27, 45, 13, 8, 54);
	expectWarp(13, 8, 57, 11, 26, 47);
	expectWarp(13, 12, 57, 11, 26, 47);
	expectWarp(13, 14, 50, 13, 41, 54);
	expectWarp(13, 31, 54, 13, 14, 52);
	expectWarp(13, 41, 53, 19, 10, 12);  // Kindra -> Mont Epervier / epave
	expectWarp(19, 13, 50, 13, 31, 54);  // retour de l'epave
	expectWarp(19, 14, 52, 119, 25, 50); // pied de l'epave -> Toroledo
	expectWarp(119, 25, 49, 19, 14, 52); // retour vers l'epave
	expectNone(11, 14, 50);  // la case existe, mais dans une autre zone
	expectNone(13, 25, 45);  // pareil dans l'autre sens
	expectNone(11, 25, 46);
	expectNone(13, 8, 56);
	expectNone(19, 25, 49);  // la case de Toroledo n'existe pas dans l'epave

	// Le retour Toroledo -> Epave arrive SUR le depart Epave -> Toroledo :
	// sans verrou, le heros ferait des allers-retours infinis.
	{
		WorldWarpState st;
		worldWarpStateInit(&st);
		int z = 0, x = 0, y = 0;
		expectTracker(worldTryWarp(&st, 119, 25, 49, &z, &x, &y) == 1 &&
		              z == 19 && x == 14 && y == 52, "depart Toroledo");
		worldWarpStateLock(&st, z, x, y);
		expectTracker(worldTryWarp(&st, 19, 14, 52, 0, 0, 0) == 0,
		              "arrivee verrouillee sur le depart");
		expectTracker(worldTryWarp(&st, 19, 13, 49, 0, 0, 0) == 0,
		              "case hors passage : verrou relache");
		expectTracker(worldTryWarp(&st, 19, 14, 52, 0, 0, 0) == 1,
		              "passage re-armee apres avoir quitte la case");
	}

	// Salle du trone : la donnee de data/warps.txt ne suffit pas. La boucle doit
	// refuser 086->120 tant que la chaine epee_supreme -> lampe -> prisme ->
	// boss_medusa -> cristal n'est pas complete.
	expectNone(86, 26, 8);  // pas de passage inconditionnel dans la table A->Z
	{
		WorldWarpState st;
		worldWarpStateInit(&st);
		int z = 0, x = 0, y = 0;
		expectTracker(worldTryWarp120(0, 86, 26, 8, &z, &x, &y) == 0,
		              "trone refuse sans gates");
		expectTracker(worldTryWarp120(QST_GATE_EPEE_SUPREME | QST_GATE_LAMPE |
		                              QST_GATE_PRISME | QST_GATE_CRISTAL,
		                              86, 26, 8, 0, 0, 0) == 0,
		              "trone refuse sans boss_medusa");
		expectTracker(worldTryWarp120(QST_GATE_TRONE_MASK, 86, 26, 8,
		                              &z, &x, &y) == 1 && z == QST_ZONE_TRONE &&
		              x == 14 && y == 52, "trone ouvert apres la chaine complete");
		expectTracker(worldTryWarp120(QST_GATE_TRONE_MASK, 86, 26, 9,
		                              0, 0, 0) == 0,
		              "hors de la rectangle de depart : pas de trone");
		expectTracker(worldTryWarp120(QST_GATE_TRONE_MASK | (1u << 7), 86, 26, 8,
		                              0, 0, 0) == 1,
		              "un gate etranger ne bloque pas le trone");
		// Le retour du trone vers le manoir reste libre (aucun gate).
		worldWarpStateInit(&st);
		expectTracker(worldTryWarp(&st, QST_ZONE_TRONE, 14, 52,
		                           &z, &x, &y) == 1 && z == 86 && x == 27 && y == 8,
		              "retour trone -> manoir libre");
	}
	// Toroledo -> Vallee de Mithra : data/warps.txt
	// `119 26 44 2 1 -> 026 26 52 haut epee_supreme` et retour libre.
	expectNone(119, 26, 44);  // jamais dans la table inconditionnelle
	expectWarp(26, 26, 52, 119, 26, 44);
	expectWarp(26, 27, 52, 119, 26, 44);
	{
		WorldWarpState st;
		worldWarpStateInit(&st);
		int z = 0, x = 0, y = 0;
		expectTracker(worldTryWarpGated(&st, 0, 119, 26, 44, &z, &x, &y) == 0,
		              "vallee refusee sans epee_supreme");
		expectTracker(worldTryWarpGated(&st, QST_GATE_LAMPE, 119, 27, 44, 0, 0, 0) == 0,
		              "un autre gate n'ouvre pas la vallee");
		expectTracker(worldTryWarpGated(&st, QST_GATE_EPEE_SUPREME, 119, 26, 44,
		                                &z, &x, &y) == 1 && z == QST_ZONE_VALLEE &&
		              x == 26 && y == 52, "vallee ouverte avec l'epee");
		expectTracker(worldTryWarpGated(&st, QST_GATE_EPEE_SUPREME, 119, 28, 44,
		                                0, 0, 0) == 0, "hors rectangle : pas de vallee");
		// L'arrivee en vallee tombe sur le retour : le verrou empeche le rebond.
		worldWarpStateLock(&st, z, x, y);
		expectTracker(worldTryWarp(&st, QST_ZONE_VALLEE, 26, 52, 0, 0, 0) == 0,
		              "arrivee vallee verrouillee");
		// Retour vers Toroledo : l'arrivee (26,44) est sur le depart garde.
		worldWarpStateInit(&st);
		expectTracker(worldTryWarp(&st, QST_ZONE_VALLEE, 26, 51, 0, 0, 0) == 0,
		              "case voisine : pas de retour");
		expectTracker(worldTryWarp(&st, QST_ZONE_VALLEE, 27, 52, &z, &x, &y) == 1 &&
		              z == 119, "retour libre vers Toroledo");
		worldWarpStateLock(&st, z, x, y);
		expectTracker(worldTryWarpGated(&st, QST_GATE_EPEE_SUPREME, 119, 26, 44,
		                                0, 0, 0) == 0, "arrivee Toroledo verrouillee");
	}
	printf("PS1_WORLD_LOGIC: %d/%d checks PASS\n", checks, checks);
	return checks == 37 ? 0 : 1;
}
