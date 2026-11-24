/*
 * Backend PS1 de l'interface minimale pf (PSn00bSDK 0.24).
 *
 * Sous-ensemble C++ sans STL : tableaux statiques, POD, aucun destructeur
 * global (le libc du SDK n'a pas atexit). Couleurs BGR555 15 bits.
 */
#include "../platform/pf.h"
#include "save_ps1.h"
#include "spu_diag_ps1.h"
#include "timing_ps1.h"
#include "vram_diag_ps1.h"

#include <psxapi.h>
#include <psxgpu.h>
#include <psxpad.h>
#include <psxsio.h>

#include <stdio.h>

namespace {

constexpr int kOtLength = 64;
constexpr int kPrimBytes = 65536;

DISPENV g_disp[2];
DRAWENV g_draw[2];
uint32_t g_ot[2][kOtLength];
uint8_t g_prims[2][kPrimBytes];
uint8_t* g_next = 0;
int g_db = 0;
uint32_t g_frame = 0;

/* Le BIOS doit garder l'auto-ack VBlank actif pendant une operation carte :
 * sinon sio0Verifier() laisse IREG.VBLANK pose et chaque ACK SIO0 re-entre
 * firstStageCardAction(), ce qui abort l'operation (statut 0x11). Mais cet
 * auto-ack empeche le callback VBlank de psxgpu d'incrementer son compteur, donc
 * VSync() tourne. On remplace le halt par une attente dynamique : on ne rend la
 * main au driver PAD qu'une fois l'operation reellement terminee cote BIOS. */
void ps1VsyncHalt(void) {
	const int before = VSync(-1);

	if (!saveCardOpPendingPS1()) {
		/* Marche normale : on attend le prochain VBlank compte par le SDK. */
		for (int i = 0; i < 0x100000; ++i)
			if (VSync(-1) != before) return;
		ChangeClearPAD(0);  /* securite : auto-ack eventuellement reste actif */
		return;
	}

	/* Operation carte en vol : le compteur du SDK est gele. On scrute le statut
	 * BIOS jusqu'a la fin (ou une erreur 0x11/0x21), puis on desactive l'auto-ack
	 * et on attend le VBlank suivant pour que VSync() puisse retourner. */
	/* Juste apres _card_write/_card_read, le statut vaut encore 1 ("pret") pour
	 * l'operation PRECEDENTE : y voir une fin coupait l'auto-ack trop tot et le
	 * BIOS abandonnait l'operation (statut 0x11, sauvegardes perdues apres la
	 * 3e ecriture, v0.8.35). "Pret" ne compte qu'apres un etat occupe observe. */
	int sawBusy = 0;
	for (int i = 0; i < 500000; ++i) {
		const int st = saveCardStatusPS1();
		if (st != 1) sawBusy = 1;
		if ((st == 1 && (sawBusy || i > 20000)) || st == 0x11 || st == 0x21) break;
		if (VSync(-1) != before) return;  /* compteur deja avance : on sort */
	}
	ChangeClearPAD(0);
	for (int i = 0; i < 0x100000; ++i)
		if (VSync(-1) != before) return;
}
uint8_t g_pad_buf[2][34];
uint32_t g_pad = 0;

uint8_t bgr5_red(uint32_t c) { return (uint8_t)(((c & 0x1fu) * 255u) / 31u); }
uint8_t bgr5_green(uint32_t c) { return (uint8_t)((((c >> 5) & 0x1fu) * 255u) / 31u); }
uint8_t bgr5_blue(uint32_t c) { return (uint8_t)((((c >> 10) & 0x1fu) * 255u) / 31u); }

}  // namespace

/* Defini a la portee globale (pas dans l'anonyme) : la sonde Lua le lit par
 * symbole ELF. 1024 periodes de 16 bits = 2 Ko, sans allocation. */
PS1FrameTiming g_ps1FrameTiming = {0u, 0u, 0u, 0u, {0}};

