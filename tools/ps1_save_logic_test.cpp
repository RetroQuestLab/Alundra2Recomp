/*
 * Test hote de la sauvegarde PS1 (TDD).
 * Compile le VRAI header partage avec le build MIPS ; aucun mock, aucun SDK.
 *
 *   external\llvm-mingw-20261020-ucrt-x86_64\bin\clang++.exe -std=c++20 ^
 *       -o ps1\build\ps1_save_logic_test.exe tools\ps1_save_logic_test.cpp
 *   .\ps1\build\ps1_save_logic_test.exe
 */
#include "../src/ps1/save_ps1_logic.h"

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

QuestState sampleQuest() {
	QuestState q;
	questInit(&q);
	q.stage = QST_STAGE_TRONE;
	q.activeNpc = 7;
	q.dialogueId = 31;
	q.talkedPrunewell = 1;
	q.talkedGarde = 1;
	q.talkedFinrod = 1;
	q.talkedMarchand = 1;
	q.talkedAlexiaEpave = 1;
	q.talkedBelgarEpave = 1;
	q.talkedNatasha = 1;
	q.talkedJeehan = 1;
	q.hasStrangeKey = 1;
	q.bossDown = 1;
	q.chestLettre = 1;
	q.zone = QST_ZONE_TOLEDO;
	q.prevPad = PF_BTN_CROSS;
	q.gates = QST_GATE_TRONE_MASK;
	q.mephistoDown = 1;
	q.ending = 1;
	q.talkedMephistoArrivee = 1;
	q.talkedMephistoTrone = 1;
	q.talkedRoi = 1;
	q.talkedAlexiaFinale = 1;
	q.talkedAlexiaVaincu = 1;
	return q;
}

bool questEqual(const QuestState& a, const QuestState& b) {
	return a.stage == b.stage && a.activeNpc == b.activeNpc &&
	       a.dialogueId == b.dialogueId &&
	       a.talkedPrunewell == b.talkedPrunewell && a.talkedGarde == b.talkedGarde &&
	       a.talkedFinrod == b.talkedFinrod && a.talkedMarchand == b.talkedMarchand &&
	       a.talkedAlexiaEpave == b.talkedAlexiaEpave &&
	       a.talkedBelgarEpave == b.talkedBelgarEpave &&
	       a.talkedNatasha == b.talkedNatasha && a.talkedJeehan == b.talkedJeehan &&
	       a.hasStrangeKey == b.hasStrangeKey && a.bossDown == b.bossDown &&
	       a.chestLettre == b.chestLettre && a.zone == b.zone &&
	       a.prevPad == b.prevPad && a.gates == b.gates &&
	       a.mephistoDown == b.mephistoDown && a.ending == b.ending &&
	       a.talkedMephistoArrivee == b.talkedMephistoArrivee &&
	       a.talkedMephistoTrone == b.talkedMephistoTrone && a.talkedRoi == b.talkedRoi &&
	       a.talkedAlexiaFinale == b.talkedAlexiaFinale &&
	       a.talkedAlexiaVaincu == b.talkedAlexiaVaincu;
}
}  // namespace

