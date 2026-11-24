#pragma once
/*
 * Logique de quete PS1 (011 Paco, 013 Kindra, 019 Epave, 119 Toroledo,
 * 120 salle du trone).
 *
 * Logique pure, sans SDL, sans SDK, sans STL : POD + fonctions inline. Ce header
 * est compile deux fois : par le build MIPS (via interaction_ps1.cpp) et par le
 * test hote tools/ps1_interaction_logic_test.cpp.
 *
 * Regles portees depuis la cible PC (src/main.cpp:477-483, tryInteract) :
 *   - point d'interaction 14 px devant le heros, 10 px en vertical ;
 *   - rayon 20 px (compare en carre, sans libm) ;
 *   - CROIX agit sur FRONT MONTANT (maintenu = un seul declenchement).
 * L'etat de quete ne change qu'a la fermeture du dialogue, jamais a l'ouverture.
 * Les cinq gates A→Z restent a 0 pendant la premiere quete : ils ne sont poses
 * que par le harnais de capture pour authoriser 086→120. Les nouveaux champs
 * sont ajoutes APRES `gates` (offset >= 68) : aucun offset Lua n'est deplace.
 */
#include <stdint.h>

#include "../platform/pf.h"
#include "world_ps1_logic.h"

#define QST_STAGE_ARRIVEE        0
#define QST_STAGE_MONT_EPERVIER  1
#define QST_STAGE_PACO_PREVENU   2
#define QST_STAGE_KINDRA         3
#define QST_STAGE_EPAVE          4
#define QST_STAGE_TOLEDO         5
#define QST_STAGE_BOSS_VAINCU    6
#define QST_STAGE_TRONE          7
#define QST_STAGE_TRONE_VAINCU   8
#define QST_STAGE_ENDING         9

#define QST_EVENT_MEPHISTO_VAINCU "mephisto_vaincu"

#define QST_ZONE_PACO      11
#define QST_ZONE_KINDRA    13
#define QST_ZONE_EPAVE     19
#define QST_ZONE_TOLEDO   119

#define QST_NPC_NONE              (-1)
#define QST_NPC_PRUNWELL           0
#define QST_NPC_GARDE              1
#define QST_NPC_FINROD             2
#define QST_NPC_MARCHAND_KINDRA    3
#define QST_NPC_ALEXIA_EPAVE       4
#define QST_NPC_BELGAR_EPAVE       5
#define QST_NPC_NATASHA            6
#define QST_NPC_JEEHAN             7
#define QST_NPC_ALEXIA_KINDRA      8  // zone 013, case (9,50)
#define QST_NPC_GUIDE_VALLEE       9  // zone 026, case (25,22) [guide_vallee]
#define QST_NPC_ROSWELL           10  // zone 042, case (20,51)
#define QST_NPC_OME               11  // zone 042, case (40,44)
#define QST_NPC_RUBY              12  // zone 050, case (40,26)
#define QST_NPC_ALBERT            13  // zone 050, case (42,26) (Zeppo : sprite trop large)
#define QST_NPC_ZACH              14  // zone 050, case (16,26)
#define QST_NPC_DIAZ              15  // zone 086, case (40,30)
#define QST_NPC_MILENA            16  // zone 086, case (26,8)
#define QST_NPC_ALEXIA_MANOIR     17  // zone 086, case (25,8)
#define QST_NPC_COUNT             18

/* Coffres portes depuis data/world.txt (centres en pixels, cases 24x16). */
#define QST_CHEST_STRANGE_KEY      0  // zone 019, case (7,16)
#define QST_CHEST_LETTRE           1  // zone 013, case (22,31), garde par l'ogre
#define QST_CHEST_ORDRE            2  // zone 119, case (25,1), ordres de Diaz
#define QST_CHEST_LAMPE            3  // zone 026, case (26,29), apres le golem
#define QST_CHEST_PRISME           4  // zone 042, case (26,31), apres le 2e golem
#define QST_CHEST_CRISTAL          5  // zone 050, case (34,29), apres le capitaine
#define QST_CHEST_COUNT            6

/* Acteurs portes par actor.dat (sprites A1 reels, interaction CROIX). */
#define QST_ACTOR_OGRE              0  // zone 013, case (22,29)
#define QST_ACTOR_CHEST_KEY         1  // zone 019, case (7,16)
#define QST_ACTOR_CHEST_LETTRE      2  // zone 013, case (22,31)
#define QST_ACTOR_MEPHISTO          3  // zone 120, spawn (20,26)
#define QST_ACTOR_ALEXIA_FINALE     4  // zone 120, npc (13,52)
#define QST_ACTOR_ROI               5  // zone 120, npc (13,42)
#define QST_ACTOR_CHEST_ORDRE       6  // zone 119, case (25,1)
#define QST_ACTOR_GOLEM             7  // zone 026, spawn (26,28)
#define QST_ACTOR_CHEST_LAMPE       8  // zone 026, case (26,29)
#define QST_ACTOR_GOLEM_EDEN        9  // zone 042, spawn (25,30)
#define QST_ACTOR_CHEST_PRISME     10  // zone 042, case (26,31)
#define QST_ACTOR_CAPITAINE       11  // zone 050, spawn (34,28)
#define QST_ACTOR_CHEST_CRISTAL   12  // zone 050, case (34,29)
#define QST_ACTOR_COUNT           13

