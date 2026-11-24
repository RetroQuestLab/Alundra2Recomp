/*
 * Test hote de la table d'occupation VRAM PS1 (TDD).
 * Compile les vrais en-tetes partages avec le build MIPS.
 * Compilation : clang++ -std=c++20 -o ps1\build\ps1_vram_diag_test.exe tools\ps1_vram_diag_test.cpp
 */
#include "../src/ps1/vram_diag_ps1_logic.h"

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

void testInit() {
	PS1VramDiag s{};
	vramDiagInit(s, 0u, 1048576u);
	CHECK(s.magic == kVramDiagMagic);
	CHECK(s.ready == 1u);
	CHECK(s.framebufferBytes == 0u);
	CHECK(s.vramBytes == 1048576u);
	CHECK(s.halfwords == 524288u);
	CHECK(s.currentBytes == 0u);
	CHECK(s.peakBytes == 0u);
	CHECK(s.uploads == 0u);
	CHECK(s.overlapHalfwords == 0u);
	CHECK(s.rejections == 0u);
	CHECK(vramDiagValidate(s) == 0u);  // pas encore d'upload
}

void testTrackRectAndRewrite() {
	uint8_t bits[kVramDiagBitsetBytes] = {0};
	PS1VramDiag s{};
	vramDiagInit(s, 0u, 1048576u);
	CHECK(vramDiagTrackRect(s, bits, 0, 300, 4, 2, 10u) == 1u);
	CHECK(s.currentBytes == 16u);   // 4 demi-mots x 2 lignes x 2 octets
	CHECK(s.peakBytes == 16u);
	CHECK(s.uploads == 1u);
	CHECK(s.overlapHalfwords == 0u);
	CHECK(s.firstFrame == 10u);
	CHECK(s.lastFrame == 10u);
	CHECK(s.peakFrame == 10u);
	CHECK(s.texturePeakBytes == 16u);

	CHECK(vramDiagTrackRect(s, bits, 0, 300, 4, 2, 11u) == 1u);
	CHECK(s.currentBytes == 16u);   // réécriture : pas de double comptage
	CHECK(s.peakBytes == 16u);
	CHECK(s.uploads == 2u);
	CHECK(s.overlapHalfwords == 8u);
	CHECK(s.peakFrame == 10u);      // le pic reste à la frame qui l'a atteint
}

void testPartialOverlapCountsOnce() {
	uint8_t bits[kVramDiagBitsetBytes] = {0};
	PS1VramDiag s{};
	vramDiagInit(s, 0u, 1048576u);
	CHECK(vramDiagTrackRect(s, bits, 0, 400, 4, 2, 3u) == 1u);
	CHECK(vramDiagTrackRect(s, bits, 2, 400, 4, 2, 4u) == 1u);
	CHECK(s.currentBytes == 24u);   // 8 + 4 nouveaux demi-mots = 12 x 2
	CHECK(s.peakBytes == 24u);
	CHECK(s.overlapHalfwords == 4u);
	CHECK(s.uploads == 2u);
}

void testAdjacentRectGrowsOccupancy() {
	uint8_t bits[kVramDiagBitsetBytes] = {0};
	PS1VramDiag s{};
	vramDiagInit(s, 0u, 1048576u);
	vramDiagTrackRect(s, bits, 0, 300, 4, 2, 1u);
	vramDiagTrackRect(s, bits, 4, 300, 4, 2, 2u);
	CHECK(s.currentBytes == 32u);
	CHECK(s.peakBytes == 32u);
	CHECK(s.overlapHalfwords == 0u);
}

void testOutOfBoundsRejected() {
	uint8_t bits[kVramDiagBitsetBytes] = {0};
	PS1VramDiag s{};
	vramDiagInit(s, 0u, 1048576u);
	CHECK(vramDiagTrackRect(s, bits, 1020, 300, 8, 2, 1u) == 0u);
	CHECK(vramDiagTrackRect(s, bits, 0, 511, 4, 2, 1u) == 0u);
	CHECK(vramDiagTrackRect(s, bits, 0, 300, 0, 2, 1u) == 0u);
	CHECK(vramDiagTrackRect(s, bits, 0, 300, 4, 0, 1u) == 0u);
	CHECK(s.rejections == 4u);
	CHECK(s.uploads == 0u);
	CHECK(s.currentBytes == 0u);
	CHECK(s.peakBytes == 0u);
}

void testPeakMonotoneAcrossFrames() {
	uint8_t bits[kVramDiagBitsetBytes] = {0};
	PS1VramDiag s{};
	vramDiagInit(s, 0u, 1048576u);
	vramDiagTrackRect(s, bits, 0, 200, 4, 4, 5u);
	vramDiagStampFrame(s, 20u);
	// Aucune liberation n'existe dans la table VRAM : une nouvelle écriture ne
	// peut qu'augmenter l'occupation ou la laisser inchangée. Réécrire le même
	// rectangle à une frame plus tardive doit conserver le pic et sa frame.
	vramDiagTrackRect(s, bits, 0, 200, 4, 4, 20u);
	CHECK(s.currentBytes == 32u);
	CHECK(s.peakBytes == 32u);
	CHECK(s.peakFrame == 5u);
	CHECK(s.firstFrame == 5u);
	CHECK(s.lastFrame == 20u);
	CHECK(s.frame == 20u);
}

void testFramebuffersAndAtlasSequence() {
	uint8_t bits[kVramDiagBitsetBytes] = {0};
	PS1VramDiag s{};
	vramDiagInit(s, 307200u, 1048576u);
	vramDiagTrackRect(s, bits, 0, 0, 320, 240, 1u);
	CHECK(s.currentBytes == 153600u);
	vramDiagTrackRect(s, bits, 320, 0, 320, 240, 1u);
	CHECK(s.currentBytes == 307200u);
	vramDiagTrackRect(s, bits, 0, 256, 64, 256, 2u);
	CHECK(s.currentBytes == 339968u);   // 307200 + 64 x 256 x 2
	CHECK(s.peakBytes == 339968u);
	CHECK(s.texturePeakBytes == 32768u);
	CHECK(s.peakFrame == 2u);
	CHECK(vramDiagValidate(s) == 1u);
}

void testIntegrityDetectsStalePod() {
	uint8_t bits[kVramDiagBitsetBytes] = {0};
	PS1VramDiag s{};
	vramDiagInit(s, 0u, 1048576u);
	vramDiagTrackRect(s, bits, 0, 300, 4, 2, 1u);
	vramDiagTrackRect(s, bits, 0, 300, 4, 2, 2u);
	const uint32_t expected = kVramDiagMagic ^ 2u ^ 16u ^ 16u ^ 8u;
	CHECK(s.integrity == expected);
	s.peakBytes = 18u;  // POD lu à cheval sur une écriture : somme obsolète
	CHECK(vramDiagValidate(s) == 0u);
}
}  // namespace

int main() {
	testInit();
	testTrackRectAndRewrite();
	testPartialOverlapCountsOnce();
	testAdjacentRectGrowsOccupancy();
	testOutOfBoundsRejected();
	testPeakMonotoneAcrossFrames();
	testFramebuffersAndAtlasSequence();
	testIntegrityDetectsStalePod();

	std::printf("PS1_VRAM_LOGIC: %d/%d checks PASS\n", gChecks - gFail, gChecks);
	return gFail == 0 ? 0 : 1;
}
