/*
 * Interactions PS1 : PNJ de Paco, Kindra, Epave et Toroledo, bulles, objectif.
 *
 * Les PNJ utilisent les VRAIES frames A1 exportees dans npc.dat : aucun
 * rectangle de couleur invente. Une seule zone est active : trois slots VRAM
 * suffisent, reteleverses a chaque passage. Les cadres viennent de WIND.TX et
 * les glyphes de FONT3.TIM, comme le dialogue natif.
 *
 * IMPORTANT (cause racine d'une capture precedente) : addPrim() insere en tete
 * de l'ordering table et plus le z est petit, plus le primitif est dessine TARD.
 * Un texte au meme z que son fond est donc recouvert. Tous les fonds sont en
 * z=2 et tous les textes en z=0.
 *
 * Sous-ensemble C++ sans STL : un seul etat POD, aucun destructeur global.
 */
#include "interaction_ps1.h"

#include "../platform/pf.h"
#include "audio_ps1.h"
#include "dialogue_ps1.h"
#include "enemy_ps1_logic.h"
#include "hero_ps1.h"
#include "menu_inventory_ps1.h"
#include "ogre_combat_ps1_logic.h"
#include "quest_ps1_logic.h"
#include "window_ps1.h"
#include "zone011.h"

#include <stddef.h>

extern "C" {
extern const uint8_t npc_dat[];
extern const uint8_t actor_dat[];
extern const uint8_t enemy_anim_dat[];
}

/* Etat global et observable par la sonde Lua (adresse ELF), sans STL. */
PS1EnemyAnimState g_ps1EnemyAnim = {0, 0, PS1_ENEMY_ANIM_FRAMES, 0, 0};
/* Duel local : observable par la sonde Lua. Le pont avec g_quest est unique :
 * le pilote appelle questOgreDefeated() sur la transition reelle vers DOWN. */
PS1OgreDuelState g_ps1OgreDuel = {0, 0, 0, 0, 0, 0, 0, 0, 0};
/* Constantes vitales locales du heros : aucun lien avec la quete ou la save. */
PS1HeroVitalsState g_ps1HeroVitals = {
    HERO_VITALS_MAX_HP, 0, 0, 0, 0, 0};

const PS1HeroVitalsState* interactionVitalsPS1(void) { return &g_ps1HeroVitals; }