int main() {
	CHECK(PS1_SAVE_SECTOR == 512);
	CHECK(PS1_SAVE_BIOS_SECTOR == 128);
	CHECK(PS1_SAVE_PAYLOAD_OFF == 12);
	CHECK(PS1_SAVE_CRC_OFF > PS1_SAVE_PAYLOAD_OFF + PS1_SAVE_PAYLOAD_SIZE);
	CHECK(PS1_SAVE_PAYLOAD_SIZE == 368);
	CHECK(PS1_SAVE_SLOT_SECTORS == 64);
	CHECK(PS1_SAVE_SLOT0_BLOC == 1);
	CHECK(PS1_SAVE_SLOT1_BLOC == 2);

	/* CRC32 IEEE de reference. */
	{
		const uint8_t s[] = {'1','2','3','4','5','6','7','8','9'};
		CHECK(ps1SaveCrc(s, 9) == 0xCBF43926u);
	}

	QuestState q = sampleQuest();
	uint8_t blob[PSI_SAVE_SIZE];
	for (int i = 0; i < PSI_SAVE_SIZE; ++i) blob[i] = static_cast<uint8_t>(i * 7 + 3);

	PS1SavePayload payload;
	ps1SavePayloadMake(&payload, &q, 0x00010020, 0x00020040, 3, blob, PSI_SAVE_SIZE);
	CHECK(questEqual(payload.quest, q));
	CHECK(payload.heroX == 0x00010020);
	CHECK(payload.heroY == 0x00020040);
	CHECK(payload.heroDir == 3);
	CHECK(payload.invSize == PSI_SAVE_SIZE);
	CHECK(payload.inv[PSI_SAVE_SIZE - 1] == static_cast<uint8_t>((PSI_SAVE_SIZE - 1) * 7 + 3));

	PS1SaveBlock a, b;
	ps1SaveBlockEncode(&a, &payload, 17);
	ps1SaveBlockEncode(&b, &payload, 18);
	CHECK(ps1SaveGet32(a.bytes) == PS1_SAVE_MAGIC);
	CHECK(ps1SaveGet16(a.bytes + 4) == PS1_SAVE_VERSION);
	CHECK(ps1SaveGet16(a.bytes + 6) == 17);
	CHECK(ps1SaveGet32(a.bytes + PS1_SAVE_CRC_OFF) ==
	      ps1SaveCrc(a.bytes, PS1_SAVE_CRC_OFF));

	PS1SavePayload out;
	uint16_t seq = 0;
	CHECK(ps1SaveBlockDecode(&out, &seq, &b) == 1);
	CHECK(seq == 18);
	CHECK(questEqual(out.quest, q));
	CHECK(out.heroX == payload.heroX);
	CHECK(out.heroY == payload.heroY);
	CHECK(out.heroDir == payload.heroDir);
	CHECK(out.invSize == PSI_SAVE_SIZE);
	int same = 1;
	for (int i = 0; i < PSI_SAVE_SIZE; ++i) if (out.inv[i] != blob[i]) same = 0;
	CHECK(same == 1);

	/* Corruptions refusees avant mutation. */
	{
		PS1SaveBlock bad = b;
		bad.bytes[PS1_SAVE_PAYLOAD_OFF + 40] ^= 0x01u;
		CHECK(ps1SaveBlockDecode(&out, &seq, &bad) == 0);
		bad = b;
		bad.bytes[PS1_SAVE_CRC_OFF] ^= 0x01u;
		CHECK(ps1SaveBlockDecode(&out, &seq, &bad) == 0);
		bad = b;
		bad.bytes[0] ^= 0x01u;
		CHECK(ps1SaveBlockDecode(&out, &seq, &bad) == 0);
		bad = b;
		ps1SavePut16(bad.bytes + 4, PS1_SAVE_VERSION + 1);
		ps1SavePut32(bad.bytes + PS1_SAVE_CRC_OFF,
		             ps1SaveCrc(bad.bytes, PS1_SAVE_CRC_OFF));
		CHECK(ps1SaveBlockDecode(&out, &seq, &bad) == 0);
		bad = b;
		ps1SavePut32(bad.bytes + PS1_SAVE_PAYLOAD_OFF + 96 + 12, PSI_SAVE_SIZE + 1);
		ps1SavePut32(bad.bytes + PS1_SAVE_CRC_OFF,
		             ps1SaveCrc(bad.bytes, PS1_SAVE_CRC_OFF));
		CHECK(ps1SaveBlockDecode(&out, &seq, &bad) == 0);
	}

	/* Choix du slot : le plus recent gagne, un slot corrompu est ignore. */
	CHECK(ps1SaveChoose(&a, &b) == 1);
	CHECK(ps1SaveChoose(&b, &a) == 0);
	{
		PS1SaveBlock bad = a;
		bad.bytes[30] ^= 0x80u;
		CHECK(ps1SaveChoose(&bad, &b) == 1);
		CHECK(ps1SaveChoose(&a, &bad) == 0);
		PS1SaveBlock empty;
		for (int i = 0; i < PS1_SAVE_SECTOR; ++i) empty.bytes[i] = 0;
		CHECK(ps1SaveChoose(&empty, &bad) == -1);
		CHECK(ps1SaveChoose(&a, &empty) == 0);
	}
	CHECK(ps1SaveWriteSlot(0) == 1);
	CHECK(ps1SaveWriteSlot(1) == 0);
	CHECK(ps1SaveWriteSlot(-1) == 0);
	CHECK(ps1SaveNextSeq(0) == 1);
	CHECK(ps1SaveNextSeq(65534) == 65535);
	CHECK(ps1SaveNextSeq(65535) == 1);

	/* Bornes du blob inventaire. */
	{
		PS1SavePayload small;
		ps1SavePayloadMake(&small, &q, 0, 0, 0, blob, 4);
		CHECK(small.invSize == 4);
		CHECK(small.inv[4] == 0);
		ps1SavePayloadMake(&small, &q, 0, 0, 0, blob, PSI_SAVE_SIZE + 100);
		CHECK(small.invSize == PSI_SAVE_SIZE);
		ps1SavePayloadMake(&small, &q, 0, 0, 0, blob, -5);
		CHECK(small.invSize == 0);
	}

	std::printf("PS1_SAVE_LOGIC %d/%d\n", g_checks - g_fail, g_checks);
	return g_fail == 0 ? 0 : 1;
}
