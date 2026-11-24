#pragma once
/*
 * Sauvegarde PS1 : format de bloc POD, CRC32 et deux slots redondants.
 *
 * Ce header est compile deux fois : par le build MIPS (via save_ps1.cpp) et par
 * le test hote tools/ps1_save_logic_test.cpp. Aucun STL, aucune allocation,
 * aucun appel BIOS : le pilote de carte memoire vit dans save_ps1.cpp.
 *
 * Format d'un slot : 512 octets (4 secteurs BIOS de 128 octets), ecrits dans
 * un bloc de 8 Ko (64 secteurs BIOS). Les deux slots occupent les blocs 1 et 2
 * (secteurs BIOS 64 et 128), apres le bloc 0 Reserve au repertoire. Le slot
 * valide avec le plus grand numero de sequence gagne ; ecrire dans l'autre
 * slot conserve toujours une copie saine en cas de coupure.
 *
 * QuestState est serialise CHAMP PAR CHAMP : l'ordre des gates et des champs
 * du trone est contraint par les harnais Lua, aucun memcpy brut de la struct.
 */
#include <stdint.h>

#include "inventory_ps1_logic.h"
#include "quest_ps1_logic.h"

#define PS1_SAVE_MAGIC        0x32415350u  /* "PSA2" little-endian */
#define PS1_SAVE_VERSION      4            /* aligne sur SAVE_VERSION PC */
#define PS1_SAVE_SECTOR       512          /* 4 secteurs BIOS de 128 octets */
#define PS1_SAVE_BIOS_SECTOR  128
#define PS1_SAVE_SLOTS        2
#define PS1_SAVE_SLOT_SECTORS 64           /* 1 bloc BIOS = 8 Ko = 64 secteurs */
#define PS1_SAVE_SLOT0_BLOC   1            /* secteur BIOS 64, apres le repertoire */
#define PS1_SAVE_SLOT1_BLOC   2            /* secteur BIOS 128 */
#define PS1_SAVE_CRC_OFF      508
#define PS1_SAVE_PAYLOAD_OFF  12
#define PS1_SAVE_QUEST_FIELDS 24
#define PS1_SAVE_PAYLOAD_SIZE (PS1_SAVE_QUEST_FIELDS * 4 + 16 + PSI_SAVE_SIZE)

/* Etat minimal restaure : quete, position heros, inventaire A2. */
struct PS1SavePayload {
	QuestState quest;
	int32_t heroX;      /* fixed-point 16.16 */
	int32_t heroY;
	int32_t heroDir;
	uint32_t invSize;
	uint8_t inv[PSI_SAVE_SIZE];
};

struct PS1SaveBlock {
	uint8_t bytes[PS1_SAVE_SECTOR];
};

/* CRC32 IEEE (poly 0xEDB88320), sans table statique : compatible MIPS. */
inline uint32_t ps1SaveCrc(const uint8_t* p, int n) {
	uint32_t crc = 0xFFFFFFFFu;
	for (int i = 0; i < n; ++i) {
		crc ^= p[i];
		for (int k = 0; k < 8; ++k)
			crc = (crc & 1u) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
	}
	return ~crc;
}

inline void ps1SavePut16(uint8_t* p, uint16_t v) {
	p[0] = static_cast<uint8_t>(v & 0xFFu);
	p[1] = static_cast<uint8_t>(v >> 8);
}