namespace {

constexpr int kZWindow = 2;  // fond de bulle / bandeau : derriere le texte
constexpr int kZText = 0;    // glyphes FONT3 : premier plan
constexpr int kWindBank = 0;
constexpr int kHeroDamageSfx = 71;  // A2 "degats" (data/sfx.txt:17), deja embarque

struct NpcSprite {
	const uint8_t* pixels;
	const uint8_t* clut;
	int w, h, ox, oy;
	int stride, uploadH;
	int texX, texY;
	int clutX, clutY;
	int zone, slot;
};

struct ActorSprite {
	const uint8_t* pixels;
	const uint8_t* clut;
	int w, h, ox, oy;
	int stride, uploadH;
	int texX, texY;
	int clutX, clutY;
	int zone, slot;
};

struct EnemyFrame {
	const uint8_t* pixels;
	const uint8_t* clut;
	int w, h, ox, oy;
	int stride, uploadH;
};

NpcSprite g_sprites[QST_NPC_COUNT];
ActorSprite g_actors[QST_ACTOR_COUNT];
EnemyFrame g_enemyFrames[PS1_ENEMY_ANIM_FRAMES];
int g_enemyUploaded = -1;
QuestState g_quest;
/* Uniquement ecrit par le harnais Lua via l'adresse ELF : simule la zone 086
 * le temps de franchir le warp conditionne, sans embarquer la carte du manoir. */
int g_ps1CaptureZone;

inline uint16_t rd16(const uint8_t* p) {
	return (uint16_t)(p[0] | (p[1] << 8));
}

inline int16_t rd16s(const uint8_t* p) {
	return (int16_t)rd16(p);
}

void loadSprites(void) {
	const uint8_t* p = npc_dat + 4;  // magic P1NP
	const int count = rd16(p);
	p += 4;  // count + reserved
	for (int i = 0; i < count && i < QST_NPC_COUNT; ++i) {
		NpcSprite& s = g_sprites[i];
		s.w = rd16(p);
		s.h = rd16(p + 2);
		s.ox = rd16s(p + 4);
		s.oy = rd16s(p + 6);
		s.stride = rd16(p + 8);
		s.uploadH = rd16(p + 10);
		p += 12;
		s.texX = rd16(p);
		s.texY = rd16(p + 2);
		s.clutX = rd16(p + 4);
		s.clutY = rd16(p + 6);
		s.zone = rd16(p + 8);
		s.slot = rd16(p + 10);
		p += 12;
		s.clut = p;
		p += 32;  // 16 mots CLUT
		s.pixels = p;
		p += (size_t)s.stride * 2u * (size_t)s.uploadH;
	}
}

void uploadNpcSprites(int zone) {
	for (int i = 0; i < QST_NPC_COUNT; ++i) {
		const NpcSprite& s = g_sprites[i];
		if (s.zone != zone) continue;
		pfUpload16(s.clutX, s.clutY, 16, 1,
		           reinterpret_cast<const uint16_t*>(s.clut));
		pfUpload4bpp(s.texX, s.texY, s.stride, s.uploadH, s.pixels);
	}
}

void loadActors(void) {
	const uint8_t* p = actor_dat + 4;  // magic P1AC
	const int count = rd16(p);
	p += 4;
	for (int i = 0; i < count && i < QST_ACTOR_COUNT; ++i) {
		ActorSprite& a = g_actors[i];
		a.w = rd16(p);
		a.h = rd16(p + 2);
		a.ox = rd16s(p + 4);
		a.oy = rd16s(p + 6);
		a.stride = rd16(p + 8);
		a.uploadH = rd16(p + 10);
		p += 12;
		a.texX = rd16(p);
		a.texY = rd16(p + 2);
		a.clutX = rd16(p + 4);
		a.clutY = rd16(p + 6);
		a.zone = rd16(p + 8);
		a.slot = rd16(p + 10);
		p += 12;
		a.clut = p;
		p += 32;
		a.pixels = p;
		p += (size_t)a.stride * 2u * (size_t)a.uploadH;
	}
}

void uploadActors(int zone) {
	for (int i = 0; i < QST_ACTOR_COUNT; ++i) {
		const ActorSprite& a = g_actors[i];
		if (a.zone != zone) continue;
		pfUpload16(a.clutX, a.clutY, 16, 1,
		           reinterpret_cast<const uint16_t*>(a.clut));
		pfUpload4bpp(a.texX, a.texY, a.stride, a.uploadH, a.pixels);
	}
}

void loadEnemyAnim(void) {
	const uint8_t* p = enemy_anim_dat + 4;  // magic P1AN
	const int count = rd16(p);
	p += 4;
	if (count > PS1_ENEMY_ANIM_FRAMES) return;
	for (int i = 0; i < count; ++i) {
		EnemyFrame& f = g_enemyFrames[i];
		f.w = rd16(p);
		f.h = rd16(p + 2);
		f.ox = rd16s(p + 4);
		f.oy = rd16s(p + 6);
		f.stride = rd16(p + 8);
		f.uploadH = rd16(p + 10);
		p += 12;
		p += 8;  // texX, texY, clutX, clutY : fixes par pf.h
		f.clut = p;
		p += 32;
		f.pixels = p;
		p += (size_t)f.stride * 2u * (size_t)f.uploadH;
	}
	enemyAnimSetFrames(&g_ps1EnemyAnim, count);
}

void uploadEnemyFrame(int frame) {
	if (frame < 0 || frame >= (int)g_ps1EnemyAnim.frames) return;
	if (frame == g_enemyUploaded) return;
	const EnemyFrame& f = g_enemyFrames[frame];
	pfUpload16(PF_ENEMY_CLUT_X, PF_ENEMY_CLUT_Y, 16, 1,
	           reinterpret_cast<const uint16_t*>(f.clut));
	pfUpload4bpp(PF_ENEMY_TEX_X, PF_ENEMY_TEX_Y, f.stride, f.uploadH, f.pixels);
	g_enemyUploaded = frame;
}

int npcZ(int ny) {
	const int z = 63 - ((ny / 16) & 63);
	return z > 0 ? (z < 63 ? z : 63) : 0;
}

void drawGlyph(unsigned char c, int x, int y) {
	const int idx = c >= 32 ? int(c) : int('?');
	const int col = idx & 15;
	const int row = idx >> 4;
	pfDrawSprite16(x, y, PF_FONT_X + col * 4, PF_FONT_Y + row * 16, 16, 16,
	               PF_FONT_CLUT_X, PF_FONT_CLUT_Y, kZText);
}

void drawText(const char* text, int x, int y, int limit) {
	for (int i = 0; text[i] != '\0' && i < limit; ++i) {
		drawGlyph(static_cast<unsigned char>(text[i]), x + i * 16, y);
	}
}

void drawNpc(int npc, int camX, int camY) {
	int nx = 0, ny = 0;
	questNpcPos(npc, &nx, &ny);
	const NpcSprite& s = g_sprites[npc];
	pfDrawSprite16(nx - camX - s.ox, ny + zoneElevAt(nx, ny) - camY - s.oy,
	               s.texX, s.texY, s.w, s.h, s.clutX, s.clutY, npcZ(ny));

	if (!questNpcBubble(&g_quest, npc)) return;
	// Cadre WIND.TX + glyphe FONT3 : le '!' est un vrai glyphe, pas un carre.
	const int bx = nx - camX - 36;
	const int by = ny + zoneElevAt(nx, ny) - camY - 56;
	a1window::draw(bx, by, 72, 26, kWindBank, kZWindow);
	drawGlyph('!', bx + 28, by + 5);
}

/* Boss de la zone courante : une seule zone est active, donc un seul duel.
 * Kindra -> ogre (bossDown) ; Vallee de Mithra -> golem (QST_FLAG_GOLEM). */
int duelActor(void) {
	return g_quest.zone == QST_ZONE_VALLEE ? QST_ACTOR_GOLEM : QST_ACTOR_OGRE;
}
int duelDone(void) {
	return g_quest.zone == QST_ZONE_VALLEE ? ((g_quest.gates & QST_FLAG_GOLEM) != 0)
	                                       : g_quest.bossDown;
}
void duelWon(void) {
	if (g_quest.zone == QST_ZONE_VALLEE) questGolemDefeated(&g_quest);
	else questOgreDefeated(&g_quest);
}

void drawActor(int actor, int camX, int camY) {
	int ax = 0, ay = 0;
	questActorPos(actor, &ax, &ay);
	// Pendant l'etourdissement/la chute locale, la frame statique de actor.dat
	// remplace la frame animee : enemyAnimPump continue mais n'est pas affichee.
	if (actor == QST_ACTOR_OGRE && g_quest.bossDown == 0 &&
	    !ogreDuelStunned(&g_ps1OgreDuel)) {
		const int frame = enemyAnimFrame(&g_ps1EnemyAnim);
		if (frame >= 0 && frame < (int)g_ps1EnemyAnim.frames) {
			uploadEnemyFrame(frame);
			const EnemyFrame& e = g_enemyFrames[frame];
			pfDrawSprite16(ax - camX - e.ox, ay + zoneElevAt(ax, ay) - camY - e.oy,
			               PF_ENEMY_TEX_X, PF_ENEMY_TEX_Y, e.w, e.h,
			               PF_ENEMY_CLUT_X, PF_ENEMY_CLUT_Y, npcZ(ay));
			return;
		}
	}
	const ActorSprite& a = g_actors[actor];
	pfDrawSprite16(ax - camX - a.ox, ay + zoneElevAt(ax, ay) - camY - a.oy,
	               a.texX, a.texY, a.w, a.h, a.clutX, a.clutY, npcZ(ay));
}

/* Objets reellement recus (comme items.cpp cote PC) : chaque transition
 * 0 -> 1 d'un drapeau de quete sauvegarde ajoute l'objet une seule fois. */
void grantQuestItems(const QuestState* a, const QuestState* b) {
	if (!a->hasStrangeKey && b->hasStrangeKey) inventoryAddPS1("strange_key", 1);
	if (!a->chestLettre && b->chestLettre) inventoryAddPS1("lettre", 1);
	if (!(a->gates & QST_FLAG_ORDRE) && (b->gates & QST_FLAG_ORDRE))
		inventoryAddPS1("ordre", 1);
	if (!(a->gates & QST_GATE_EPEE_SUPREME) && (b->gates & QST_GATE_EPEE_SUPREME))
		inventoryAddPS1("epee_supreme", 1);
	if (!(a->gates & QST_GATE_LAMPE) && (b->gates & QST_GATE_LAMPE))
		inventoryAddPS1("lampe", 1);
}

void drawObjective(void) {
	if (!questObjectiveVisible(&g_quest)) return;
	a1window::draw(10, 20, 300, 24, kWindBank, kZWindow);
	if (g_quest.stage == QST_STAGE_BOSS_VAINCU) {
		drawText("Vers Toroledo", 36, 24, 13);
	} else if (g_quest.stage >= QST_STAGE_TOLEDO) {
		drawText("Vers la Vallee", 36, 24, 14);
	} else if (g_quest.stage >= QST_STAGE_EPAVE) {
		drawText("Vers Toroledo", 36, 24, 13);
	} else if (g_quest.stage >= QST_STAGE_KINDRA) {
		drawText("Vers l'epave", 36, 24, 12);
	} else if (g_quest.stage >= QST_STAGE_PACO_PREVENU) {
		drawText("Paco prevenu!", 36, 24, 13);
	} else {
		drawText("Objectif Alexia", 36, 24, 15);
	}
}

}  // namespace