#define QST_DLG_NONE               (-1)
#define QST_DLG_PRUNWELL            0
#define QST_DLG_GARDE               1
#define QST_DLG_FINROD              2
#define QST_DLG_MARCHAND_KINDRA     3
#define QST_DLG_ALEXIA_EPAVE        4
#define QST_DLG_BELGAR_EPAVE        5
#define QST_DLG_NATASHA             6
#define QST_DLG_JEEHAN              7
#define QST_DLG_OBJET_STRANGE_KEY   8
#define QST_DLG_ALEXIA_EPAVE_CLE    9
#define QST_DLG_VICTOIRE           10
#define QST_DLG_OBJET_LETTRE       11
/* L'ordre doit rester identique a DIALOGUE_IDS de ps1_export_dialogue.py. */
#define QST_DLG_MEPHISTO_ARRIVEE   12  // [mephisto_arrivee]
#define QST_DLG_MEPHISTO_TONE      13  // [mephisto_trone]
#define QST_DLG_MEPHISTO_VAINCU    14  // [mephisto_vaincu]
#define QST_DLG_ROI_TONE           15  // [roi_trone]
#define QST_DLG_ROI_VAINCU         16  // [roi_trone_mephisto_vaincu]
#define QST_DLG_ALEXIA_FINALE      17  // [alexia_finale]
#define QST_DLG_ALEXIA_VAINCU      18  // [alexia_finale_mephisto_vaincu]
#define QST_DLG_FINROD_REUNION     19  // [finrod_prunewell], apres [prunewell]
#define QST_DLG_ALEXIA_KINDRA      20  // [alexia_kindra]
#define QST_DLG_OBJET_ORDRE        21  // [objet_ordre]
#define QST_DLG_JEEHAN_ORDRE       22  // [jeehan_objet_ordre]
#define QST_DLG_VALLEE_ARRIVEE     23  // [vallee_arrivee]
#define QST_DLG_GUIDE_VALLEE       24  // [guide_vallee]
#define QST_DLG_BOSS_VALLEE        25  // [boss_026]
#define QST_DLG_OBJET_LAMPE        26  // [objet_lampe]
#define QST_DLG_EDEN_ARRIVEE       27  // [eden_arrivee]
#define QST_DLG_ROSWELL            28  // [roswell]
#define QST_DLG_ROSWELL_LAMPE      29  // [roswell_objet_lampe]
#define QST_DLG_ROSWELL_PRISME     30  // [roswell_objet_prisme]
#define QST_DLG_OME                31  // [ome]
#define QST_DLG_BOSS_EDEN          32  // [boss_042]
#define QST_DLG_OBJET_PRISME       33  // [objet_prisme]
#define QST_DLG_MEDUSA_ARRIVEE     34  // [medusa_arrivee]
#define QST_DLG_RUBY               35  // [ruby_puerto]
#define QST_DLG_ALBERT             36  // [albert_puerto]
#define QST_DLG_ZACH               37  // [zach_fort]
#define QST_DLG_BOSS_MEDUSA        38  // [boss_medusa]
#define QST_DLG_OBJET_CRISTAL      39  // [objet_cristal]
#define QST_DLG_MANOIR_ARRIVEE     40  // [manoir_arrivee]
#define QST_DLG_DIAZ               41  // [diaz_manoir]
#define QST_DLG_DIAZ_CRISTAL       42  // [diaz_manoir_objet_cristal]
#define QST_DLG_MILENA             43  // [milena_boss_medusa] (gate deja pose a l'entree)
#define QST_DLG_ALEXIA_MANOIR      44  // [alexia_manoir]

#define QST_FWD_X   14
#define QST_FWD_Y   10
#define QST_RADIUS2 (20 * 20)

struct QuestState {
	int stage;
	int activeNpc;
	int dialogueId;
	int talkedPrunewell;
	int talkedGarde;
	int talkedFinrod;
	int talkedMarchand;
	int talkedAlexiaEpave;
	int talkedBelgarEpave;
	int talkedNatasha;
	int talkedJeehan;
	int hasStrangeKey;
	int bossDown;
	int chestLettre;
	int zone;
	uint32_t prevPad;
	uint32_t gates;  // QST_GATE_* / QST_FLAG_* (epee : Jeehan ; autres gates : harnais)
	/* Champs de la salle du trone : APRES gates, offsets 68..91. */
	int mephistoDown;
	int ending;
	int talkedMephistoArrivee;
	int talkedMephistoTrone;
	int talkedRoi;
	int talkedAlexiaFinale;
	int talkedAlexiaVaincu;
};

inline void questInit(QuestState* s) {
	s->stage = QST_STAGE_ARRIVEE;
	s->activeNpc = QST_NPC_NONE;
	s->dialogueId = QST_DLG_NONE;
	s->talkedPrunewell = 0;
	s->talkedGarde = 0;
	s->talkedFinrod = 0;
	s->talkedMarchand = 0;
	s->talkedAlexiaEpave = 0;
	s->talkedBelgarEpave = 0;
	s->talkedNatasha = 0;
	s->talkedJeehan = 0;
	s->hasStrangeKey = 0;
	s->bossDown = 0;
	s->chestLettre = 0;
	s->zone = QST_ZONE_PACO;
	s->prevPad = 0;
	s->gates = 0;
	s->mephistoDown = 0;
	s->ending = 0;
	s->talkedMephistoArrivee = 0;
	s->talkedMephistoTrone = 0;
	s->talkedRoi = 0;
	s->talkedAlexiaFinale = 0;
	s->talkedAlexiaVaincu = 0;
}

