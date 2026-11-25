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

#include "audio_ps1.h"
#include "diag_ps1.h"
#include "dialogue_ps1.h"
#include "hero_ps1.h"
#include "hero_vitals_ps1_logic.h"
#include "hud_ps1.h"
#include "interaction_ps1.h"
#include "intro_ps1.h"
#include "menu_inventory_ps1.h"
#include "menu_ps1.h"
#include "menu_ps1_logic.h"
#include "quest_ps1_logic.h"
#include "save_ps1.h"
#include "spu_diag_ps1.h"
#include "world_ps1_logic.h"
#include "zone011.h"
#include "zone_heap_ps1.h"
#include "vram_diag_ps1.h"

#ifdef PS1_DIAG

namespace {
const uint32_t kBg = 0x1021u;
const uint32_t kSquare = 0x001Fu;
const uint32_t kBar = 0x03E0u;
const uint32_t kWhite = 0x7FFFu;
}  // namespace

int main(void) {
	vramDiagInitPS1();
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

constexpr int kMarginRows = 16;
constexpr int kTileW = 24;
constexpr int kTileH = 16;
constexpr uint32_t kBlack = 0x0000u;

// Etats de la boucle : titre/menu, introduction A1/A2, zone jouable, vraie fin.
constexpr int kModeMenu = 0;
constexpr int kModeIntro = 1;
constexpr int kModeGame = 2;
constexpr int kModeEnding = 3;
constexpr int kModeInventory = 4;
constexpr int kModeGameOver = 5;

inline int clampi(int v, int lo, int hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

// Theme A2 du flux CD : titre (data/world.txt "music titre 1"), puis la
// premiere piste de la categorie de la zone (data/music.txt).
inline int themeForZone(int zone) {
	switch (zone) {
	case QST_ZONE_PACO:
	case QST_ZONE_TOLEDO:
	case QST_ZONE_MEDUSA:  return 17;  // village
	case QST_ZONE_KINDRA:
	case QST_ZONE_EPAVE:
	case QST_ZONE_EDEN:
	case QST_ZONE_MANOIR:  return 25;  // donjon
	case QST_ZONE_TRONE:   return 21;  // boss
	default:               return 1;   // plaine (Vallee de Mithra) / titre
	}
}

// Seul point de chargement de carte par zone : passages ET reprise de partie.
// Avant v0.8.26, la reprise laissait la carte 011 active quelle que soit la
// zone sauvegardee. 026 n'est pas embarquee : elle est lue depuis le CD.
// Retourne 0 si la carte n'a pas pu etre chargee (la carte courante reste).
int loadZoneMap(int zone) {
	switch (zone) {
	case QST_ZONE_KINDRA: zone013Init(); return 1;
	case QST_ZONE_EPAVE:  zone019Init(); return 1;
	case QST_ZONE_TOLEDO: zone119Init(); return 1;
	case QST_ZONE_TRONE:  zone120Init(); return 1;
	case QST_ZONE_VALLEE: return zoneLoadFromCdPS1(QST_ZONE_VALLEE);
	case QST_ZONE_EDEN:   return zoneLoadFromCdPS1(QST_ZONE_EDEN);
	case QST_ZONE_MEDUSA: return zoneLoadFromCdPS1(QST_ZONE_MEDUSA);
	case QST_ZONE_MANOIR: return zoneLoadFromCdPS1(QST_ZONE_MANOIR);
	case QST_ZONE_PACO:   zoneInit(); return 1;
	default:              return 0;
	}
}

}  // namespace

int main(void) {
	zoneHeapInitPS1();  // arene malloc capture-only, avant tout appel de jeu
	vramDiagInitPS1();  // table VRAM passive, avant tout televersement
	spuDiagInitPS1();   // table SPU passive, avant tout transfert DMA
	pfInit();
	diagInitPS1();      // peinture de pile + etat heap, avant tout appel de jeu
	saveEarlyInitPS1(); // BIOS carte avant le premier VSync de psxgpu
	audioInitPS1();     // SPU materiel : banque VAG televersee une seule fois
	// Jingle resident demarre puis immediatement relaye par le flux CD sur la
	// meme voix 23 : preserve le marqueur audioPlayMusicPS1 de la sonde v0.8.1.
	audioPlayMusicPS1(0);
	audioStreamMusicPS1(1);  // theme du titre, flux CD VAGi
	zoneInit();
	heroInit();
	dlgInit();          // televerse FONT3.TIM et WIND.TX/WIND.CL en VRAM
	hudInitPS1();       // gemmes A1 du HUD (a1_hud.a1s)
	interactionInitPS1();
	menuInitPS1(saveHasGamePS1(), 0);
	inventoryInitPS1(); // objets A2 + icones A1 natives du menu
	introInitPS1(0);    // reinitialisee a chaque nouvelle partie

	int mode = kModeMenu;
	int zone = QST_ZONE_PACO;
	WorldWarpState warp;
	worldWarpStateInit(&warp);
	bool armed = false;   // commandes armees seulement apres relachement complet
	uint32_t prevPad = 0;
	uint32_t menuPrevPad = 0;
	uint32_t invPrevPad = 0;
	int endingTimer = 0;
	int gameOverTimer = 0;
	bool endingMusicStarted = false;
	int saveChecked = 0;
	uint32_t bootFrames = 0;

	for (;;) {
		const uint32_t pad = pfPad();
		++bootFrames;

		/* Le BIOS carte avance par VBlank : une commande par image, sans
		 * bloquer la manette ni le compteur de capture. */
		savePumpPS1();
		diagSamplePS1();
		zoneHeapSamplePS1();  // au plus une commande capture par image
		audioPumpPS1();

		if (mode == kModeMenu) {
			/* La detection carte est asynchrone : on attend sa fin avant de
			 * figer l'etat du menu (entree "LOAD GAME"). */
			if (!saveChecked && bootFrames >= 2 && saveInitPS1()) {
				menuSetHasSavePS1(saveHasGamePS1());
				saveChecked = 1;
			}
			// Front montant : un seul clic par direction, meme si le bouton
			// reste appuye (le menu lit le niveau, le SPU recoit l'evenement).
			const uint32_t menuEdge = pad & ~menuPrevPad;
			menuPrevPad = pad;
			const uint32_t menuNav = PF_BTN_UP | PF_BTN_DOWN | PF_BTN_LEFT |
			                         PF_BTN_RIGHT | PF_BTN_START | PF_BTN_CROSS;
			if (menuEdge & menuNav) audioPlaySfxIdPS1(1);
			menuUpdatePS1(pad);
			if (menuScreenPS1() != MENU_SCR_DONE) {
				menuDrawPS1();
				pfPresent();
				continue;
			}
			// "Quitter" n'a pas d'equivalent systeme sur PS1 : on revient au
			// titre avec le meme etat de manette, sans lancer l'intro ni changer
			// la sauvegarde.
			if (menuActionPS1() == MENU_ACT_QUIT) {
				menuReturnToTitlePS1();
				menuDrawPS1();
				pfPresent();
				continue;
			}
			// Nouvelle partie OU reprise : on prepare d'abord un etat propre,
			// puis la sauvegarde ecrase les trois modules concernes si necessaire.
			const bool wantLoad = menuActionPS1() == MENU_ACT_LOAD_GAME;
			dlgInit();
			hudInitPS1();
			zoneInit();
			heroInit();
			interactionInitPS1();
			inventoryInitPS1();
			worldWarpStateInit(&warp);
			armed = false;
			gameOverTimer = 0;
			prevPad = pad;

			int loaded = 0;
			if (wantLoad) {
				int lx = 0, ly = 0, ld = 0;
				loaded = saveReadGamePS1(&zone, &lx, &ly, &ld);
				// La carte de la zone sauvegardee, pas celle de Paco.
				if (loaded && !loadZoneMap(zone)) loaded = 0;
				if (loaded) {
					heroSetPos(lx, ly);
					heroSetDirPS1(ld);
					interactionSetZonePS1(zone);
					// La sauvegarde automatique est ecrite a l'arrivee d'un passage ;
					// en vallee, cette case est aussi le depart du retour (026 26 52).
					// Sans verrou, la reprise renvoyait aussitot a Toroledo.
					worldWarpStateLock(&warp, zone, lx / kTileW, ly / kTileH);
				}
			}
			if (loaded) {
				audioStreamMusicPS1(themeForZone(zone));
				mode = kModeGame;  // sauvegarde valide : introduction sautee
			} else {
				// Aucune sauvegarde exploitable : nouvelle partie complete.
				zone = QST_ZONE_PACO;
				zoneInit();
				heroInit();
				interactionInitPS1();
				inventoryInitPS1();
				worldWarpStateInit(&warp);
				// Le START qui a valide le menu ne doit pas passer l'intro.
				introInitPS1(pad);
				mode = kModeIntro;
				introDrawPS1();
				pfPresent();
				continue;
			}
		}

		if (mode == kModeIntro) {
			introUpdatePS1(pad);
			if (!introDonePS1()) {
				introDrawPS1();
				pfPresent();
				continue;
			}
			audioStreamMusicPS1(themeForZone(zone));
			mode = kModeGame;
			armed = false;
			prevPad = pad;
			interactionAfterIntroPS1();  // replique de reveil de Flint (PC)
		}

		// Inventaire armes / objets / equipement (START en jeu, CERCLE pour fermer).
		// Ecran dedie : aucun appel a la physique ni aux warps tant qu'il est ouvert.
		if (mode == kModeInventory) {
			const uint32_t invEdge = pad & ~invPrevPad;
			invPrevPad = pad;
			if (invEdge & (PF_BTN_CIRCLE | PF_BTN_CROSS | PF_BTN_START)) {
				audioPlaySfxIdPS1(3);
			}
			inventoryUpdatePS1(pad);
			inventoryDrawPS1();
			if (inventoryIsOpenPS1() == 0) {
				mode = kModeGame;
				armed = false;  // exige le relachement avant de re-armer la manette
				prevPad = pad;
			}
			pfPresent();
			continue;
		}

		// Vraie fin : six lignes A2 dans une fenetre A1, sans HUD ni dialogue.
		if (mode == kModeEnding) {
			if (!endingMusicStarted) {
				audioStreamStopPS1();
				audioPlayMusicPS1(0);  // jingle "victoire" resident, une fois
				endingMusicStarted = true;
			}
			const uint32_t endingEdge = pad & ~prevPad;
			prevPad = pad;
			endingDrawPS1(endingTimer);
			++endingTimer;
			if (endingTimer > 420 ||
			    (endingTimer > 90 && (endingEdge & PF_BTN_START))) {
				int ex = 0, ey = 0;
				heroPos(&ex, &ey);
				saveWriteGamePS1(zone, ex, ey, heroDir());
				mode = kModeMenu;
				endingTimer = 0;
				gameOverTimer = 0;
				zone = QST_ZONE_PACO;
				zoneInit();
				heroInit();
				interactionInitPS1();
				worldWarpStateInit(&warp);
				armed = false;
				menuInitPS1(saveHasGamePS1(), 0);
				menuUpdatePS1(pad);
				audioStreamMusicPS1(1);  // retour titre
			}
			pfPresent();
			continue;
		}

		// Game Over local : aucun warp, aucune ecriture carte, aucun dialogue.
		// CARRÉ (jamais CROIX) ou le delai remettent le heros debout sur place.
		if (mode == kModeGameOver) {
			const uint32_t overEdge = pad & ~prevPad;
			prevPad = pad;
			gameOverDrawPS1(gameOverTimer);
			++gameOverTimer;
			if (gameOverTimer > 150 ||
			    (gameOverTimer > 40 && (overEdge & PF_BTN_SQUARE))) {
				heroVitalsRespawn(&g_ps1HeroVitals);
				mode = kModeGame;
				gameOverTimer = 0;
				armed = false;
			}
			pfPresent();
			continue;
		}

		// SELECT : retour au titre, avec front montant pour eviter le rebond.
		const uint32_t edge = pad & ~prevPad;
		prevPad = pad;
		if (edge & PF_BTN_SELECT) {
			int sx = 0, sy = 0;
			heroPos(&sx, &sy);
			saveWriteGamePS1(zone, sx, sy, heroDir());
			mode = kModeMenu;
			endingTimer = 0;
			gameOverTimer = 0;
			armed = false;
			zone = QST_ZONE_PACO;
			zoneInit();
			interactionInitPS1();
			worldWarpStateInit(&warp);
			menuInitPS1(saveHasGamePS1(), 0);
			menuUpdatePS1(pad);
			menuDrawPS1();
			audioStreamMusicPS1(1);  // retour titre
			pfPresent();
			continue;
		}
		// Un bouton reste appuye a la fermeture du menu : on n'alimente pas le jeu
		// tant que la manette n'a pas ete relachee.
		if (!armed && pad == 0) armed = true;
		const uint32_t gamePad = armed ? pad : 0u;

		// START ouvre l'inventaire hors dialogue (front montant, manette armee).
		if (armed && !dlgActive() && (gamePad & PF_BTN_START)) {
			audioPlaySfxIdPS1(63); // ouverture de menu (A2)
			inventoryOpenPS1(pad);
			invPrevPad = pad;
			mode = kModeInventory;
			inventoryDrawPS1();
			pfPresent();
			continue;
		}

		// CROIX sert a parler quand un PNJ est a portee, sinon a sauter : on
		// consomme le front montant avant la physique pour ne pas sauter en parlant.
		int hx = 0;
		int hy = 0;
		heroPos(&hx, &hy);
		uint32_t heroPad = gamePad;
		if (interactionWantsTalkPS1(gamePad, hx, hy)) heroPad &= ~PF_BTN_CROSS;
		heroUpdate(heroPad, pfFrame());
		heroPos(&hx, &hy);

		// Passages A→Z subset (011 ↔ 013 ↔ 019 ↔ 119), retour libre depuis
		// le trone, et 086→120 uniquement apres la chaine complete des gates.
		// Le verrou evite le rebond quand une arrivee tombe sur le depart.
		int toZone = 0, toX = 0, toY = 0;
		if (!dlgActive()) {
			const int cx = hx / kTileW;
			const int cy = hy / kTileH;
			const int warpZone = interactionCaptureZonePS1() != 0
			                     ? interactionCaptureZonePS1() : zone;
			if (!worldTryWarp120(interactionGatesPS1(), warpZone, cx, cy,
			                     &toZone, &toX, &toY) &&
			    !worldTryWarpGated(&warp, interactionGatesPS1(), zone, cx, cy,
			                       &toZone, &toX, &toY)) {
				worldTryWarp(&warp, zone, cx, cy, &toZone, &toX, &toY);
			}
		}
		// Lecture CD impossible : le passage est annule, la zone courante reste.
		if (toZone && !loadZoneMap(toZone)) {
			toZone = 0;
			audioStreamMusicPS1(themeForZone(zone));
		}
		if (toZone) {
			heroSetPos(toX * kTileW + kTileW / 2, toY * kTileH + kTileH / 2);
			interactionSetZonePS1(toZone);
			if (toZone == QST_ZONE_TRONE) interactionSetCaptureZonePS1(0);
			worldWarpStateLock(&warp, toZone, toX, toY);
			if (themeForZone(toZone) != themeForZone(zone))
				audioStreamMusicPS1(themeForZone(toZone));
			zone = toZone;
			heroPos(&hx, &hy);
			saveWriteGamePS1(zone, hx, hy, heroDir());
		}

		const int camX = clampi(hx - PF_W / 2, 0, zoneW() * kTileW - PF_W);
		// Camera verticale PC (followHero) : suit le heros a son niveau de terrain.
		const int camY = clampi(hy + heroElevPS1() - PF_H / 2 - 16, 0,
		                        (zoneH() + kMarginRows) * kTileH - PF_H);

		// Ordre d'ajout = inverse de l'ordre de dessin a z egal (addPrim en tete).
		// Le PC dessine chaque entite APRES les tuiles de sa rangee (Map::draw,
		// afterRow) : heros et PNJ sont donc ajoutes AVANT le decor pour passer
		// devant les tuiles de leur rangee (ils restaient caches dans l'herbe de
		// Paco, v0.8.37), et le fond noir en dernier pour rester derriere tout.
		heroDraw(camX, camY);
		interactionDrawPS1(camX, camY);  // PNJ, bulles et objectif de quete
		zoneDraw(camX, camY, 0);
		pfDrawRect(0, 0, PF_W, PF_H, kBlack, 63);

		// Dialogue A1 natif : fenetre WIND.TX + police FONT3.TIM, texte A2.
		// interactionUpdatePS1() gere aussi l'ouverture et la fin du dialogue.
		interactionUpdatePS1(gamePad, hx, hy);

		// HUD : gemmes de vie natives A1 (a1_hud.a1s). DOIT etre appele APRES
		// interactionUpdatePS1() : sinon le rendu affiche les PV de la frame
		// precedente (observe en capture : 1 gemme eteinte au lieu de 2 a 8 PV).
		hudSetVitalsPS1(g_ps1HeroVitals.hp, HERO_VITALS_MAX_HP);
		hudDrawPS1();

		if (heroVitalsDown(&g_ps1HeroVitals)) {
			dlgClose();
			mode = kModeGameOver;
			gameOverTimer = 0;
			armed = false;
			prevPad = pad;
			pfPresent();
			continue;
		}
		if (interactionEndingPS1()) {
			dlgClose();
			mode = kModeEnding;
			endingTimer = 0;
			endingMusicStarted = false;
			armed = false;
			prevPad = pad;
			pfPresent();
			continue;
		}
		dlgDraw();

		pfPresent();
	}
}

#endif