void interactionInitPS1(void) {
	questInit(&g_quest);
	g_ps1CaptureZone = 0;
	dlgClose();
	loadSprites();
	loadActors();
	enemyAnimInit(&g_ps1EnemyAnim);
	ogreDuelInit(&g_ps1OgreDuel);
	heroVitalsInit(&g_ps1HeroVitals);
	loadEnemyAnim();
	uploadNpcSprites(g_quest.zone);
	uploadActors(g_quest.zone);
	uploadEnemyFrame(0);
}

void interactionSetZonePS1(int zone) {
	questSetZone(&g_quest, zone);
	enemyAnimReset(&g_ps1EnemyAnim);
	ogreDuelReset(&g_ps1OgreDuel);
	uploadNpcSprites(zone);
	uploadActors(zone);
	uploadEnemyFrame(0);
	if (zone == QST_ZONE_TRONE) questThroneEnter(&g_quest);
	questZoneArrival(&g_quest);  // narration unique (vallee)
}

int interactionWantsTalkPS1(uint32_t pad, int hx, int hy) {
	if (dlgActive()) return 0;
	const uint32_t pressed = pad & ~g_quest.prevPad;
	if (!(pressed & PF_BTN_CROSS)) return 0;
	// L'ogre ne se "parle" pas : CROIX ne peut pas contourner le combat.
	// Une fois DOWN, questOgreDefeated() a pose bossDown et l'acteur n'est
	// plus disponible ; CROIX ne sert alors qu'a confirmer le dialogue.
	if (!ogreDuelInteractAllowed(&g_ps1OgreDuel) &&
	    questActorNear(&g_quest, hx, hy, heroDir()) == duelActor() &&
	    questNpcNear(&g_quest, hx, hy, heroDir()) == QST_NPC_NONE) return 0;
	return questHasInteract(&g_quest, hx, hy, heroDir());
}