/* Centres pixels des PNJ, en cases de 24x16 px (coordonnees data/world.txt). */
inline void questNpcPos(int npc, int* x, int* y) {
	switch (npc) {
	case QST_NPC_PRUNWELL:
		if (x) *x = 31 * 24 + 12;  // 756
		if (y) *y = 47 * 16 + 8;   // 760
		break;
	case QST_NPC_GARDE:
		if (x) *x = 34 * 24 + 12;  // 828
		if (y) *y = 49 * 16 + 8;   // 792
		break;
	case QST_NPC_FINROD:
		if (x) *x = 41 * 24 + 12;  // 996, zone 013
		if (y) *y = 55 * 16 + 8;   // 888
		break;
	case QST_NPC_MARCHAND_KINDRA:
		if (x) *x = 10 * 24 + 12;  // 252, zone 013
		if (y) *y = 57 * 16 + 8;   // 920
		break;
	case QST_NPC_ALEXIA_EPAVE:
		if (x) *x = 10 * 24 + 12;  // 252, zone 019
		if (y) *y = 11 * 16 + 8;   // 184
		break;
	case QST_NPC_BELGAR_EPAVE:
		if (x) *x = 12 * 24 + 12;  // 300, zone 019
		if (y) *y = 12 * 16 + 8;   // 200
		break;
	case QST_NPC_NATASHA:
		if (x) *x = 10 * 24 + 12;  // 252, zone 119
		if (y) *y = 34 * 16 + 8;   // 552
		break;
	case QST_NPC_JEEHAN:
		if (x) *x = 23 * 24 + 12;  // 564, zone 119
		if (y) *y = 12 * 16 + 8;   // 200
		break;
	case QST_NPC_DIAZ:
		if (x) *x = 40 * 24 + 12;
		if (y) *y = 30 * 16 + 8;
		break;
	case QST_NPC_MILENA:
		if (x) *x = 26 * 24 + 12;
		if (y) *y = 8 * 16 + 8;
		break;
	case QST_NPC_ALEXIA_MANOIR:
		if (x) *x = 25 * 24 + 12;
		if (y) *y = 8 * 16 + 8;
		break;
	case QST_NPC_RUBY:
		if (x) *x = 40 * 24 + 12;
		if (y) *y = 26 * 16 + 8;
		break;
	case QST_NPC_ALBERT:
		if (x) *x = 42 * 24 + 12;
		if (y) *y = 26 * 16 + 8;
		break;
	case QST_NPC_ZACH:
		if (x) *x = 16 * 24 + 12;
		if (y) *y = 26 * 16 + 8;
		break;
	case QST_NPC_ROSWELL:
		if (x) *x = 20 * 24 + 12;  // 492, zone 042
		if (y) *y = 51 * 16 + 8;   // 824
		break;
	case QST_NPC_OME:
		if (x) *x = 40 * 24 + 12;  // 972, zone 042
		if (y) *y = 44 * 16 + 8;   // 712
		break;
	case QST_NPC_GUIDE_VALLEE:
		if (x) *x = 25 * 24 + 12;  // 612, zone 026
		if (y) *y = 22 * 16 + 8;   // 360
		break;
	case QST_NPC_ALEXIA_KINDRA:
		if (x) *x = 9 * 24 + 12;   // 228, zone 013
		if (y) *y = 50 * 16 + 8;   // 808
		break;
	default:
		if (x) *x = 0;
		if (y) *y = 0;
		break;
	}
}

inline void questSetZone(QuestState* s, int zone) { s->zone = zone; }

inline int questJeehanAwaitsOrdre(const QuestState* s) {
	return (s->gates & QST_FLAG_ORDRE) != 0 &&
	       (s->gates & QST_GATE_EPEE_SUPREME) == 0 ? 1 : 0;
}

/* Narration d'arrivee (data/world.txt : event 026 arrivee_vallee). Une seule
 * fois : le drapeau est pose a la fin du dialogue, donc sauvegarde. */
inline int questZoneArrival(QuestState* s) {
	if (s->dialogueId != QST_DLG_NONE) return QST_DLG_NONE;
	if (s->zone == QST_ZONE_VALLEE && (s->gates & QST_FLAG_VALLEE_VUE) == 0) {
		s->dialogueId = QST_DLG_VALLEE_ARRIVEE;
		return s->dialogueId;
	}
	if (s->zone == QST_ZONE_MANOIR && (s->gates & QST_FLAG_MANOIR_VU) == 0) {
		s->dialogueId = QST_DLG_MANOIR_ARRIVEE;
		return s->dialogueId;
	}
	if (s->zone == QST_ZONE_MEDUSA && (s->gates & QST_FLAG_MEDUSA_VU) == 0) {
		s->dialogueId = QST_DLG_MEDUSA_ARRIVEE;
		return s->dialogueId;
	}
	if (s->zone == QST_ZONE_EDEN && (s->gates & QST_FLAG_EDEN_VU) == 0) {
		s->dialogueId = QST_DLG_EDEN_ARRIVEE;
		return s->dialogueId;
	}
	return QST_DLG_NONE;
}

/* Un seul zone active a la fois : les PNJ des autres zones sont invisibles.
 * Le garde de Paco reste conditionne par l'avertissement de Prunewell. */
inline int questNpcAvailable(const QuestState* s, int npc) {
	switch (s->zone) {
	case QST_ZONE_PACO:
		if (npc == QST_NPC_PRUNWELL) return 1;
		return npc == QST_NPC_GARDE && s->stage >= QST_STAGE_MONT_EPERVIER ? 1 : 0;
	case QST_ZONE_KINDRA:
		return npc == QST_NPC_FINROD || npc == QST_NPC_MARCHAND_KINDRA ||
		       npc == QST_NPC_ALEXIA_KINDRA ? 1 : 0;
	case QST_ZONE_VALLEE:
		return npc == QST_NPC_GUIDE_VALLEE ? 1 : 0;
	case QST_ZONE_EDEN:
		return npc == QST_NPC_ROSWELL || npc == QST_NPC_OME ? 1 : 0;
	case QST_ZONE_MANOIR:
		return npc == QST_NPC_DIAZ || npc == QST_NPC_MILENA ||
		       npc == QST_NPC_ALEXIA_MANOIR ? 1 : 0;
	case QST_ZONE_MEDUSA:
		return npc == QST_NPC_RUBY || npc == QST_NPC_ALBERT || npc == QST_NPC_ZACH ? 1 : 0;
	case QST_ZONE_EPAVE:
		return npc == QST_NPC_ALEXIA_EPAVE || npc == QST_NPC_BELGAR_EPAVE ? 1 : 0;
	case QST_ZONE_TOLEDO:
		return npc == QST_NPC_NATASHA || npc == QST_NPC_JEEHAN ? 1 : 0;
	default:
		return 0;
	}
}

