/*
 * Pilote de sauvegarde PS1 natif.
 *
 * Acces secteur brut via le BIOS (128 octets), sans systeme de fichiers :
 * aucun formatage automatique, aucune ecriture dans le bloc 0. Deux blocs de
 * 8 Ko contiennent chacun un bloc POD de 512 octets avec CRC32 et numero de
 * sequence. La derniere ecriture est confirmee par relecture.
 *
 * Le BIOS carte est asynchrone et partage SIO0 avec la manette : toutes les
 * commandes sont emises par savePumpPS1(), une par image, sans spin ni VSync()
 * supplementaire. Cela preserve la manette et les harnais de capture.
 */
#include "save_ps1.h"

#include "hero_ps1.h"
#include "interaction_ps1.h"
#include "menu_inventory_ps1.h"

#include <psxapi.h>
#include <stdio.h>

namespace {

const int kCardChan = 0x00;       /* _card_info : port 1 x 16 + carte 0 */
const int kCardPort = 0x00;       /* _card_read/_card_write : meme canon */
const int kCardSlot = 0;          /* _card_status : port/10h = 0x00/0x10 */
const int kCardStatusReady = 0x01;
const int kCardSectorCount = 1024;

enum CardState {
	kIdle = 0,
	kInitProbeInfo,
	kInitInfoWait,
	kInitReadSlot0,
	kInitReadSlot1,
	kInitDone,
	kWriteNewCard,
	kWriteSector,
	kWriteInfo,
	kVerifySector,
	kWriteDone,
	kWriteReset  // relance apres abandon : _card_info puis ecriture complete
};

int g_ready = 0;
int g_early = 0;
int g_hasGame = 0;
int g_best = -1;
uint16_t g_seq = 0;
PS1SaveBlock g_slots[PS1_SAVE_SLOTS];

CardState g_state = kIdle;
int g_pendingWrite = 0;  // ecriture demandee pendant une operation en cours
int g_writeRetries = 0;  // relances apres abandon BIOS de l'ecriture courante
int g_pzone = 0, g_px = 0, g_py = 0, g_pdir = 0;
int g_wait = 0;
int g_sub = 0;
int g_targetSlot = 0;
uint16_t g_pendingSeq = 0;
PS1SaveBlock g_pendingBlock;
PS1SaveBlock g_verifyBlock;

int slotSector(int slot, int sub) {
	const int bloc = slot == 0 ? PS1_SAVE_SLOT0_BLOC : PS1_SAVE_SLOT1_BLOC;
	return bloc * PS1_SAVE_SLOT_SECTORS + sub;
}

/* Retourne 1 lorsqu'une commande peut etre emise. Le BIOS ne traite qu'un
 * acces par image : l'etat en cours reste en attente jusqu'au VBlank suivant. */
static int g_waitFrames = 0;
static int g_readyFrames = 0;

int cardIdle(void) {
	if (!g_wait) return 1;
	const int st = _card_status(kCardSlot);
	++g_waitFrames;
	/* OpenBIOS peut annoncer "pret" des que l'octet de statut est pose, alors
	 * que la machine a etat interne SIO0 n'a pas encore relache l'operation.
	 * Exiger quelques images stables evite les echecs _card_read suivants. */
	if (st == kCardStatusReady) {
		++g_readyFrames;
		if (g_readyFrames >= 3) {
			g_wait = 0;
			g_waitFrames = 0;
			g_readyFrames = 0;
			/* Operation terminee : rendre le VBlank au driver PAD/VSync. */
			ChangeClearPAD(0);
			return 1;
		}
		return 0;
	}
	g_readyFrames = 0;
	// Un statut d'erreur lu juste apres l'emission peut etre celui de l'operation
	// precedente : on ne conclut a l'abandon qu'apres quelques images (v0.8.37).
	if (((st == 0x11 || st == 0x21) && g_waitFrames > 3) || g_waitFrames > 600) {
		volatile uint32_t* istat = reinterpret_cast<volatile uint32_t*>(0x1f801070u);
		volatile uint32_t* imask = reinterpret_cast<volatile uint32_t*>(0x1f801074u);
		volatile uint16_t* sstat = reinterpret_cast<volatile uint16_t*>(0x1f801044u);
		volatile uint16_t* sctrl = reinterpret_cast<volatile uint16_t*>(0x1f80104au);
		volatile uint16_t* smode = reinterpret_cast<volatile uint16_t*>(0x1f801048u);
		volatile uint16_t* sbaud = reinterpret_cast<volatile uint16_t*>(0x1f80104eu);
		printf("PS1_CARD_WAIT FAIL frames=%d st=%x state=%d istat=%x imask=%x sstat=%x sctrl=%x smode=%x sbaud=%x\n",
		       g_waitFrames, st, static_cast<int>(g_state),
		       static_cast<unsigned>(*istat), static_cast<unsigned>(*imask),
		       static_cast<unsigned>(*sstat), static_cast<unsigned>(*sctrl),
		       static_cast<unsigned>(*smode), static_cast<unsigned>(*sbaud));
		g_wait = 0;
		g_waitFrames = 0;
		ChangeClearPAD(0);
		// Abandon BIOS (0x11) pendant une ecriture : la sequence complete est
		// relancee (au plus 3 fois) au lieu de perdre la sauvegarde (v0.8.37).
		if (g_state >= kWriteNewCard && g_state <= kWriteReset && g_writeRetries < 3) {
			++g_writeRetries;
			printf("PS1_SAVE_RETRY %d\n", g_writeRetries);
			g_sub = 0;
			g_state = kWriteReset;  // _card_info d'abord, comme l'initialisation
		} else {
			g_state = kIdle;
		}
	}
	return 0;
}

/* Le verificateur d'interruption du BIOS (sio0Verifier) teste IREG.VBLANK.
 * Avec ChangeClearPAD(0), ce bit n'est jamais acquitte : chaque IRQ SIO0
 * declenche alors aussi firstStageCardAction(), qui abort l'operation en cours
 * et pose le statut 0x11. On active l'auto-ack VBlank uniquement pendant la
 * fenetre ou une commande carte est en vol. Le halt VSync (pf_ps1.cpp) surveille
 * _card_status() et desactive l'auto-ack des la fin de l'operation ; cardIdle()
 * conserve sa propre verification pour la machine a etat. */
void armCardOp(void) { ChangeClearPAD(1); }

void finishInit(void) {
	g_best = ps1SaveChoose(&g_slots[0], &g_slots[1]);
	PS1SavePayload probe;
	uint16_t seq = 0;
	if (g_best >= 0 && ps1SaveBlockDecode(&probe, &seq, &g_slots[g_best]))
		g_seq = seq;
	else
		g_seq = 0;
	g_hasGame = g_best >= 0 ? 1 : 0;
	g_ready = 1;
	g_state = kIdle;
	printf("PS1_SAVE_INIT READY hasGame=%d seq=%u\n", g_hasGame,
	       static_cast<unsigned>(g_seq));
}

void finishWrite(void) {
	PS1SavePayload decoded;
	uint16_t decodedSeq = 0;
	if (ps1SaveBlockDecode(&decoded, &decodedSeq, &g_verifyBlock) &&
	    decodedSeq == g_pendingSeq) {
		for (int i = 0; i < PS1_SAVE_SECTOR; ++i)
			g_slots[g_targetSlot].bytes[i] = g_verifyBlock.bytes[i];
		g_best = g_targetSlot;
		g_seq = g_pendingSeq;
		g_hasGame = 1;
		printf("PS1_SAVE_WRITE OK seq=%u slot=%d\n",
		       static_cast<unsigned>(g_seq), g_targetSlot);
	} else {
		printf("PS1_SAVE_WRITE FAIL verify\n");
	}
	g_state = kIdle;
}

}  // namespace