void interactionUpdatePS1(uint32_t pad, int hx, int hy) {
	// Duel local : CARRE frappe, CROIX ne peut pas contourner le combat.
	// L'etat dlgActive() est celui de la frame precedente : front deterministe.
	// questOgreDefeated() n'est appele que sur la transition reelle vers DOWN.
	int ax = 0, ay = 0;
	questActorPos(duelActor(), &ax, &ay);
	const int dx = hx - ax;
	const int dy = hy - ay;
	const int inRange = questActorAvailable(&g_quest, duelActor()) &&
	                    duelDone() == 0 && dx * dx + dy * dy <= 40 * 40 ? 1 : 0;
	const int square = (!dlgActive() && (pad & PF_BTN_SQUARE)) ? 1 : 0;
	// Copie 20 octets avant mutation : le mapping SFX reste une fonction pure et
	// n'ajoute aucun champ a l'etat observable par la sonde.
	const PS1OgreDuelState before = g_ps1OgreDuel;
	const int hit = ogreDuelTryHit(&g_ps1OgreDuel, duelDone(), square, inRange);
	const int sfx = ogreDuelSfxForHit(&before, &g_ps1OgreDuel, hit);
	if (sfx != OGRE_DUEL_SFX_NONE) audioPlaySfxIdPS1(sfx);
	// Victoire reellement obtenue au combat : consequence de quete unique.
	// questOgreDefeated() pose bossDown, stage=6 et le dialogue de victoire ;
	// ses guards rendent tout appel ulterieur sans effet (pas de double lot).
	if (ogreDuelJustWon(&before, &g_ps1OgreDuel, hit)) duelWon();

	// Contre-attaque de l'ogre : uniquement en GARDE, a portee, hors dialogue.
	// Le POD vital ne touche jamais a la quete ni a la sauvegarde.
	heroVitalsTick(&g_ps1HeroVitals);
	const int ogreGuard = g_ps1OgreDuel.state == OGRE_DUEL_GUARD ? 1 : 0;
	const int guardActive = dlgActive() ? 0 : ogreGuard;
	if (heroVitalsTryHit(&g_ps1HeroVitals, 1, guardActive, inRange) != 0)
		audioPlaySfxIdPS1(kHeroDamageSfx);

	// CROIX est masque pour questStep tant que le duel n'est pas termine :
	// sinon questActorInteract() poserait bossDown sans combat (contournement).
	uint32_t questPad = pad;
	if (!ogreDuelInteractAllowed(&g_ps1OgreDuel) &&
	    questActorNear(&g_quest, hx, hy, heroDir()) == duelActor() &&
	    questNpcNear(&g_quest, hx, hy, heroDir()) == QST_NPC_NONE)
		questPad &= ~PF_BTN_CROSS;
	const QuestState prev = g_quest;
	questStep(&g_quest, questPad, hx, hy, heroDir());
	if (g_quest.dialogueId != QST_DLG_NONE && !dlgActive()) dlgOpen(g_quest.dialogueId);
	dlgUpdate(pad);
	if (dlgConsumeFinished()) questDialogueDone(&g_quest);
	grantQuestItems(&prev, &g_quest);
}