inline int questNpcBubble(const QuestState* s, int npc) {
	if (!questNpcAvailable(s, npc)) return 0;
	switch (npc) {
	case QST_NPC_PRUNWELL: return s->talkedPrunewell == 0 ? 1 : 0;
	case QST_NPC_FINROD: return s->talkedFinrod == 0 ? 1 : 0;
	case QST_NPC_MARCHAND_KINDRA: return s->talkedMarchand == 0 ? 1 : 0;
	case QST_NPC_ALEXIA_EPAVE: return s->talkedAlexiaEpave == 0 ? 1 : 0;
	case QST_NPC_BELGAR_EPAVE: return s->talkedBelgarEpave == 0 ? 1 : 0;
	case QST_NPC_NATASHA: return s->talkedNatasha == 0 ? 1 : 0;
	case QST_NPC_GUIDE_VALLEE: return (s->gates & QST_FLAG_GUIDE_VU) == 0 ? 1 : 0;
	case QST_NPC_OME: return (s->gates & QST_FLAG_OME_VU) == 0 ? 1 : 0;
	case QST_NPC_RUBY: return (s->gates & QST_FLAG_RUBY_VU) == 0 ? 1 : 0;
	case QST_NPC_MILENA: return (s->gates & QST_FLAG_MILENA_VU) == 0 ? 1 : 0;
	case QST_NPC_ALEXIA_MANOIR: return (s->gates & QST_FLAG_ALEXIA_MANOIR_VU) == 0 ? 1 : 0;
	case QST_NPC_DIAZ:
		return (s->gates & ((s->gates & QST_GATE_CRISTAL) ? QST_FLAG_DIAZ_CRISTAL_VU
		                                                  : QST_FLAG_DIAZ_VU)) == 0 ? 1 : 0;
	case QST_NPC_ALBERT: return (s->gates & QST_FLAG_ALBERT_VU) == 0 ? 1 : 0;
	case QST_NPC_ZACH: return (s->gates & QST_FLAG_ZACH_VU) == 0 ? 1 : 0;
	case QST_NPC_ROSWELL:
		// Un nouveau variant (Prisme) est une nouvelle information : bulle.
		return (s->gates & ((s->gates & QST_GATE_PRISME) ? QST_FLAG_ROSWELL_PRISME_VU
		                                                  : QST_FLAG_ROSWELL_VU)) == 0 ? 1 : 0;
	case QST_NPC_JEEHAN:
		// Jeehan revient vers Flint quand il apporte les ordres (epee pas encore remise).
		return s->talkedJeehan == 0 || questJeehanAwaitsOrdre(s) ? 1 : 0;
	default: return s->talkedGarde == 0 ? 1 : 0;
	}
}

inline int questNpcNear(const QuestState* s, int hx, int hy, int dir) {
	int fx = hx;
	int fy = hy;
	if (dir == PF_DIR_RIGHT) fx += QST_FWD_X;
	else if (dir == PF_DIR_LEFT) fx -= QST_FWD_X;
	else if (dir == PF_DIR_UP) fy -= QST_FWD_Y;
	else if (dir == PF_DIR_DOWN) fy += QST_FWD_Y;
	for (int npc = 0; npc < QST_NPC_COUNT; ++npc) {
		if (!questNpcAvailable(s, npc)) continue;
		int nx = 0, ny = 0;
		questNpcPos(npc, &nx, &ny);
		const int dx = nx - fx;
		const int dy = ny - fy;
		if (dx * dx + dy * dy < QST_RADIUS2) return npc;
	}
	return QST_NPC_NONE;
}

inline int questNpcDialogue(const QuestState* s, int npc) {
	// data/dialogues.txt [alexia_epave] : `si objet_strange_key alexia_epave_cle`.
	if (npc == QST_NPC_ALEXIA_EPAVE && s->hasStrangeKey)
		return QST_DLG_ALEXIA_EPAVE_CLE;
	switch (npc) {
	case QST_NPC_GARDE: return QST_DLG_GARDE;
	case QST_NPC_FINROD:
		// Retrouvailles atteste es A2 (bloc 32) : Finrod reagit a la visite de
		// Prunewell a Paco. Le flag deja sauvegarde suffit, aucun nouveau POD.
		return s->talkedPrunewell ? QST_DLG_FINROD_REUNION : QST_DLG_FINROD;
	case QST_NPC_MARCHAND_KINDRA: return QST_DLG_MARCHAND_KINDRA;
	case QST_NPC_ALEXIA_EPAVE: return QST_DLG_ALEXIA_EPAVE;
	case QST_NPC_BELGAR_EPAVE: return QST_DLG_BELGAR_EPAVE;
	case QST_NPC_NATASHA: return QST_DLG_NATASHA;
	case QST_NPC_JEEHAN:
		return questJeehanAwaitsOrdre(s) ? QST_DLG_JEEHAN_ORDRE : QST_DLG_JEEHAN;
	case QST_NPC_ALEXIA_KINDRA: return QST_DLG_ALEXIA_KINDRA;
	case QST_NPC_GUIDE_VALLEE: return QST_DLG_GUIDE_VALLEE;
	case QST_NPC_OME: return QST_DLG_OME;
	case QST_NPC_RUBY: return QST_DLG_RUBY;
	case QST_NPC_DIAZ:
		return (s->gates & QST_GATE_CRISTAL) ? QST_DLG_DIAZ_CRISTAL : QST_DLG_DIAZ;
	case QST_NPC_MILENA: return QST_DLG_MILENA;
	case QST_NPC_ALEXIA_MANOIR: return QST_DLG_ALEXIA_MANOIR;
	case QST_NPC_ALBERT: return QST_DLG_ALBERT;
	case QST_NPC_ZACH: return QST_DLG_ZACH;
	case QST_NPC_ROSWELL:
		// main.cpp tryInteract : le dernier variant d'evenement disponible gagne.
		if (s->gates & QST_GATE_PRISME) return QST_DLG_ROSWELL_PRISME;
		if (s->gates & QST_GATE_LAMPE) return QST_DLG_ROSWELL_LAMPE;
		return QST_DLG_ROSWELL;
	default: return QST_DLG_PRUNWELL;
	}
}