void pfInit(void) {
	// NTSC explicite : 320x240 a 59,82 Hz, meme si la console est PAL.
	SetVideoMode(MODE_NTSC);
	ResetGraph(0);
	VSyncHaltFunction(&ps1VsyncHalt);

	// Le BIOS configure RCntCNT1 en clock Hblank (bit 8 du mode). Sous
	// OpenBIOS/PCSX-Redux, GetRCnt(RCntCNT1) rend de plus toujours 0 alors que
	// le compteur materiel avance (verifie le 2026-11-24). On reprogramme le
	// mode avec RCntMdFR (registre observe 0x0448, horloge systeme, sans IRQ)
	// et pfPresent() lit directement 0x1f801110. Aucun effet sur le rendu ni
	// sur les entrees.
	SetRCnt(RCntCNT1, 0xffff, RCntMdFR);

	// Deux framebuffers 320x240 places l'un au-dessus de l'autre en VRAM.
	// Chaque buffer a sa propre adresse de dessin et d'affichage (voir hellocpp).
	SetDefDispEnv(&g_disp[0], PF_FB0_X, PF_FB0_Y, PF_W, PF_H);
	SetDefDrawEnv(&g_draw[0], PF_FB0_X, PF_FB0_Y, PF_W, PF_H);
	SetDefDispEnv(&g_disp[1], PF_FB1_X, PF_FB1_Y, PF_W, PF_H);
	SetDefDrawEnv(&g_draw[1], PF_FB1_X, PF_FB1_Y, PF_W, PF_H);

	for (int i = 0; i < 2; ++i) {
		setRGB0(&g_draw[i], 8, 12, 32);
		g_draw[i].isbg = 1;
		g_draw[i].dtd = 1;
	}

	g_db = 0;
	g_frame = 0;
	g_next = g_prims[g_db];
	ClearOTagR(g_ot[g_db], kOtLength);

	// Reserve passive des deux framebuffers dans la table de mesure. Aucun
	// appel GPU n'est ajoute ni modifie : SetDefDispEnv/SetDefDrawEnv ci-dessus
	// restent seuls responsables du rendu. La frame 1 evite un pic a frame 0.
	vramDiagTrackPS1(PF_FB0_X, PF_FB0_Y, PF_W, PF_H, 1u);
	vramDiagTrackPS1(PF_FB1_X, PF_FB1_Y, PF_W, PF_H, 1u);

	// Manette : le driver BIOS remplit les deux buffers en permanence.
	// ChangeClearPAD(0) est obligatoire apres StartPAD(): le BIOS pose sinon
	// l'auto-ack SIO0/VBlank et VSync() tourne en timeout (voir LibPSn00b).
	InitPAD(&g_pad_buf[0][0], 34, &g_pad_buf[1][0], 34);
	StartPAD();
	ChangeClearPAD(0);

	PutDrawEnv(&g_draw[g_db]);
	PutDispEnv(&g_disp[g_db]);
	SetDispMask(1);
}

/* LoadImage lit la source par mots de 32 bits (DMA) : une adresse non
 * alignee sur 4 octets decale toute l'image d'un demi-mot. C'etait le cas
 * des CLUT et tuiles des blobs P1ZM a nombre de tuiles impair (offset
 * 14 + 10*nTiles + 4*nCluts) : points parasites a Toroledo, sable zebre a
 * Eden. Les sources mal alignees passent par un tampon aligne et le
 * transfert est attendu avant de rendre la main (tampon reutilise). */
static uint32_t s_alignBuf[2048];  // 8 Kio : plus grand televersement unitaire

static void loadImageAligned(RECT* r, const void* data, int bytes) {
	if ((reinterpret_cast<uintptr_t>(data) & 3u) == 0u ||
	    bytes > (int)sizeof(s_alignBuf)) {
		LoadImage(r, reinterpret_cast<const uint32_t*>(data));
		return;
	}
	__builtin_memcpy(s_alignBuf, data, (size_t)bytes);
	LoadImage(r, s_alignBuf);
	DrawSync(0);
}

void pfUpload4bpp(int x, int y, int w, int h, const uint8_t* data) {
	if (!data || w <= 0 || h <= 0) return;
	DrawSync(0);
	RECT r;
	// x,y,w sont en demi-mots VRAM (4 pixels 4bpp par demi-mot). Le blob
	// P1ZM stocke deja w dans cette unite, comme le champ RECT de LoadImage.
	setRECT(&r, x, y, w, h);
	vramDiagTrackPS1(x, y, w, h, g_frame + 1u);
	loadImageAligned(&r, data, w * h * 2);
}

void pfUpload16(int x, int y, int w, int h, const uint16_t* data) {
	if (!data || w <= 0 || h <= 0) return;
	DrawSync(0);
	RECT r;
	setRECT(&r, x, y, w, h);
	vramDiagTrackPS1(x, y, w, h, g_frame + 1u);
	loadImageAligned(&r, data, w * h * 2);
}

void pfDrawSprite16(int x, int y, int uVram, int vVram, int w, int h, int clutX, int clutY, int z) {
	if (w <= 0 || h <= 0) return;
	const int need = (int)sizeof(SPRT) + (int)sizeof(DR_TPAGE);
	if (g_next + need > g_prims[g_db] + kPrimBytes) {
		pfLog("PF_CAP primbuf overflow");
		return;
	}
	if (z < 0) z = 0;
	if (z >= kOtLength) z = kOtLength - 1;

	// uVram,vVram sont en demi-mots VRAM. getTPage arrondit a l'origine de
	// page (64 demi-mots en X, 256 lignes en Y) ; les UV du primitif sont en
	// pixels reels relatifs a cette page (4 pixels par demi-mot en 4bpp).
	const int pageX = uVram & ~63;
	const int pageY = vVram & ~255;
	const int u = (uVram - pageX) * 4;
	const int v = vVram - pageY;

	SPRT* s = reinterpret_cast<SPRT*>(g_next);
	setSprt(s);  // taille variable : contrairement a SPRT_16, SPRT a w et h
	setXY0(s, x, y);
	setWH(s, w, h);
	setUV0(s, u & 0xff, v & 0xff);
	s->clut = getClut(clutX, clutY);
	setRGB0(s, 128, 128, 128);  // 0x80 = modulation neutre pour une texture 4bpp
	addPrim(&g_ot[g_db][z], s);

	// addPrim() insere en tete : ajouter la page APRES le sprite la place avant
	// lui dans la chaine, donc le GPU applique la bonne texture page.
	DR_TPAGE* tp = reinterpret_cast<DR_TPAGE*>(g_next + sizeof(SPRT));
	setDrawTPage(tp, 0, 0, getTPage(0, 0, uVram, vVram));
	addPrim(&g_ot[g_db][z], tp);
	g_next += need;
}