static int startWrite(int zone, int hx, int hy, int dir);

int saveCardOpPendingPS1(void) { return g_wait; }

int saveCardStatusPS1(void) { return _card_status(kCardSlot); }

/* Evenements BIOS carte : la sequence officielle Psy-Q exige OpenEvent()
 * avant InitCARD, puis EnableEvent() apres _bu_init(). Sans ces evenements,
 * le BIOS ne peut pas signaler la fin des operations asynchrones et la
 * machine a etat carte reste en attente (status 0x11). */
int g_evCardIO = -1;
int g_evCardTO = -1;
int g_evCardERR = -1;
int g_evCardNEW = -1;

void saveEarlyInitPS1(void) {
	if (g_early) return;
	/* pfInit() a deja appele InitPAD()/StartPAD()/ChangeClearPAD(0).
	 * La sequence officielle (Psy-Q "Old Way") est :
	 *   OpenEvent -> InitCARD -> StartCARD -> _bu_init -> EnableEvent
	 * StartCARD() appelle ChangeClearPAD(1) en interne : on doit remettre 0
	 * juste apres, sinon VSync() tourne en timeout et le handler carte ne
	 * recoit plus ses interruptions. */
	EnterCriticalSection();
	g_evCardIO = OpenEvent(SwCARD, EvSpIOE, 0x2000, 0);
	g_evCardTO = OpenEvent(SwCARD, EvSpTIMOUT, 0x2000, 0);
	g_evCardERR = OpenEvent(SwCARD, EvSpERROR, 0x2000, 0);
	g_evCardNEW = OpenEvent(SwCARD, EvSpNEW, 0x2000, 0);
	ExitCriticalSection();
	InitCARD(1);
	StartCARD();
	ChangeClearPAD(0);  // annule le ChangeClearPAD(1) interne de StartCARD()
	_bu_init();

	EnableEvent(g_evCardIO);
	EnableEvent(g_evCardTO);
	EnableEvent(g_evCardERR);
	EnableEvent(g_evCardNEW);
	g_early = 1;
}

