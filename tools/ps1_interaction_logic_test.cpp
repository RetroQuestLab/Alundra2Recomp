/*
 * Test hote de la premiere interaction PS1 (TDD) : declenchement par approche,
 * front montant de CROIX, etat de quete, consequence observable, gates intact.
 * Compile le VRAI header utilise par le build MIPS ; aucun mock, aucun SDK.
 */
#include "../src/ps1/quest_ps1_logic.h"
#include "../src/ps1/world_ps1_logic.h"

#include <cstdio>

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

void tap(QuestState& s, uint32_t bit, int hx, int hy, int dir) {
	questStep(&s, bit, hx, hy, dir);
	questStep(&s, 0, hx, hy, dir);
}
}  // namespace

int main() {
	// --- etat initial : aucun dialogue ouvert, aucun PNJ actif ----------------------
	{
		QuestState s;
		questInit(&s);
		CHECK(s.stage == QST_STAGE_ARRIVEE);
		CHECK(s.activeNpc == QST_NPC_NONE);
		CHECK(s.dialogueId == QST_DLG_NONE);
		CHECK(s.talkedPrunewell == 0);
		CHECK(s.talkedGarde == 0);
		CHECK(s.gates == 0);
	}

	// --- hors de portee depuis le spawn reel (660,760) -----------------------------
	{
		QuestState s;
		questInit(&s);
		CHECK(questNpcNear(&s, 660, 760, PF_DIR_RIGHT) == QST_NPC_NONE);
		tap(s, PF_BTN_CROSS, 660, 760, PF_DIR_RIGHT);
		CHECK(s.activeNpc == QST_NPC_NONE);
		CHECK(s.dialogueId == QST_DLG_NONE);
	}

	// --- approche de Prunewell (31,47) -> centre pixel (756,760) --------------------
	{
		QuestState s;
		questInit(&s);
		CHECK(questNpcNear(&s, 742, 760, PF_DIR_RIGHT) == QST_NPC_PRUNWELL);
		tap(s, PF_BTN_CROSS, 742, 760, PF_DIR_RIGHT);
		CHECK(s.activeNpc == QST_NPC_PRUNWELL);
		CHECK(s.dialogueId == QST_DLG_PRUNWELL);
		CHECK(s.stage == QST_STAGE_ARRIVEE);  // l'etat ne change qu'a la fin du dialogue

		// Un CROIX maintenu ne re-declenche pas le dialogue.
		for (int i = 0; i < 20; ++i) questStep(&s, PF_BTN_CROSS, 742, 760, PF_DIR_RIGHT);
		CHECK(s.dialogueId == QST_DLG_PRUNWELL);
		CHECK(s.activeNpc == QST_NPC_PRUNWELL);

		questDialogueDone(&s);
		CHECK(s.dialogueId == QST_DLG_NONE);
		CHECK(s.activeNpc == QST_NPC_NONE);
		CHECK(s.talkedPrunewell == 1);
		CHECK(s.stage == QST_STAGE_MONT_EPERVIER);
		CHECK(questObjectiveVisible(&s) == 1);
	}

	// --- consequence observable : le garde de Paco ne s'active qu'apres Prunewell ---
	{
		QuestState s;
		questInit(&s);
		CHECK(questNpcNear(&s, 814, 792, PF_DIR_RIGHT) == QST_NPC_NONE);
		questDialogueDoneAfter(&s, QST_DLG_PRUNWELL);
		CHECK(questNpcNear(&s, 814, 792, PF_DIR_RIGHT) == QST_NPC_GARDE);
		CHECK(questNpcBubble(&s, QST_NPC_GARDE) == 1);

		tap(s, PF_BTN_CROSS, 814, 792, PF_DIR_RIGHT);
		CHECK(s.dialogueId == QST_DLG_GARDE);
		questDialogueDone(&s);
		CHECK(s.talkedGarde == 1);
		CHECK(s.stage == QST_STAGE_PACO_PREVENU);
		CHECK(questObjectiveVisible(&s) == 1);
	}

	// --- re-parler a Prunewell ne fait pas reculer la quete -------------------------
	{
		QuestState s;
		questInit(&s);
		questDialogueDoneAfter(&s, QST_DLG_PRUNWELL);
		questDialogueDoneAfter(&s, QST_DLG_GARDE);
		tap(s, PF_BTN_CROSS, 742, 760, PF_DIR_RIGHT);
		CHECK(s.dialogueId == QST_DLG_PRUNWELL);
		questDialogueDone(&s);
		CHECK(s.stage == QST_STAGE_PACO_PREVENU);
	}

	// --- nouvelle partie : tout est reinitalise, y compris les gates ---------------
	{
		QuestState s;
		questInit(&s);
		questDialogueDoneAfter(&s, QST_DLG_PRUNWELL);
		questDialogueDoneAfter(&s, QST_DLG_GARDE);
		questInit(&s);
		CHECK(s.stage == QST_STAGE_ARRIVEE);
		CHECK(s.talkedPrunewell == 0);
		CHECK(s.talkedGarde == 0);
		CHECK(s.gates == 0);
	}
	// --- v0.5.0 : Kindra, Finrod et le marchand depouille -------------------------
	{
		QuestState s;
		questInit(&s);
		CHECK(s.zone == QST_ZONE_PACO);
		CHECK(questNpcAvailable(&s, QST_NPC_PRUNWELL) == 1);
		questSetZone(&s, QST_ZONE_KINDRA);
		CHECK(s.zone == QST_ZONE_KINDRA);
		CHECK(questNpcAvailable(&s, QST_NPC_PRUNWELL) == 0);
		CHECK(questNpcAvailable(&s, QST_NPC_FINROD) == 1);
		CHECK(questNpcAvailable(&s, QST_NPC_MARCHAND_KINDRA) == 1);
		CHECK(questNpcNear(&s, 982, 888, PF_DIR_RIGHT) == QST_NPC_FINROD);
		CHECK(questNpcNear(&s, 238, 920, PF_DIR_RIGHT) == QST_NPC_MARCHAND_KINDRA);
		tap(s, PF_BTN_CROSS, 982, 888, PF_DIR_RIGHT);
		CHECK(s.dialogueId == QST_DLG_FINROD);
		questDialogueDone(&s);
		CHECK(s.talkedFinrod == 1);
		CHECK(s.stage == QST_STAGE_KINDRA);
		CHECK(s.gates == 0);
	}

	// --- v0.5.0 : Mont Epervier, Alexia et Belgar --------------------------------
	{
		QuestState s;
		questInit(&s);
		questSetZone(&s, QST_ZONE_EPAVE);
		CHECK(questNpcNear(&s, 238, 184, PF_DIR_RIGHT) == QST_NPC_ALEXIA_EPAVE);
		CHECK(questNpcNear(&s, 286, 200, PF_DIR_RIGHT) == QST_NPC_BELGAR_EPAVE);
		tap(s, PF_BTN_CROSS, 238, 184, PF_DIR_RIGHT);
		CHECK(s.dialogueId == QST_DLG_ALEXIA_EPAVE);
		questDialogueDone(&s);
		CHECK(s.talkedAlexiaEpave == 1);
		CHECK(s.stage == QST_STAGE_EPAVE);
		CHECK(s.gates == 0);
	}

	// --- v0.5.0 : Toroledo, Natasha et Jeehan ------------------------------------
	{
		QuestState s;
		questInit(&s);
		questSetZone(&s, QST_ZONE_TOLEDO);
		CHECK(questNpcNear(&s, 238, 552, PF_DIR_RIGHT) == QST_NPC_NATASHA);
		CHECK(questNpcNear(&s, 550, 200, PF_DIR_RIGHT) == QST_NPC_JEEHAN);
		tap(s, PF_BTN_CROSS, 550, 200, PF_DIR_RIGHT);
		CHECK(s.dialogueId == QST_DLG_JEEHAN);
		questDialogueDone(&s);
		CHECK(s.talkedJeehan == 1);
		CHECK(s.stage == QST_STAGE_TOLEDO);
		CHECK(s.gates == 0);
	}

	// --- v0.6.0 etape 1 : cle etrange -> Alexia -> ogre -> victoire ---------------
	{
		QuestState s;
		questInit(&s);
		CHECK(s.hasStrangeKey == 0);
		CHECK(s.bossDown == 0);
		CHECK(s.chestLettre == 0);
		CHECK(s.gates == 0);

		// Le coffre a cle est dans la zone 019, en (7,16) : centre (180,264).
		questSetZone(&s, QST_ZONE_EPAVE);
		CHECK(questChestAvailable(&s, QST_CHEST_STRANGE_KEY) == 1);
		CHECK(questChestIdAt(&s, 180, 264) == QST_CHEST_STRANGE_KEY);
		CHECK(questChestTake(&s, QST_CHEST_STRANGE_KEY) == QST_DLG_OBJET_STRANGE_KEY);
		CHECK(s.hasStrangeKey == 1);
		CHECK(s.dialogueId == QST_DLG_OBJET_STRANGE_KEY);
		questDialogueDone(&s);
		CHECK(questChestAvailable(&s, QST_CHEST_STRANGE_KEY) == 0);
		CHECK(questChestTake(&s, QST_CHEST_STRANGE_KEY) == QST_DLG_NONE);

		// Le dialogue d'Alexia change apres la cle (condition PC `si objet_strange_key`).
		CHECK(questNpcDialogue(&s, QST_NPC_ALEXIA_EPAVE) == QST_DLG_ALEXIA_EPAVE_CLE);
		tap(s, PF_BTN_CROSS, 238, 184, PF_DIR_RIGHT);
		CHECK(s.dialogueId == QST_DLG_ALEXIA_EPAVE_CLE);
		questDialogueDone(&s);
		CHECK(s.talkedAlexiaEpave == 1);

		// Avant la victoire, le coffre que gardait l'ogre (013, 22,31) est ferme.
		questSetZone(&s, QST_ZONE_KINDRA);
		CHECK(questChestAvailable(&s, QST_CHEST_LETTRE) == 0);
		questOgreDefeated(&s);
		CHECK(s.bossDown == 1);
		CHECK(s.stage == QST_STAGE_BOSS_VAINCU);
		CHECK(s.dialogueId == QST_DLG_VICTOIRE);
		questDialogueDone(&s);
		CHECK(questChestAvailable(&s, QST_CHEST_LETTRE) == 1);
		CHECK(questChestIdAt(&s, 540, 504) == QST_CHEST_LETTRE);
		CHECK(questChestTake(&s, QST_CHEST_LETTRE) != QST_DLG_NONE);
		CHECK(s.chestLettre == 1);
		CHECK(questChestAvailable(&s, QST_CHEST_LETTRE) == 0);
		questDialogueDone(&s);

		// Un second affrontement ne re-ouvre ni la victoire ni le coffre.
		questOgreDefeated(&s);
		CHECK(s.dialogueId == QST_DLG_NONE);
		CHECK(s.chestLettre == 1);
		CHECK(s.gates == 0);
	}

	// --- v0.6.0 etape 1 : acteurs interactifs (ogre + coffres) --------------------
	{
		QuestState s;
		questInit(&s);
		questSetZone(&s, QST_ZONE_KINDRA);
		CHECK(questActorAvailable(&s, QST_ACTOR_OGRE) == 1);
		CHECK(questActorAvailable(&s, QST_ACTOR_CHEST_LETTRE) == 0);
		CHECK(questActorNear(&s, 540, 472, PF_DIR_RIGHT) == QST_ACTOR_OGRE);
		CHECK(questHasInteract(&s, 540, 472, PF_DIR_RIGHT) == 1);
		// CROIX ne combat pas : la logique pure refuse le contournement, meme si
		// le pilote ne masquait pas le bouton (PNJ proche, regression future).
		tap(s, PF_BTN_CROSS, 540, 472, PF_DIR_RIGHT);
		CHECK(s.bossDown == 0);
		CHECK(s.stage < QST_STAGE_BOSS_VAINCU);
		CHECK(s.dialogueId == QST_DLG_NONE);
		CHECK(questActorInteract(&s, QST_ACTOR_OGRE) == QST_DLG_NONE);
		CHECK(s.bossDown == 0);
		// Seule la victoire reelle du duel (pilote : ogreDuelJustWon) conclut.
		questOgreDefeated(&s);
		CHECK(s.bossDown == 1);
		CHECK(s.dialogueId == QST_DLG_VICTOIRE);
		questDialogueDone(&s);
		questOgreDefeated(&s);  // second appel : aucun double lot
		CHECK(s.dialogueId == QST_DLG_NONE);
		CHECK(questActorAvailable(&s, QST_ACTOR_OGRE) == 0);
		CHECK(questActorAvailable(&s, QST_ACTOR_CHEST_LETTRE) == 1);
		CHECK(questActorNear(&s, 540, 504, PF_DIR_RIGHT) == QST_ACTOR_CHEST_LETTRE);
		tap(s, PF_BTN_CROSS, 540, 504, PF_DIR_RIGHT);
		CHECK(s.chestLettre == 1);
		CHECK(s.dialogueId == QST_DLG_OBJET_LETTRE);
		questDialogueDone(&s);
		CHECK(questHasInteract(&s, 540, 472, PF_DIR_RIGHT) == 0);
		CHECK(questHasInteract(&s, 540, 504, PF_DIR_RIGHT) == 0);

		questSetZone(&s, QST_ZONE_EPAVE);
		CHECK(questActorNear(&s, 180, 264, PF_DIR_RIGHT) == QST_ACTOR_CHEST_KEY);
		CHECK(questActorInteract(&s, QST_ACTOR_CHEST_KEY) == QST_DLG_OBJET_STRANGE_KEY);
		CHECK(s.hasStrangeKey == 1);
		CHECK(questActorAvailable(&s, QST_ACTOR_CHEST_KEY) == 0);
		CHECK(s.gates == 0);
	}

	// --- nouvelle partie : tous les objets et la victoire sont reinitalises --------
	{
		QuestState s;
		questInit(&s);
		questChestTake(&s, QST_CHEST_STRANGE_KEY);
		questOgreDefeated(&s);
		questInit(&s);
		CHECK(s.hasStrangeKey == 0);
		CHECK(s.bossDown == 0);
		CHECK(s.chestLettre == 0);
		CHECK(s.stage == QST_STAGE_ARRIVEE);
		CHECK(s.gates == 0);
	}

	// --- v0.6.0b : salle du trone, chaine Mephisto et vraie fin -----------------
	{
		QuestState s;
		questInit(&s);
		questSetZone(&s, QST_ZONE_TRONE);
		CHECK(s.gates == 0);
		CHECK(s.mephistoDown == 0);
		CHECK(s.ending == 0);
		CHECK(questThroneEnter(&s) == 0);  // sans les cinq gates, rien ne s'ouvre
		CHECK(s.dialogueId == QST_DLG_NONE);

		s.gates = QST_GATE_TRONE_MASK;
		CHECK(questThroneEnter(&s) == 1);
		CHECK(s.stage == QST_STAGE_TRONE);
		CHECK(s.dialogueId == QST_DLG_MEPHISTO_ARRIVEE);
		questDialogueDone(&s);
		CHECK(s.talkedMephistoArrivee == 1);
		CHECK(questThroneEnter(&s) == 0);  // l'arrivee ne se rejoue pas

		// Mephisto : arrivee -> dialogue de trone -> victoire.
		CHECK(questActorAvailable(&s, QST_ACTOR_MEPHISTO) == 1);
		CHECK(questActorNear(&s, 492, 424, PF_DIR_RIGHT) == QST_ACTOR_MEPHISTO);
		tap(s, PF_BTN_CROSS, 492, 424, PF_DIR_RIGHT);
		CHECK(s.dialogueId == QST_DLG_MEPHISTO_TONE);
		questDialogueDone(&s);
		CHECK(s.talkedMephistoTrone == 1);
		tap(s, PF_BTN_CROSS, 492, 424, PF_DIR_RIGHT);
		CHECK(s.mephistoDown == 1);
		CHECK(s.dialogueId == QST_DLG_MEPHISTO_VAINCU);
		CHECK(s.ending == 0);  // la fin n'apparait pas a la victoire seule
		questDialogueDone(&s);
		CHECK(questActorAvailable(&s, QST_ACTOR_MEPHISTO) == 0);
		CHECK(questEnding(&s) == 0);  // il faut aussi la derniere replique d'Alexia

		// Roi et Alexia : variantes apres la victoire.
		CHECK(questActorAvailable(&s, QST_ACTOR_ROI) == 1);
		CHECK(questActorAvailable(&s, QST_ACTOR_ALEXIA_FINALE) == 1);
		CHECK(questActorNear(&s, 324, 680, PF_DIR_RIGHT) == QST_ACTOR_ROI);
		CHECK(questActorNear(&s, 324, 840, PF_DIR_RIGHT) == QST_ACTOR_ALEXIA_FINALE);
		tap(s, PF_BTN_CROSS, 324, 680, PF_DIR_RIGHT);
		CHECK(s.dialogueId == QST_DLG_ROI_VAINCU);
		questDialogueDone(&s);
		CHECK(s.ending == 0);
		tap(s, PF_BTN_CROSS, 324, 840, PF_DIR_RIGHT);
		CHECK(s.dialogueId == QST_DLG_ALEXIA_VAINCU);
		questDialogueDone(&s);
		CHECK(s.ending == 1);
		CHECK(s.stage == QST_STAGE_ENDING);
		CHECK(s.gates == QST_GATE_TRONE_MASK);  // aucun gate modifie par la victoire
	}

	// --- variantes avant la victoire : dialogues de trone, sans fin ---------------
	{
		QuestState s;
		questInit(&s);
		questSetZone(&s, QST_ZONE_TRONE);
		s.gates = QST_GATE_TRONE_MASK;
		questThroneEnter(&s);
		questDialogueDone(&s);
		tap(s, PF_BTN_CROSS, 324, 840, PF_DIR_RIGHT);
		CHECK(s.dialogueId == QST_DLG_ALEXIA_FINALE);
		questDialogueDone(&s);
		tap(s, PF_BTN_CROSS, 324, 680, PF_DIR_RIGHT);
		CHECK(s.dialogueId == QST_DLG_ROI_TONE);
		questDialogueDone(&s);
		CHECK(s.ending == 0);
		CHECK(s.mephistoDown == 0);
	}

	// --- nouvelle partie : le trone et la fin sont reinitalises -------------------
	{
		QuestState s;
		questInit(&s);
		s.gates = QST_GATE_TRONE_MASK;
		questMephistoDefeated(&s);
		questDialogueDone(&s);
		questEnding(&s);
		questInit(&s);
		CHECK(s.gates == 0);
		CHECK(s.mephistoDown == 0);
		CHECK(s.ending == 0);
		CHECK(s.talkedMephistoArrivee == 0);
		CHECK(s.stage == QST_STAGE_ARRIVEE);
	}

	// --- retrouvailles Prunewell / Finrod (A2 bloc 32, dialogue inedit) ---------
	{
		QuestState s;
		questInit(&s);
		questSetZone(&s, QST_ZONE_PACO);
		int px = 0, py = 0;
		questNpcPos(QST_NPC_PRUNWELL, &px, &py);
		CHECK(questNpcDialogue(&s, QST_NPC_FINROD) == QST_DLG_FINROD);
		tap(s, PF_BTN_CROSS, px, py, PF_DIR_RIGHT);
		CHECK(s.dialogueId == QST_DLG_PRUNWELL);
		CHECK(s.talkedPrunewell == 0);
		questDialogueDone(&s);
		CHECK(s.talkedPrunewell == 1);
		CHECK(questNpcDialogue(&s, QST_NPC_FINROD) == QST_DLG_FINROD_REUNION);
		questSetZone(&s, QST_ZONE_KINDRA);
		int fx = 0, fy = 0;
		questNpcPos(QST_NPC_FINROD, &fx, &fy);
		tap(s, PF_BTN_CROSS, fx, fy, PF_DIR_RIGHT);
		CHECK(s.dialogueId == QST_DLG_FINROD_REUNION);
		CHECK(s.talkedFinrod == 0);
		questDialogueDone(&s);
		CHECK(s.talkedFinrod == 1);
		CHECK(s.stage >= QST_STAGE_KINDRA);
	}

	// --- Alexia rattrape Flint en foret Kindra (A2 blocs 30-31) ---------------
	{
		QuestState s;
		questInit(&s);
		questSetZone(&s, QST_ZONE_KINDRA);
		int ax = 0, ay = 0;
		questNpcPos(QST_NPC_ALEXIA_KINDRA, &ax, &ay);
		CHECK(ax == 228);
		CHECK(ay == 808);
		CHECK(questNpcAvailable(&s, QST_NPC_ALEXIA_KINDRA) == 1);
		CHECK(questNpcDialogue(&s, QST_NPC_ALEXIA_KINDRA) == QST_DLG_ALEXIA_KINDRA);
		tap(s, PF_BTN_CROSS, ax - QST_FWD_X, ay, PF_DIR_RIGHT);
		CHECK(s.dialogueId == QST_DLG_ALEXIA_KINDRA);
		questDialogueDone(&s);
		CHECK(s.gates == 0);
	}

	// --- Ordres de Diaz -> Jeehan -> epee supreme -> Vallee de Mithra --------
	// data/world.txt : chest 119 25 1 ordre ; items.cpp : Jeehan remet
	// l'epee a qui apporte les ordres ; warps.txt : 119->026 epee_supreme.
	{
		QuestState s;
		questInit(&s);
		questSetZone(&s, QST_ZONE_TOLEDO);
		int jx = 0, jy = 0;
		questNpcPos(QST_NPC_JEEHAN, &jx, &jy);
		// Premiere visite : conseil classique, pas d'epee sans les ordres.
		CHECK(questNpcDialogue(&s, QST_NPC_JEEHAN) == QST_DLG_JEEHAN);
		tap(s, PF_BTN_CROSS, jx - QST_FWD_X, jy, PF_DIR_RIGHT);
		CHECK(s.dialogueId == QST_DLG_JEEHAN);
		questDialogueDone(&s);
		CHECK((s.gates & QST_GATE_EPEE_SUPREME) == 0);
		CHECK(questNpcBubble(&s, QST_NPC_JEEHAN) == 0);

		// Coffre des ordres : case (25,1), une seule fois.
		int cx = 0, cy = 0;
		questActorPos(QST_ACTOR_CHEST_ORDRE, &cx, &cy);
		CHECK(cx == 25 * 24 + 12);
		CHECK(cy == 1 * 16 + 8);
		CHECK(questActorAvailable(&s, QST_ACTOR_CHEST_ORDRE) == 1);
		CHECK(questActorNear(&s, cx, cy, PF_DIR_RIGHT) == QST_ACTOR_CHEST_ORDRE);
		tap(s, PF_BTN_CROSS, cx, cy, PF_DIR_RIGHT);
		CHECK(s.dialogueId == QST_DLG_OBJET_ORDRE);
		CHECK((s.gates & QST_FLAG_ORDRE) != 0);
		questDialogueDone(&s);
		CHECK(questActorAvailable(&s, QST_ACTOR_CHEST_ORDRE) == 0);
		tap(s, PF_BTN_CROSS, cx, cy, PF_DIR_RIGHT);
		CHECK(s.dialogueId == QST_DLG_NONE);  // coffre deja vide

		// Jeehan redevient disponible et remet l'epee, une seule fois.
		CHECK(questNpcBubble(&s, QST_NPC_JEEHAN) == 1);
		CHECK(questNpcDialogue(&s, QST_NPC_JEEHAN) == QST_DLG_JEEHAN_ORDRE);
		tap(s, PF_BTN_CROSS, jx - QST_FWD_X, jy, PF_DIR_RIGHT);
		CHECK(s.dialogueId == QST_DLG_JEEHAN_ORDRE);
		CHECK((s.gates & QST_GATE_EPEE_SUPREME) == 0);  // pas avant la fin du dialogue
		questDialogueDone(&s);
		CHECK((s.gates & QST_GATE_EPEE_SUPREME) != 0);
		CHECK(questNpcBubble(&s, QST_NPC_JEEHAN) == 0);
		CHECK(worldTroneGatesComplete(s.gates) == 0);

		// Vallee : narration d'arrivee une seule fois.
		questSetZone(&s, QST_ZONE_VALLEE);
		CHECK(questZoneArrival(&s) == QST_DLG_VALLEE_ARRIVEE);
		questDialogueDone(&s);
		CHECK((s.gates & QST_FLAG_VALLEE_VUE) != 0);
		questSetZone(&s, QST_ZONE_TOLEDO);
		questSetZone(&s, QST_ZONE_VALLEE);
		CHECK(questZoneArrival(&s) == QST_DLG_NONE);
		CHECK(s.dialogueId == QST_DLG_NONE);
	}
	// --- Guide de la Vallee de Mithra (data/world.txt : npc 026 25 22) -------
	{
		QuestState s;
		questInit(&s);
		questSetZone(&s, QST_ZONE_VALLEE);
		s.gates |= QST_FLAG_VALLEE_VUE;  // narration deja lue
		int gx = 0, gy = 0;
		questNpcPos(QST_NPC_GUIDE_VALLEE, &gx, &gy);
		CHECK(gx == 25 * 24 + 12);
		CHECK(gy == 22 * 16 + 8);
		CHECK(questNpcAvailable(&s, QST_NPC_GUIDE_VALLEE) == 1);
		CHECK(questNpcBubble(&s, QST_NPC_GUIDE_VALLEE) == 1);
		tap(s, PF_BTN_CROSS, gx - QST_FWD_X, gy, PF_DIR_RIGHT);
		CHECK(s.dialogueId == QST_DLG_GUIDE_VALLEE);
		questDialogueDone(&s);
		CHECK(questNpcBubble(&s, QST_NPC_GUIDE_VALLEE) == 0);
		CHECK(worldTroneGatesComplete(s.gates) == 0);
		CHECK((s.gates & QST_GATE_LAMPE) == 0);  // la lampe vient du golem
		questSetZone(&s, QST_ZONE_TOLEDO);
		CHECK(questNpcAvailable(&s, QST_NPC_GUIDE_VALLEE) == 0);
	}
	// --- Golem de la vallee et Lampe de Lumiere (world.txt : spawn 026 26 28
	// golem ; chest 026 26 29 lampe boss_026 ; warps : 026->042 lampe) ------
	{
		QuestState s;
		questInit(&s);
		questSetZone(&s, QST_ZONE_VALLEE);
		s.gates |= QST_FLAG_VALLEE_VUE;
		int gx = 0, gy = 0, cx = 0, cy = 0;
		questActorPos(QST_ACTOR_GOLEM, &gx, &gy);
		questActorPos(QST_ACTOR_CHEST_LAMPE, &cx, &cy);
		CHECK(gx == 26 * 24 + 12 && gy == 28 * 16 + 8);
		CHECK(cx == 26 * 24 + 12 && cy == 29 * 16 + 8);
		CHECK(questActorAvailable(&s, QST_ACTOR_GOLEM) == 1);
		CHECK(questActorAvailable(&s, QST_ACTOR_CHEST_LAMPE) == 0);
		// CROIX ne combat pas, meme directement sur l'acteur.
		tap(s, PF_BTN_CROSS, gx, gy, PF_DIR_RIGHT);
		CHECK(s.dialogueId == QST_DLG_NONE);
		CHECK(questActorInteract(&s, QST_ACTOR_GOLEM) == QST_DLG_NONE);
		CHECK((s.gates & QST_FLAG_GOLEM) == 0);
		CHECK(questChestTake(&s, QST_CHEST_LAMPE) == QST_DLG_NONE);  // coffre scelle
		// Victoire reelle (pilote : ogreDuelJustWon sur l'etat du golem).
		CHECK(questGolemDefeated(&s) == 1);
		CHECK(s.dialogueId == QST_DLG_BOSS_VALLEE);
		questDialogueDone(&s);
		CHECK(questGolemDefeated(&s) == 0);  // une seule fois
		CHECK(s.dialogueId == QST_DLG_NONE);
		CHECK(questActorAvailable(&s, QST_ACTOR_GOLEM) == 0);
		CHECK(questActorAvailable(&s, QST_ACTOR_CHEST_LAMPE) == 1);
		CHECK((s.gates & QST_GATE_LAMPE) == 0);
		tap(s, PF_BTN_CROSS, cx, cy - QST_FWD_Y, PF_DIR_DOWN);
		CHECK(s.dialogueId == QST_DLG_OBJET_LAMPE);
		CHECK((s.gates & QST_GATE_LAMPE) != 0);
		questDialogueDone(&s);
		CHECK(questActorAvailable(&s, QST_ACTOR_CHEST_LAMPE) == 0);
		CHECK(questChestTake(&s, QST_CHEST_LAMPE) == QST_DLG_NONE);
		CHECK(worldTroneGatesComplete(s.gates) == 0);
		// Hors vallee : ni golem ni coffre.
		questSetZone(&s, QST_ZONE_TOLEDO);
		CHECK(questActorAvailable(&s, QST_ACTOR_GOLEM) == 0);
		// Le golem ne doit pas s'afficher sur le terrain de l'ogre.
		questSetZone(&s, QST_ZONE_KINDRA);
		CHECK(questActorAvailable(&s, QST_ACTOR_GOLEM) == 0);
	}
	// --- Arrivee aux Ruines d'Eden (event 042 arrivee_eden eden_arrivee) ----
	{
		QuestState s;
		questInit(&s);
		questSetZone(&s, QST_ZONE_EDEN);
		CHECK(questZoneArrival(&s) == QST_DLG_EDEN_ARRIVEE);
		questDialogueDone(&s);
		CHECK((s.gates & QST_FLAG_EDEN_VU) != 0);
		CHECK(questZoneArrival(&s) == QST_DLG_NONE);
		CHECK((s.gates & QST_FLAG_VALLEE_VUE) == 0);  // drapeaux distincts
	}
	// --- Ruines d'Eden : Roswell, Omega, second golem, Prisme ---------------
	{
		QuestState s;
		questInit(&s);
		questSetZone(&s, QST_ZONE_EDEN);
		s.gates |= QST_FLAG_EDEN_VU | QST_GATE_EPEE_SUPREME | QST_GATE_LAMPE;
		int rx = 0, ry = 0, ox = 0, oy = 0;
		questNpcPos(QST_NPC_ROSWELL, &rx, &ry);
		questNpcPos(QST_NPC_OME, &ox, &oy);
		CHECK(rx == 20 * 24 + 12 && ry == 51 * 16 + 8);
		CHECK(ox == 40 * 24 + 12 && oy == 44 * 16 + 8);
		CHECK(questNpcAvailable(&s, QST_NPC_ROSWELL) == 1);
		CHECK(questNpcAvailable(&s, QST_NPC_OME) == 1);
		// Variant PC : le dernier evenement disponible (lampe) l'emporte.
		CHECK(questNpcDialogue(&s, QST_NPC_ROSWELL) == QST_DLG_ROSWELL_LAMPE);
		s.gates &= ~QST_GATE_LAMPE;
		CHECK(questNpcDialogue(&s, QST_NPC_ROSWELL) == QST_DLG_ROSWELL);
		s.gates |= QST_GATE_LAMPE;
		CHECK(questNpcBubble(&s, QST_NPC_ROSWELL) == 1);
		tap(s, PF_BTN_CROSS, rx + QST_FWD_X, ry, PF_DIR_LEFT);
		CHECK(s.dialogueId == QST_DLG_ROSWELL_LAMPE);
		questDialogueDone(&s);
		CHECK(questNpcBubble(&s, QST_NPC_ROSWELL) == 0);
		CHECK(questNpcDialogue(&s, QST_NPC_OME) == QST_DLG_OME);
		tap(s, PF_BTN_CROSS, ox, oy + QST_FWD_Y, PF_DIR_UP);
		CHECK(s.dialogueId == QST_DLG_OME);
		questDialogueDone(&s);
		CHECK(questNpcBubble(&s, QST_NPC_OME) == 0);
		// Second golem : CROIX sans effet, coffre scelle avant la victoire.
		int gx = 0, gy = 0, cx = 0, cy = 0;
		questActorPos(QST_ACTOR_GOLEM_EDEN, &gx, &gy);
		questActorPos(QST_ACTOR_CHEST_PRISME, &cx, &cy);
		CHECK(gx == 25 * 24 + 12 && gy == 30 * 16 + 8);
		CHECK(cx == 26 * 24 + 12 && cy == 31 * 16 + 8);
		CHECK(questActorAvailable(&s, QST_ACTOR_GOLEM_EDEN) == 1);
		CHECK(questActorAvailable(&s, QST_ACTOR_GOLEM) == 0);  // pas le golem de la vallee
		CHECK(questActorInteract(&s, QST_ACTOR_GOLEM_EDEN) == QST_DLG_NONE);
		CHECK(questChestTake(&s, QST_CHEST_PRISME) == QST_DLG_NONE);
		CHECK(questGolemEdenDefeated(&s) == 1);
		CHECK(s.dialogueId == QST_DLG_BOSS_EDEN);
		questDialogueDone(&s);
		CHECK(questGolemEdenDefeated(&s) == 0);
		CHECK(questActorAvailable(&s, QST_ACTOR_GOLEM_EDEN) == 0);
		CHECK(questActorAvailable(&s, QST_ACTOR_CHEST_PRISME) == 1);
		CHECK(questChestTake(&s, QST_CHEST_PRISME) == QST_DLG_OBJET_PRISME);
		CHECK((s.gates & QST_GATE_PRISME) != 0);
		questDialogueDone(&s);
		CHECK(questChestTake(&s, QST_CHEST_PRISME) == QST_DLG_NONE);
		CHECK(questNpcDialogue(&s, QST_NPC_ROSWELL) == QST_DLG_ROSWELL_PRISME);
		// Un nouveau variant reouvre la bulle de Roswell (nouvelle information).
		CHECK(questNpcBubble(&s, QST_NPC_ROSWELL) == 1);
		CHECK(worldTroneGatesComplete(s.gates) == 0);
		// Le golem de la vallee et celui d'Eden ont des drapeaux distincts.
		CHECK((s.gates & QST_FLAG_GOLEM) == 0);
	}
	// Ordre inverse : les ordres trouves avant la premiere visite a Jeehan.
	{
		QuestState s;
		questInit(&s);
		questSetZone(&s, QST_ZONE_TOLEDO);
		CHECK(questChestTake(&s, QST_CHEST_ORDRE) == QST_DLG_OBJET_ORDRE);
		questDialogueDone(&s);
		CHECK(questNpcDialogue(&s, QST_NPC_JEEHAN) == QST_DLG_JEEHAN_ORDRE);
		CHECK(questZoneArrival(&s) == QST_DLG_NONE);  // Toroledo : pas de narration
		// Aucun coffre ordre hors de Toroledo.
		questSetZone(&s, QST_ZONE_KINDRA);
		CHECK(questActorAvailable(&s, QST_ACTOR_CHEST_ORDRE) == 0);
	}

	if (g_fail != 0) {
		std::printf("PS1_INTERACTION_LOGIC: %d/%d checks FAIL\n", g_checks - g_fail, g_checks);
		return 1;
	}
	std::printf("PS1_INTERACTION_LOGIC: %d/%d checks PASS\n", g_checks, g_checks);
	return 0;
}