static void drawRectImpl(int x, int y, int w, int h, uint32_t bgr555, int z, bool semi) {
	if (g_next + (int)sizeof(TILE) > g_prims[g_db] + kPrimBytes) {
		pfLog("PF_CAP primbuf overflow");
		return;
	}
	if (w <= 0 || h <= 0) return;

	TILE* tile = reinterpret_cast<TILE*>(g_next);
	// setTile() ecrit len=3 ET code=0x60 dans le mot d'en-tete. addPrim() ne
	// remplit que le champ addr : sans len, le GPU recoit un primitif de
	// longueur 0 et ne dessine rien (ecran de fond seul, observe en capture).
	setTile(tile);
	setSemiTrans(tile, semi);
	setRGB0(tile, bgr5_red(bgr555), bgr5_green(bgr555), bgr5_blue(bgr555));
	tile->x0 = (int16_t)x;
	tile->y0 = (int16_t)y;
	tile->w = (int16_t)w;
	tile->h = (int16_t)h;
	if (z < 0) z = 0;
	if (z >= kOtLength) z = kOtLength - 1;
	addPrim(&g_ot[g_db][z], tile);
	g_next += sizeof(TILE);
}

void pfDrawRect(int x, int y, int w, int h, uint32_t bgr555, int z) {
	drawRectImpl(x, y, w, h, bgr555, z, false);
}

void pfDrawRectSemi(int x, int y, int w, int h, uint32_t bgr555, int z) {
	drawRectImpl(x, y, w, h, bgr555, z, true);
}

void pfPresent(void) {
	DrawSync(0);
	VSync(0);

	// Temps d'image : periode entre deux retours de VSync(0), lue avec le
	// compteur racine 1. Sous OpenBIOS/PCSX-Redux, GetRCnt(RCntCNT1) rend
	// toujours 0 alors que le registre materiel 0x1f801110 avance : la sonde
	// lit donc directement le compteur. C'est un MMIO 16 bits ; une soustraction
	// sur uint16_t suffit. Aucun impact sur le rendu.
	{
		const uint16_t now = *(volatile uint16_t*)0x1f801110u;
		PS1FrameTiming* t = &g_ps1FrameTiming;
		if (t->lastTicks != 0u) {
			const uint16_t period = (uint16_t)(now - t->lastTicks);
			if (period != 0u) {
				t->samples[t->head] = period;
				t->head = (t->head + 1u) % PS1_FRAME_TIMING_SAMPLES;
				if (t->count < PS1_FRAME_TIMING_SAMPLES) ++t->count;
				t->ready = 1u;
			}
		}
		t->lastTicks = now;
	}

	// On affiche le buffer precedemment dessine pendant qu'on execute la liste
	// du buffer courant (double buffering, voir hellocpp::flip).
	PutDispEnv(&g_disp[g_db ^ 1]);
	DrawOTagEnv(&g_ot[g_db][kOtLength - 1], &g_draw[g_db]);

	// Lecture pad : btn est actif a l'etat bas sur PS1, on le convertit en masque
	// actif en haut pour respecter le contrat de pf.h.
	PADTYPE* pad = reinterpret_cast<PADTYPE*>(&g_pad_buf[0][0]);
	g_pad = (uint32_t)(~(uint32_t)pad->btn) & 0xffffu;

	g_db ^= 1;
	g_next = g_prims[g_db];
	ClearOTagR(g_ot[g_db], kOtLength);
	++g_frame;
	vramDiagStampPS1(g_frame);
	spuDiagStampPS1(g_frame);
}

uint32_t pfPad(void) { return g_pad; }

uint32_t pfFrame(void) { return g_frame; }

void pfLog(const char* line) {
	if (!line) return;
	// Le driver TTY du BIOS est un "dummy" par defaut : sans AddSIO(), puts()
	// n'atteint jamais la console (voir psxsio.h, AddSIO).
	static bool s_tty_ready = false;
	if (!s_tty_ready) {
		AddSIO(115200);
		s_tty_ready = true;
	}
	// puts() est la sortie TTY brute du libc PSn00bSDK (pas de stdout/fflush ici).
	puts(line);
}