/* --- v0.6.0 : objets, coffres et mini-boss -------------------------------------
 * La logique reste POD : aucun ennemi vivant cote PS1 pour l'instant, seule la
 * consequence de la victoire est porte (bossDown, coffre libere, dialogue).
 * Les gates A→Z ne sont toujours pas representes (gates reste a 0).
 */
inline int questChestAvailable(const QuestState* s, int chest) {
	switch (chest) {
	case QST_CHEST_STRANGE_KEY:
		return s->zone == QST_ZONE_EPAVE && s->hasStrangeKey == 0 ? 1 : 0;
	case QST_CHEST_LETTRE:
		return s->zone == QST_ZONE_KINDRA && s->bossDown == 1 &&
		       s->chestLettre == 0 ? 1 : 0;
	case QST_CHEST_CRISTAL:
		return s->zone == QST_ZONE_MEDUSA && (s->gates & QST_GATE_BOSS_MEDUSA) != 0 &&
		       (s->gates & QST_GATE_CRISTAL) == 0 ? 1 : 0;
	case QST_CHEST_PRISME:
		return s->zone == QST_ZONE_EDEN && (s->gates & QST_FLAG_GOLEM_EDEN) != 0 &&
		       (s->gates & QST_GATE_PRISME) == 0 ? 1 : 0;
	case QST_CHEST_LAMPE:
		return s->zone == QST_ZONE_VALLEE && (s->gates & QST_FLAG_GOLEM) != 0 &&
		       (s->gates & QST_GATE_LAMPE) == 0 ? 1 : 0;
	case QST_CHEST_ORDRE:
		return s->zone == QST_ZONE_TOLEDO && (s->gates & QST_FLAG_ORDRE) == 0 ? 1 : 0;
	default:
		return 0;
	}
}

inline int questChestIdAt(const QuestState* s, int hx, int hy) {
	for (int chest = 0; chest < QST_CHEST_COUNT; ++chest) {
		if (!questChestAvailable(s, chest)) continue;
		const int cx = chest == QST_CHEST_CRISTAL ? 34 :
		               chest == QST_CHEST_STRANGE_KEY ? 7 : chest == QST_CHEST_ORDRE ? 25 :
		               chest == QST_CHEST_LAMPE || chest == QST_CHEST_PRISME ? 26 : 22;
		const int cy = chest == QST_CHEST_CRISTAL ? 29 :
		               chest == QST_CHEST_STRANGE_KEY ? 16 : chest == QST_CHEST_ORDRE ? 1 :
		               chest == QST_CHEST_LAMPE ? 29 : 31;  // prisme : (26,31)
		const int dx = hx - (cx * 24 + 12);
		const int dy = hy - (cy * 16 + 8);
		if (dx * dx + dy * dy < QST_RADIUS2) return chest;
	}
	return -1;
}

/* Ouvre un coffre disponible et retourne le dialogue a afficher, ou NONE. */
inline int questChestTake(QuestState* s, int chest) {
	if (!questChestAvailable(s, chest) || s->dialogueId != QST_DLG_NONE)
		return QST_DLG_NONE;
	switch (chest) {
	case QST_CHEST_STRANGE_KEY:
		s->hasStrangeKey = 1;
		s->dialogueId = QST_DLG_OBJET_STRANGE_KEY;
		if (s->stage < QST_STAGE_EPAVE) s->stage = QST_STAGE_EPAVE;
		return s->dialogueId;
	case QST_CHEST_LETTRE:
		s->chestLettre = 1;
		s->dialogueId = QST_DLG_OBJET_LETTRE;
		return s->dialogueId;
	case QST_CHEST_CRISTAL:
		s->gates |= QST_GATE_CRISTAL;  // derniere relique, gate du trone
		s->dialogueId = QST_DLG_OBJET_CRISTAL;
		return s->dialogueId;
	case QST_CHEST_PRISME:
		s->gates |= QST_GATE_PRISME;  // gate de la route 042 -> 050
		s->dialogueId = QST_DLG_OBJET_PRISME;
		return s->dialogueId;
	case QST_CHEST_LAMPE:
		s->gates |= QST_GATE_LAMPE;  // gate de la route 026 -> 042
		s->dialogueId = QST_DLG_OBJET_LAMPE;
		return s->dialogueId;
	case QST_CHEST_ORDRE:
		s->gates |= QST_FLAG_ORDRE;
		s->dialogueId = QST_DLG_OBJET_ORDRE;
		return s->dialogueId;
	default:
		return QST_DLG_NONE;
	}
}

/* Consequence de la victoire sur l'ogre de la foret Kindra (src/entities.cpp). */
inline void questOgreDefeated(QuestState* s) {
	if (s->bossDown) return;
	s->bossDown = 1;
	if (s->stage < QST_STAGE_BOSS_VAINCU) s->stage = QST_STAGE_BOSS_VAINCU;
	s->dialogueId = QST_DLG_VICTOIRE;
}

/* Consequence de la victoire reelle sur le golem de la vallee (boss_026).
 * Retourne 1 la premiere fois seulement. */
inline int questGolemDefeated(QuestState* s) {
	if (s->zone != QST_ZONE_VALLEE || (s->gates & QST_FLAG_GOLEM) != 0) return 0;
	s->gates |= QST_FLAG_GOLEM;
	s->dialogueId = QST_DLG_BOSS_VALLEE;
	return 1;
}