int saveInitPS1(void) {
	if (g_ready) return 1;
	if (!g_early) return 0;
	if (g_state == kIdle) {
		g_sub = 0;
		g_state = kInitProbeInfo;
	}
	return 0;
}

void savePumpPS1(void) {
	if (!g_early || !cardIdle()) return;

	switch (g_state) {
	case kIdle:
		if (g_pendingWrite && g_ready) startWrite(g_pzone, g_px, g_py, g_pdir);
		return;

	case kInitProbeInfo: {
		_new_card();
		armCardOp();
		_card_info(kCardChan);
		g_wait = 1;
		g_state = kInitInfoWait;
		return;
	}

	case kInitInfoWait:
		/* Le statut est deja passe par cardIdle() : on enchaine les lectures. */
		g_state = kInitReadSlot0;
		return;

	case kInitReadSlot0:
	case kInitReadSlot1: {
		const int slot = g_state == kInitReadSlot0 ? 0 : 1;
		const int sector = slotSector(slot, g_sub);
		_new_card();  /* sans _bu_init(), le drapeau "carte changee" reste pose */
		armCardOp();
		const int rd = _card_read(kCardPort, sector,
		                          g_slots[slot].bytes + g_sub * 128);
		if (rd != 1) {
			printf("PS1_SAVE_FAIL init_read slot=%d sub=%d\n", slot, g_sub);
			g_state = kIdle;
			return;
		}
		g_wait = 1;
		++g_sub;
		if (g_sub >= PS1_SAVE_SECTOR / PS1_SAVE_BIOS_SECTOR) {
			g_sub = 0;
			g_state = slot == 0 ? kInitReadSlot1 : kInitDone;
		}
		return;
	}

	case kInitDone:
		finishInit();
		return;

	case kWriteNewCard:
		/* Le BIOS refuse les ecritures tant que le drapeau "carte changee"
		 * est pose. _new_card() le neutralise sans formatter la carte. */
		_new_card();
		g_wait = 1;
		g_state = kWriteSector;
		return;

	case kWriteSector: {
		_new_card();  /* le drapeau "carte changee" bloque chaque ecriture */
		armCardOp();
		const int wr = _card_write(kCardPort, slotSector(g_targetSlot, g_sub),
		                g_pendingBlock.bytes + g_sub * 128);
		if (wr != 1) {
			printf("PS1_SAVE_FAIL write sub=%d\n", g_sub);
			g_state = kIdle;
			return;
		}
		g_wait = 1;
		++g_sub;
		if (g_sub >= PS1_SAVE_SECTOR / PS1_SAVE_BIOS_SECTOR) {
			g_sub = 0;
			g_state = kWriteInfo;
		}
		return;
	}

	case kWriteInfo: {
		/* Le BIOS ne signale les erreurs de la derniere ecriture qu'a
		 * l'operation suivante : _card_info sert de synchronisation. */
		armCardOp();
		const int info = _card_info(kCardChan);
		if (info != 1) {
			printf("PS1_SAVE_FAIL info\n");
			g_state = kIdle;
			return;
		}
		g_wait = 1;
		g_state = kVerifySector;
		return;
	}

	case kVerifySector: {
		_new_card();
		armCardOp();
		const int rd = _card_read(kCardPort, slotSector(g_targetSlot, g_sub),
		               g_verifyBlock.bytes + g_sub * 128);
		if (rd != 1) {
			printf("PS1_SAVE_FAIL verify_read sub=%d\n", g_sub);
			g_state = kIdle;
			return;
		}
		g_wait = 1;
		++g_sub;
		if (g_sub >= PS1_SAVE_SECTOR / PS1_SAVE_BIOS_SECTOR) {
			g_sub = 0;
			g_state = kWriteDone;
		}
		return;
	}

	case kWriteDone:
		finishWrite();
		return;

	case kWriteReset: {
		_new_card();
		armCardOp();
		if (_card_info(kCardChan) != 1) { g_state = kIdle; return; }
		g_wait = 1;
		g_state = kWriteNewCard;
		return;
	}
	}
}