inline void ps1SavePut32(uint8_t* p, uint32_t v) {
	p[0] = static_cast<uint8_t>(v & 0xFFu);
	p[1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
	p[2] = static_cast<uint8_t>((v >> 16) & 0xFFu);
	p[3] = static_cast<uint8_t>(v >> 24);
}

inline uint16_t ps1SaveGet16(const uint8_t* p) {
	return static_cast<uint16_t>(p[0] | (uint16_t(p[1]) << 8));
}

inline uint32_t ps1SaveGet32(const uint8_t* p) {
	return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
	       (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

/* Ecriture explicite des 24 entiers de QuestState, dans l'ordre figé. */
inline int ps1SaveQuestWrite(uint8_t* p, const QuestState* q) {
	const uint32_t v[PS1_SAVE_QUEST_FIELDS] = {
		static_cast<uint32_t>(q->stage), static_cast<uint32_t>(q->activeNpc),
		static_cast<uint32_t>(q->dialogueId), static_cast<uint32_t>(q->talkedPrunewell),
		static_cast<uint32_t>(q->talkedGarde), static_cast<uint32_t>(q->talkedFinrod),
		static_cast<uint32_t>(q->talkedMarchand),
		static_cast<uint32_t>(q->talkedAlexiaEpave),
		static_cast<uint32_t>(q->talkedBelgarEpave),
		static_cast<uint32_t>(q->talkedNatasha), static_cast<uint32_t>(q->talkedJeehan),
		static_cast<uint32_t>(q->hasStrangeKey), static_cast<uint32_t>(q->bossDown),
		static_cast<uint32_t>(q->chestLettre), static_cast<uint32_t>(q->zone),
		static_cast<uint32_t>(q->prevPad), q->gates,
		static_cast<uint32_t>(q->mephistoDown), static_cast<uint32_t>(q->ending),
		static_cast<uint32_t>(q->talkedMephistoArrivee),
		static_cast<uint32_t>(q->talkedMephistoTrone),
		static_cast<uint32_t>(q->talkedRoi),
		static_cast<uint32_t>(q->talkedAlexiaFinale),
		static_cast<uint32_t>(q->talkedAlexiaVaincu)
	};
	for (int i = 0; i < PS1_SAVE_QUEST_FIELDS; ++i)
		ps1SavePut32(p + i * 4, v[i]);
	return PS1_SAVE_QUEST_FIELDS * 4;
}

/* Lecture explicite : aucun offsets de struct n'est suppose. */
inline int ps1SaveQuestRead(QuestState* q, const uint8_t* p) {
	q->stage = static_cast<int>(ps1SaveGet32(p));
	q->activeNpc = static_cast<int>(ps1SaveGet32(p + 4));
	q->dialogueId = static_cast<int>(ps1SaveGet32(p + 8));
	q->talkedPrunewell = static_cast<int>(ps1SaveGet32(p + 12));
	q->talkedGarde = static_cast<int>(ps1SaveGet32(p + 16));
	q->talkedFinrod = static_cast<int>(ps1SaveGet32(p + 20));
	q->talkedMarchand = static_cast<int>(ps1SaveGet32(p + 24));
	q->talkedAlexiaEpave = static_cast<int>(ps1SaveGet32(p + 28));
	q->talkedBelgarEpave = static_cast<int>(ps1SaveGet32(p + 32));
	q->talkedNatasha = static_cast<int>(ps1SaveGet32(p + 36));
	q->talkedJeehan = static_cast<int>(ps1SaveGet32(p + 40));
	q->hasStrangeKey = static_cast<int>(ps1SaveGet32(p + 44));
	q->bossDown = static_cast<int>(ps1SaveGet32(p + 48));
	q->chestLettre = static_cast<int>(ps1SaveGet32(p + 52));
	q->zone = static_cast<int>(ps1SaveGet32(p + 56));
	q->prevPad = static_cast<int>(ps1SaveGet32(p + 60));
	q->gates = ps1SaveGet32(p + 64);
	q->mephistoDown = static_cast<int>(ps1SaveGet32(p + 68));
	q->ending = static_cast<int>(ps1SaveGet32(p + 72));
	q->talkedMephistoArrivee = static_cast<int>(ps1SaveGet32(p + 76));
	q->talkedMephistoTrone = static_cast<int>(ps1SaveGet32(p + 80));
	q->talkedRoi = static_cast<int>(ps1SaveGet32(p + 84));
	q->talkedAlexiaFinale = static_cast<int>(ps1SaveGet32(p + 88));
	q->talkedAlexiaVaincu = static_cast<int>(ps1SaveGet32(p + 92));
	return PS1_SAVE_QUEST_FIELDS * 4;
}

/* Remplit un payload depuis l'etat courant. Le blob inventaire est deja
 * serialize par psiSave() (PSI_SAVE_SIZE octets). */
inline void ps1SavePayloadMake(PS1SavePayload* out, const QuestState* quest,
                               int heroX, int heroY, int heroDir,
                               const uint8_t* invBlob, int invSize) {
	out->quest = *quest;
	out->heroX = static_cast<int32_t>(heroX);
	out->heroY = static_cast<int32_t>(heroY);
	out->heroDir = static_cast<int32_t>(heroDir);
	const int n = invSize < PSI_SAVE_SIZE ? invSize : PSI_SAVE_SIZE;
	out->invSize = static_cast<uint32_t>(n < 0 ? 0 : n);
	for (int i = 0; i < n; ++i) out->inv[i] = invBlob[i];
	for (int i = n; i < PSI_SAVE_SIZE; ++i) out->inv[i] = 0;
}

/* Encode un bloc autosuffisant : en-tete, payload, zero padding, CRC. */
inline void ps1SaveBlockEncode(PS1SaveBlock* out, const PS1SavePayload* in,
                               uint16_t seq) {
	for (int i = 0; i < PS1_SAVE_SECTOR; ++i) out->bytes[i] = 0;
	ps1SavePut32(out->bytes, PS1_SAVE_MAGIC);
	ps1SavePut16(out->bytes + 4, PS1_SAVE_VERSION);
	ps1SavePut16(out->bytes + 6, seq);
	ps1SavePut16(out->bytes + 8, PS1_SAVE_PAYLOAD_SIZE);

	uint8_t* p = out->bytes + PS1_SAVE_PAYLOAD_OFF;
	p += ps1SaveQuestWrite(p, &in->quest);
	ps1SavePut32(p, static_cast<uint32_t>(in->heroX)); p += 4;
	ps1SavePut32(p, static_cast<uint32_t>(in->heroY)); p += 4;
	ps1SavePut32(p, static_cast<uint32_t>(in->heroDir)); p += 4;
	ps1SavePut32(p, in->invSize); p += 4;
	for (int i = 0; i < PSI_SAVE_SIZE; ++i) p[i] = in->inv[i];
	ps1SavePut32(out->bytes + PS1_SAVE_CRC_OFF,
	             ps1SaveCrc(out->bytes, PS1_SAVE_CRC_OFF));
}

/* Valide magic, version, taille et CRC AVANT toute copie dans l'etat courant. */
inline int ps1SaveBlockDecode(PS1SavePayload* out, uint16_t* seq,
                              const PS1SaveBlock* in) {
	if (ps1SaveGet32(in->bytes) != PS1_SAVE_MAGIC) return 0;
	if (ps1SaveGet16(in->bytes + 4) != PS1_SAVE_VERSION) return 0;
	if (ps1SaveGet16(in->bytes + 8) != PS1_SAVE_PAYLOAD_SIZE) return 0;
	if (ps1SaveGet32(in->bytes + PS1_SAVE_CRC_OFF) !=
	    ps1SaveCrc(in->bytes, PS1_SAVE_CRC_OFF)) return 0;

	const uint8_t* p = in->bytes + PS1_SAVE_PAYLOAD_OFF;
	p += ps1SaveQuestRead(&out->quest, p);
	out->heroX = static_cast<int32_t>(ps1SaveGet32(p)); p += 4;
	out->heroY = static_cast<int32_t>(ps1SaveGet32(p)); p += 4;
	out->heroDir = static_cast<int32_t>(ps1SaveGet32(p)); p += 4;
	out->invSize = ps1SaveGet32(p); p += 4;
	if (out->invSize > PSI_SAVE_SIZE) return 0;
	for (int i = 0; i < static_cast<int>(out->invSize); ++i) out->inv[i] = p[i];
	for (int i = static_cast<int>(out->invSize); i < PSI_SAVE_SIZE; ++i) out->inv[i] = 0;
	if (seq) *seq = ps1SaveGet16(in->bytes + 6);
	return 1;
}

/* Retourne l'index du slot le plus recent (0/1), ou -1 si aucun n'est valide.
 * En cas d'egalite, le slot 0 gagne : l'ecriture alternee assure qu'un slot
 * ecrit a la meme sequence est une copie identique. */
inline int ps1SaveChoose(const PS1SaveBlock* a, const PS1SaveBlock* b) {
	PS1SavePayload pa, pb;
	uint16_t sa = 0, sb = 0;
	const int okA = ps1SaveBlockDecode(&pa, &sa, a);
	const int okB = ps1SaveBlockDecode(&pb, &sb, b);
	if (okA && okB) return sb > sa ? 1 : 0;
	if (okA) return 0;
	if (okB) return 1;
	return -1;
}

/* Slot ou ecrire : toujours l'autre que le meilleur, pour preserver une copie. */
inline int ps1SaveWriteSlot(int best) {
	return best == 0 ? 1 : 0;
}

/* Sequence suivante, avec repli a 1 apres 65535. */
inline uint16_t ps1SaveNextSeq(uint16_t current) {
	return current == 0xFFFFu ? 1u : static_cast<uint16_t>(current + 1u);
}