/* Second golem, Ruines d'Eden (boss_042). Retourne 1 la premiere fois. */
inline int questGolemEdenDefeated(QuestState* s) {
	if (s->zone != QST_ZONE_EDEN || (s->gates & QST_FLAG_GOLEM_EDEN) != 0) return 0;
	s->gates |= QST_FLAG_GOLEM_EDEN;
	s->dialogueId = QST_DLG_BOSS_EDEN;
	return 1;
}

/* Capitaine des pirates de Puerto Medusa : sa defaite pose le gate
 * boss_medusa (entities.cpp). Retourne 1 la premiere fois. */
inline int questCapitaineDefeated(QuestState* s) {
	if (s->zone != QST_ZONE_MEDUSA || (s->gates & QST_GATE_BOSS_MEDUSA) != 0) return 0;
	s->gates |= QST_GATE_BOSS_MEDUSA;
	s->dialogueId = QST_DLG_BOSS_MEDUSA;
	return 1;
}

/* --- v0.6.0b : salle du trone ---------------------------------------------------
 * Le passage 086→120 est conditionne par worldTryWarp120(). Ces helpers ne font
 * que consommer l'etat deja autorise : aucun gate n'est modifie ici.
 */
inline int questThroneEnter(QuestState* s) {
	if (!s || s->zone != QST_ZONE_TRONE || s->ending ||
	    !worldTroneGatesComplete(s->gates)) return 0;
	if (s->talkedMephistoArrivee || s->dialogueId != QST_DLG_NONE) return 0;
	s->dialogueId = QST_DLG_MEPHISTO_ARRIVEE;
	if (s->stage < QST_STAGE_TRONE) s->stage = QST_STAGE_TRONE;
	return 1;
}

inline int questMephistoDefeated(QuestState* s) {
	if (!s || s->zone != QST_ZONE_TRONE || s->mephistoDown ||
	    !worldTroneGatesComplete(s->gates)) return 0;
	s->mephistoDown = 1;
	s->stage = QST_STAGE_TRONE_VAINCU;
	s->dialogueId = QST_DLG_MEPHISTO_VAINCU;
	return 1;
}

/* La vraie fin n'est atteinte qu'apres la victoire ET la derniere replique
 * d'Alexia. Un simple retour en zone 120 ne suffit jamais. */
/* Le combat final n'est arme qu'apres la replique de Mephisto sur son trone. */
inline int questMephistoCombatReady(const QuestState* s) {
	return s && s->zone == QST_ZONE_TRONE && s->talkedMephistoTrone &&
	       !s->mephistoDown && worldTroneGatesComplete(s->gates) ? 1 : 0;
}

inline int questEnding(QuestState* s) {
	if (!s || s->zone != QST_ZONE_TRONE || !s->mephistoDown ||
	    !s->talkedAlexiaVaincu || s->ending) return 0;
	s->ending = 1;
	s->stage = QST_STAGE_ENDING;
	s->dialogueId = QST_DLG_NONE;
	return 1;
}

inline void questActorPos(int actor, int* x, int* y) {
	switch (actor) {
	case QST_ACTOR_OGRE:
		if (x) *x = 22 * 24 + 12;  // 540, zone 013
		if (y) *y = 29 * 16 + 8;   // 472
		break;
	case QST_ACTOR_CHEST_KEY:
		if (x) *x = 7 * 24 + 12;   // 180, zone 019
		if (y) *y = 16 * 16 + 8;   // 264
		break;
	case QST_ACTOR_CHEST_LETTRE:
		if (x) *x = 22 * 24 + 12;  // 540, zone 013
		if (y) *y = 31 * 16 + 8;   // 504
		break;
	case QST_ACTOR_MEPHISTO:
		if (x) *x = 20 * 24 + 12;  // 492, zone 120 (data/world.txt spawn)
		if (y) *y = 26 * 16 + 8;   // 424
		break;
	case QST_ACTOR_ALEXIA_FINALE:
		if (x) *x = 13 * 24 + 12;  // 324, zone 120
		if (y) *y = 52 * 16 + 8;   // 840
		break;
	case QST_ACTOR_ROI:
		if (x) *x = 13 * 24 + 12;  // 324, zone 120
		if (y) *y = 42 * 16 + 8;   // 680
		break;
	case QST_ACTOR_CAPITAINE:
		if (x) *x = 34 * 24 + 12;
		if (y) *y = 28 * 16 + 8;
		break;
	case QST_ACTOR_CHEST_CRISTAL:
		if (x) *x = 34 * 24 + 12;
		if (y) *y = 29 * 16 + 8;
		break;
	case QST_ACTOR_GOLEM_EDEN:
		if (x) *x = 25 * 24 + 12;  // 612, zone 042
		if (y) *y = 30 * 16 + 8;   // 488
		break;
	case QST_ACTOR_CHEST_PRISME:
		if (x) *x = 26 * 24 + 12;  // 636, zone 042
		if (y) *y = 31 * 16 + 8;   // 504
		break;
	case QST_ACTOR_GOLEM:
		if (x) *x = 26 * 24 + 12;  // 636, zone 026
		if (y) *y = 28 * 16 + 8;   // 456
		break;
	case QST_ACTOR_CHEST_LAMPE:
		if (x) *x = 26 * 24 + 12;  // 636, zone 026
		if (y) *y = 29 * 16 + 8;   // 472
		break;
	case QST_ACTOR_CHEST_ORDRE:
		if (x) *x = 25 * 24 + 12;  // 612, zone 119
		if (y) *y = 1 * 16 + 8;    // 24
		break;
	default:
		if (x) *x = 0;
		if (y) *y = 0;
		break;
	}
}