void interactionDrawPS1(int camX, int camY) {
	// Une avance par image presentee. Le gel de l'animation v0.8.8 reste pilote
	// par bossDown uniquement : aucun observable existant n'est modifie.
	ogreDuelStep(&g_ps1OgreDuel, duelDone());
	enemyAnimPump(&g_ps1EnemyAnim, g_quest.bossDown);
	for (int npc = 0; npc < QST_NPC_COUNT; ++npc) {
		if (questNpcAvailable(&g_quest, npc)) drawNpc(npc, camX, camY);
	}
	for (int actor = 0; actor < QST_ACTOR_COUNT; ++actor) {
		if (questActorAvailable(&g_quest, actor)) drawActor(actor, camX, camY);
	}
	// Flash du duel : trois tires au-dessus de l'ogre, entre le sprite et le
	// texte (z plus petit = dessine plus tard). Aucune CLUT ni VRAM supplementaire.
	if (g_ps1OgreDuel.flash != 0 && questActorAvailable(&g_quest, duelActor())) {
		int ax = 0, ay = 0;
		questActorPos(duelActor(), &ax, &ay);
		int z = npcZ(ay) - 1;
		if (z < 1) z = 1;
		const uint32_t color = g_ps1OgreDuel.flash > OGRE_DUEL_ABSORB_FLASH_TICKS
		                     ? 0x7FFFu : 0x4210u;
		for (int i = 0; i < 3; ++i) {
			pfDrawRect(ax - camX - 12 + i * 10, ay + zoneElevAt(ax, ay) - camY - 58, 4, 1, color, z);
		}
	}
	drawObjective();
}

int interactionStagePS1(void) { return g_quest.stage; }
int interactionDialoguePS1(void) { return g_quest.dialogueId; }
int interactionNearNpcPS1(int hx, int hy) {
	return questNpcNear(&g_quest, hx, hy, heroDir());
}
int interactionNearActorPS1(int hx, int hy) {
	return questActorNear(&g_quest, hx, hy, heroDir());
}
uint32_t interactionGatesPS1(void) { return g_quest.gates; }
void interactionQuestPS1(QuestState* out) {
	if (out) *out = g_quest;
}
void interactionSetQuestPS1(const QuestState* in) {
	if (in) g_quest = *in;
}
int interactionEndingPS1(void) { return g_quest.ending; }
int interactionCaptureZonePS1(void) { return g_ps1CaptureZone; }
void interactionSetCaptureZonePS1(int zone) { g_ps1CaptureZone = zone; }