int saveHasGamePS1(void) { return g_ready && g_hasGame; }

uint16_t saveSequencePS1(void) { return g_ready ? g_seq : 0; }

/* Une ecriture demandee pendant qu'une autre est en cours n'est plus perdue :
 * elle est memorisee (la plus recente remplace la precedente) et lancee par
 * savePumpPS1() des que la machine carte est au repos (v0.8.35). L'etat de
 * quete est relu au lancement : c'est l'etat le plus recent qui est ecrit. */
int saveWriteGamePS1(int zone, int hx, int hy, int dir) {
	if (!g_ready || g_state != kIdle) {
		g_pendingWrite = 1;
		g_pzone = zone; g_px = hx; g_py = hy; g_pdir = dir;
		return 1;
	}
	return startWrite(zone, hx, hy, dir);
}

static int startWrite(int zone, int hx, int hy, int dir) {
	g_pendingWrite = 0;
	g_writeRetries = 0;

	PS1SavePayload payload;
	QuestState quest;
	interactionQuestPS1(&quest);
	quest.zone = zone;
	quest.prevPad = 0;

	uint8_t invBlob[PSI_SAVE_SIZE];
	if (inventorySavePS1(invBlob) != 1) return 0;
	ps1SavePayloadMake(&payload, &quest, hx, hy, dir, invBlob, PSI_SAVE_SIZE);

	const int target = ps1SaveWriteSlot(g_best);
	/* Ne jamais ecraser les donnees d'un autre jeu : le bloc cible doit etre
	 * vierge (0x00/0xFF) ou deja etre une sauvegarde Alundra 2 valide. */
	int blank = 1;
	for (int i = 0; i < PS1_SAVE_SECTOR; ++i) {
		const uint8_t v = g_slots[target].bytes[i];
		if (v != 0x00u && v != 0xFFu) { blank = 0; break; }
	}
	PS1SavePayload probe;
	if (!blank && !ps1SaveBlockDecode(&probe, 0, &g_slots[target])) {
		printf("PS1_SAVE_FAIL foreign_block slot=%d\n", target);
		return 0;
	}

	g_targetSlot = target;
	g_pendingSeq = g_best < 0 ? 1 : ps1SaveNextSeq(g_seq);
	ps1SaveBlockEncode(&g_pendingBlock, &payload, g_pendingSeq);
	g_sub = 0;
	g_state = kWriteNewCard;
	return 1;
}

int saveReadGamePS1(int* zone, int* hx, int* hy, int* dir) {
	if (!g_ready || g_best < 0) return 0;
	PS1SavePayload payload;
	uint16_t seq = 0;
	if (!ps1SaveBlockDecode(&payload, &seq, &g_slots[g_best])) return 0;

	QuestState quest = payload.quest;
	quest.prevPad = 0;  /* aucun front de manette herite d'une session precedente */
	interactionSetQuestPS1(&quest);
	if (inventoryLoadPS1(payload.inv) != 1) return 0;
	heroSetPos(static_cast<int>(payload.heroX), static_cast<int>(payload.heroY));
	heroSetDirPS1(static_cast<int>(payload.heroDir));
	interactionSetZonePS1(quest.zone);

	if (zone) *zone = quest.zone;
	if (hx) *hx = static_cast<int>(payload.heroX);
	if (hy) *hy = static_cast<int>(payload.heroY);
	if (dir) *dir = static_cast<int>(payload.heroDir);
	g_seq = seq;
	printf("PS1_SAVE_READ OK seq=%u zone=%d x=%d y=%d\n",
	       static_cast<unsigned>(seq), quest.zone,
	       static_cast<int>(payload.heroX), static_cast<int>(payload.heroY));
	return 1;
}