inline int questActorAvailable(const QuestState* s, int actor) {
	switch (actor) {
	case QST_ACTOR_OGRE:
		return s->zone == QST_ZONE_KINDRA && s->bossDown == 0 ? 1 : 0;
	case QST_ACTOR_CHEST_KEY:
		return s->zone == QST_ZONE_EPAVE && s->hasStrangeKey == 0 ? 1 : 0;
	case QST_ACTOR_CHEST_LETTRE:
		return questChestAvailable(s, QST_CHEST_LETTRE);
	case QST_ACTOR_CHEST_ORDRE:
		return questChestAvailable(s, QST_CHEST_ORDRE);
	case QST_ACTOR_GOLEM:
		return s->zone == QST_ZONE_VALLEE && (s->gates & QST_FLAG_GOLEM) == 0 ? 1 : 0;
	case QST_ACTOR_CHEST_LAMPE:
		return questChestAvailable(s, QST_CHEST_LAMPE);
	case QST_ACTOR_GOLEM_EDEN:
		return s->zone == QST_ZONE_EDEN && (s->gates & QST_FLAG_GOLEM_EDEN) == 0 ? 1 : 0;
	case QST_ACTOR_CHEST_PRISME:
		return questChestAvailable(s, QST_CHEST_PRISME);
	case QST_ACTOR_CAPITAINE:
		return s->zone == QST_ZONE_MEDUSA && (s->gates & QST_GATE_BOSS_MEDUSA) == 0 ? 1 : 0;
	case QST_ACTOR_CHEST_CRISTAL:
		return questChestAvailable(s, QST_CHEST_CRISTAL);
	case QST_ACTOR_MEPHISTO:
		return s->zone == QST_ZONE_TRONE && s->talkedMephistoArrivee == 1 &&
		       s->mephistoDown == 0 ? 1 : 0;
	case QST_ACTOR_ALEXIA_FINALE:
	case QST_ACTOR_ROI:
		return s->zone == QST_ZONE_TRONE && s->ending == 0 ? 1 : 0;
	default:
		return 0;
	}
}

inline int questActorNear(const QuestState* s, int hx, int hy, int dir) {
	int fx = hx;
	int fy = hy;
	if (dir == PF_DIR_RIGHT) fx += QST_FWD_X;
	else if (dir == PF_DIR_LEFT) fx -= QST_FWD_X;
	else if (dir == PF_DIR_UP) fy -= QST_FWD_Y;
	else if (dir == PF_DIR_DOWN) fy += QST_FWD_Y;
	for (int actor = 0; actor < QST_ACTOR_COUNT; ++actor) {
		if (!questActorAvailable(s, actor)) continue;
		int ax = 0, ay = 0;
		questActorPos(actor, &ax, &ay);
		const int dx = ax - fx;
		const int dy = ay - fy;
		if (dx * dx + dy * dy < QST_RADIUS2) return actor;
	}
	return QST_NPC_NONE;
}

/* Interaction CROIX sur un acteur. Le resultat est le dialogue a ouvrir. */
inline int questActorInteract(QuestState* s, int actor) {
	if (!questActorAvailable(s, actor) || s->dialogueId != QST_DLG_NONE)
		return QST_DLG_NONE;
	switch (actor) {
	case QST_ACTOR_OGRE:
		// L'ogre se combat (CARRE, ogre_combat_ps1_logic.h) : CROIX ne conclut
		// jamais le duel. Seul ogreDuelJustWon() appelle questOgreDefeated().
		return QST_DLG_NONE;
	case QST_ACTOR_CHEST_KEY:
		return questChestTake(s, QST_CHEST_STRANGE_KEY);
	case QST_ACTOR_CHEST_LETTRE:
		return questChestTake(s, QST_CHEST_LETTRE);
	case QST_ACTOR_CHEST_ORDRE:
		return questChestTake(s, QST_CHEST_ORDRE);
	case QST_ACTOR_GOLEM:
		return QST_DLG_NONE;  // se combat (CARRE) : CROIX ne conclut jamais
	case QST_ACTOR_CHEST_LAMPE:
		return questChestTake(s, QST_CHEST_LAMPE);
	case QST_ACTOR_GOLEM_EDEN:
		return QST_DLG_NONE;  // se combat (CARRE)
	case QST_ACTOR_CHEST_PRISME:
		return questChestTake(s, QST_CHEST_PRISME);
	case QST_ACTOR_CAPITAINE:
		return QST_DLG_NONE;  // se combat (CARRE)
	case QST_ACTOR_CHEST_CRISTAL:
		return questChestTake(s, QST_CHEST_CRISTAL);
	case QST_ACTOR_MEPHISTO:
		if (!s->talkedMephistoTrone) {
			s->dialogueId = QST_DLG_MEPHISTO_TONE;
			return s->dialogueId;
		}
		// Ensuite il se combat (entities.cpp, 36 PV) : CROIX ne conclut jamais.
		return QST_DLG_NONE;
	case QST_ACTOR_ALEXIA_FINALE:
		s->dialogueId = s->mephistoDown ? QST_DLG_ALEXIA_VAINCU
		                                : QST_DLG_ALEXIA_FINALE;
		return s->dialogueId;
	case QST_ACTOR_ROI:
		s->dialogueId = s->mephistoDown ? QST_DLG_ROI_VAINCU : QST_DLG_ROI_TONE;
		return s->dialogueId;
	default:
		return QST_DLG_NONE;
	}
}

inline int questHasInteract(const QuestState* s, int hx, int hy, int dir) {
	return questNpcNear(s, hx, hy, dir) != QST_NPC_NONE ||
	       questActorNear(s, hx, hy, dir) != QST_NPC_NONE ? 1 : 0;
}

inline int questObjectiveVisible(const QuestState* s) {
	return s->stage >= QST_STAGE_MONT_EPERVIER ? 1 : 0;
}

/* A appeler chaque frame avec le masque de manette deja filtre par `armed`. */
inline void questStep(QuestState* s, uint32_t pad, int hx, int hy, int dir) {
	const uint32_t pressed = pad & ~s->prevPad;
	s->prevPad = pad;
	if (s->dialogueId != QST_DLG_NONE) return;  // le dialogue gere ses propres touches
	if (!(pressed & PF_BTN_CROSS)) return;
	const int actor = questActorNear(s, hx, hy, dir);
	if (actor != QST_NPC_NONE) {
		questActorInteract(s, actor);
		s->activeNpc = QST_NPC_NONE;
		return;
	}
	const int npc = questNpcNear(s, hx, hy, dir);
	if (npc == QST_NPC_NONE) return;
	s->activeNpc = npc;
	s->dialogueId = questNpcDialogue(s, npc);
}

