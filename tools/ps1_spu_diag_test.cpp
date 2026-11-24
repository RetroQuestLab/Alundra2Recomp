/*
 * Test hote de la table d'occupation SPU PS1 (TDD).
 * Compile les vrais en-tetes partages avec le build MIPS.
 * Compilation : clang++ -std=c++20 -o ps1\build\ps1_spu_diag_test.exe tools\ps1_spu_diag_test.cpp
 */
#include "../src/ps1/spu_diag_ps1_logic.h"

#include <cstdint>
#include <cstdio>

namespace {
int gFail = 0;
int gChecks = 0;
#define CHECK(cond)                                                     \
	do {                                                                  \
		++gChecks;                                                         \
		if (!(cond)) {                                                      \
			std::printf("FAIL ligne %d : %s\n", __LINE__, #cond);            \
			++gFail;                                                       \
		}                                                                   \
	} while (0)

constexpr uint32_t kBankAddr = 0x1010u;
constexpr uint32_t kBankBytes = 378304u;
constexpr uint32_t kChunk0Addr = 0x5D600u;
constexpr uint32_t kChunk1Addr = 0x5F600u;
constexpr uint32_t kChunkBytes = 8192u;

void testInit() {
	PS1SpuDiag s{};
	spuDiagInit(s);
	CHECK(s.magic == 0xA2535044u);
	CHECK(s.ready == 1u);
	CHECK(s.spuBytes == 524288u);
	CHECK(s.reservedBytes == 4096u);
	CHECK(s.currentBytes == 0u);
	CHECK(s.peakBytes == 0u);
	CHECK(s.transfers == 0u);
	CHECK(s.overlapBytes == 0u);
	CHECK(s.rejections == 0u);
	CHECK(s.bankBytes == 0u);
	CHECK(s.streamBytes == 0u);
	CHECK(s.integrity == (0xA2535044u ^ 0u));
	CHECK(spuDiagValidate(s) == 0u);  // aucun transfert : pas encore de mesure
}

void testBankThenStream() {
	uint8_t bits[kSpuDiagBitsetBytes] = {0};
	PS1SpuDiag s{};
	spuDiagInit(s);

	CHECK(spuDiagTrackRange(s, bits, kBankAddr, kBankBytes, 1u) == 1u);
	CHECK(s.currentBytes == 378304u);
	CHECK(s.peakBytes == 378304u);
	CHECK(s.peakFrame == 1u);
	CHECK(s.bankBytes == 378304u);
	CHECK(s.streamBytes == 0u);
	CHECK(s.transfers == 1u);
	CHECK(s.frame == 1u);

	CHECK(spuDiagTrackRange(s, bits, kChunk0Addr, kChunkBytes, 2u) == 1u);
	CHECK(s.currentBytes == 386496u);
	CHECK(s.peakBytes == 386496u);
	CHECK(s.peakFrame == 2u);
	CHECK(s.bankBytes == 378304u);
	CHECK(s.streamBytes == 8192u);
	CHECK(s.transfers == 2u);
	CHECK(s.overlapBytes == 0u);
	CHECK(spuDiagValidate(s) == 1u);
}

void testRewriteCountsOnce() {
	uint8_t bits[kSpuDiagBitsetBytes] = {0};
	PS1SpuDiag s{};
	spuDiagInit(s);
	spuDiagTrackRange(s, bits, kBankAddr, kBankBytes, 1u);
	spuDiagTrackRange(s, bits, kChunk0Addr, kChunkBytes, 2u);

	CHECK(spuDiagTrackRange(s, bits, kChunk0Addr, kChunkBytes, 7u) == 1u);
	CHECK(s.currentBytes == 386496u);  // réécriture : pas de double comptage
	CHECK(s.peakBytes == 386496u);
	CHECK(s.peakFrame == 2u);          // le pic reste à sa frame d'origine
	CHECK(s.overlapBytes == 8192u);
	CHECK(s.transfers == 3u);
	CHECK(s.frame == 7u);
}

void testSecondChunkFillsUnion() {
	uint8_t bits[kSpuDiagBitsetBytes] = {0};
	PS1SpuDiag s{};
	spuDiagInit(s);
	spuDiagTrackRange(s, bits, kBankAddr, kBankBytes, 1u);
	spuDiagTrackRange(s, bits, kChunk0Addr, kChunkBytes, 2u);
	spuDiagTrackRange(s, bits, kChunk1Addr, kChunkBytes, 9u);
	CHECK(s.currentBytes == 394688u);
	CHECK(s.peakBytes == 394688u);
	CHECK(s.peakFrame == 9u);
	CHECK(s.bankBytes == 378304u);
	CHECK(s.streamBytes == 16384u);
	CHECK(s.overlapBytes == 0u);
	CHECK(spuDiagValidate(s) == 1u);
}

void testPartialOverlapCountsOnce() {
	uint8_t bits[kSpuDiagBitsetBytes] = {0};
	PS1SpuDiag s{};
	spuDiagInit(s);
	CHECK(spuDiagTrackRange(s, bits, 0x2000u, 64u, 3u) == 1u);
	CHECK(spuDiagTrackRange(s, bits, 0x2020u, 64u, 4u) == 1u);
	// 64 octets + 32 nouveaux = 12 blocs de 8 = 96 octets ; 32 réécrits.
	CHECK(s.currentBytes == 96u);
	CHECK(s.peakBytes == 96u);
	CHECK(s.overlapBytes == 32u);
	CHECK(s.bankBytes == 96u);
	CHECK(s.transfers == 2u);
}

void testOutOfBoundsRejected() {
	uint8_t bits[kSpuDiagBitsetBytes] = {0};
	PS1SpuDiag s{};
	spuDiagInit(s);
	CHECK(spuDiagTrackRange(s, bits, 524288u, 8u, 1u) == 0u);        // fin hors SPU
	CHECK(spuDiagTrackRange(s, bits, 524280u, 16u, 1u) == 0u);      // debordement
	CHECK(spuDiagTrackRange(s, bits, 0x1010u, 0u, 1u) == 0u);       // taille nulle
	CHECK(spuDiagTrackRange(s, bits, 0x1011u, 8u, 1u) == 0u);       // adresse non alignee
	CHECK(spuDiagTrackRange(s, bits, 0x1010u, 7u, 1u) == 0u);       // taille non alignee
	CHECK(spuDiagTrackRange(s, bits, 0x5D5F8u, 16u, 1u) == 0u);     // chevauche le split
	CHECK(s.rejections == 6u);
	CHECK(s.transfers == 0u);
	CHECK(s.currentBytes == 0u);
	CHECK(s.peakBytes == 0u);
	CHECK(s.peakFrame == 0u);
	CHECK(spuDiagValidate(s) == 0u);
}

void testPeakMonotoneAndIntegrity() {
	uint8_t bits[kSpuDiagBitsetBytes] = {0};
	PS1SpuDiag s{};
	spuDiagInit(s);
	spuDiagTrackRange(s, bits, kBankAddr, kBankBytes, 5u);
	spuDiagStampFrame(s, 20u);
	spuDiagTrackRange(s, bits, kChunk0Addr, kChunkBytes, 20u);
	CHECK(s.currentBytes == 386496u);
	CHECK(s.peakBytes == 386496u);
	CHECK(s.peakFrame == 20u);
	CHECK(s.frame == 20u);
	const uint32_t expected = 0xA2535044u ^ 2u ^ 386496u ^ 386496u ^ 0u;
	CHECK(s.integrity == expected);

	s.peakBytes = 18u;  // POD lu à cheval sur une écriture : somme obsolète
	CHECK(spuDiagValidate(s) == 0u);
}
}  // namespace

int main() {
	testInit();
	testBankThenStream();
	testRewriteCountsOnce();
	testSecondChunkFillsUnion();
	testPartialOverlapCountsOnce();
	testOutOfBoundsRejected();
	testPeakMonotoneAndIntegrity();

	std::printf("PS1_SPU_LOGIC: %d/%d checks PASS\n", gChecks - gFail, gChecks);
	return gFail == 0 ? 0 : 1;
}