inline void questDialogueDoneAfter(QuestState* s, int dialogueId) {
	switch (dialogueId) {
	case QST_DLG_PRUNWELL:
		s->talkedPrunewell = 1;
		if (s->stage < QST_STAGE_MONT_EPERVIER) s->stage = QST_STAGE_MONT_EPERVIER;
		break;
	case QST_DLG_GARDE:
		s->talkedGarde = 1;
		if (s->stage < QST_STAGE_PACO_PREVENU) s->stage = QST_STAGE_PACO_PREVENU;
		break;
	case QST_DLG_FINROD:
	case QST_DLG_FINROD_REUNION:
		s->talkedFinrod = 1;
		if (s->stage < QST_STAGE_KINDRA) s->stage = QST_STAGE_KINDRA;
		break;
	case QST_DLG_MARCHAND_KINDRA:
		s->talkedMarchand = 1;
		break;
	case QST_DLG_ALEXIA_EPAVE:
		s->talkedAlexiaEpave = 1;
		if (s->stage < QST_STAGE_EPAVE) s->stage = QST_STAGE_EPAVE;
		break;
	case QST_DLG_BELGAR_EPAVE:
		s->talkedBelgarEpave = 1;
		if (s->stage < QST_STAGE_EPAVE) s->stage = QST_STAGE_EPAVE;
		break;
	case QST_DLG_NATASHA:
		s->talkedNatasha = 1;
		if (s->stage < QST_STAGE_TOLEDO) s->stage = QST_STAGE_TOLEDO;
		break;
	case QST_DLG_JEEHAN:
		s->talkedJeehan = 1;
		if (s->stage < QST_STAGE_TOLEDO) s->stage = QST_STAGE_TOLEDO;
		break;
	case QST_DLG_ALEXIA_EPAVE_CLE:
		s->talkedAlexiaEpave = 1;
		break;
	case QST_DLG_JEEHAN_ORDRE:
		// items.cpp grantSupreme : l'epee n'est remise qu'avec les ordres.
		s->talkedJeehan = 1;
		if (s->gates & QST_FLAG_ORDRE) s->gates |= QST_GATE_EPEE_SUPREME;
		break;
	case QST_DLG_VALLEE_ARRIVEE:
		s->gates |= QST_FLAG_VALLEE_VUE;
		break;
	case QST_DLG_GUIDE_VALLEE:
		s->gates |= QST_FLAG_GUIDE_VU;
		break;
	case QST_DLG_EDEN_ARRIVEE:
		s->gates |= QST_FLAG_EDEN_VU;
		break;
	case QST_DLG_ROSWELL:
	case QST_DLG_ROSWELL_LAMPE:
		s->gates |= QST_FLAG_ROSWELL_VU;
		break;
	case QST_DLG_ROSWELL_PRISME:
		s->gates |= QST_FLAG_ROSWELL_VU | QST_FLAG_ROSWELL_PRISME_VU;
		break;
	case QST_DLG_OME:
		s->gates |= QST_FLAG_OME_VU;
		break;
	case QST_DLG_MEDUSA_ARRIVEE:
		s->gates |= QST_FLAG_MEDUSA_VU;
		break;
	case QST_DLG_RUBY:
		s->gates |= QST_FLAG_RUBY_VU;
		break;
	case QST_DLG_MANOIR_ARRIVEE:
		s->gates |= QST_FLAG_MANOIR_VU;
		break;
	case QST_DLG_DIAZ:
		s->gates |= QST_FLAG_DIAZ_VU;
		break;
	case QST_DLG_DIAZ_CRISTAL:
		s->gates |= QST_FLAG_DIAZ_VU | QST_FLAG_DIAZ_CRISTAL_VU;
		break;
	case QST_DLG_MILENA:
		s->gates |= QST_FLAG_MILENA_VU;
		break;
	case QST_DLG_ALEXIA_MANOIR:
		s->gates |= QST_FLAG_ALEXIA_MANOIR_VU;
		break;
	case QST_DLG_ALBERT:
		s->gates |= QST_FLAG_ALBERT_VU;
		break;
	case QST_DLG_ZACH:
		s->gates |= QST_FLAG_ZACH_VU;
		break;
	case QST_DLG_MEPHISTO_ARRIVEE:
		s->talkedMephistoArrivee = 1;
		break;
	case QST_DLG_MEPHISTO_TONE:
		s->talkedMephistoTrone = 1;
		break;
	case QST_DLG_ROI_TONE:
		s->talkedRoi = 1;
		break;
	case QST_DLG_ALEXIA_FINALE:
		s->talkedAlexiaFinale = 1;
		break;
	case QST_DLG_ALEXIA_VAINCU:
		s->talkedAlexiaVaincu = 1;
		questEnding(s);
		break;
	case QST_DLG_OBJET_STRANGE_KEY:
		if (s->stage < QST_STAGE_EPAVE) s->stage = QST_STAGE_EPAVE;
		break;
	case QST_DLG_VICTOIRE:
		if (s->stage < QST_STAGE_BOSS_VAINCU) s->stage = QST_STAGE_BOSS_VAINCU;
		break;
	default:
		break;
	}
}

/* Appele par la boucle quand le dernier panneau du dialogue est ferme. */
inline void questDialogueDone(QuestState* s) {
	if (s->dialogueId == QST_DLG_NONE) return;
	questDialogueDoneAfter(s, s->dialogueId);
	s->dialogueId = QST_DLG_NONE;
	s->activeNpc = QST_NPC_NONE;
}
