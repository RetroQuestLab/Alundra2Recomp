/*
 * Test hote du module SPU PS1 partage avec le build MIPS (TDD).
 * Compile le vrai module : il ne depend que des POD audio, aucun appel SPU.
 * Compilation : clang++ -std=c++20 -o ps1\build\ps1_spu_diag_host_test.exe tools\ps1_spu_diag_host_test.cpp
 */
#include "../src/ps1/spu_diag_ps1.cpp"

#include <cstdint>
#include <cstdio>

PS1StreamDiag g_ps1StreamDiag = {0u, 0u, 0u, 0u, 0u, 0u, 0u,
                                 0u, 0u, 0u, 0u, 0u, 0u, 0u};

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
}  // namespace

int main() {
	spuDiagInitPS1();
	CHECK(g_ps1SpuDiag.magic == 0xA2535044u);
	CHECK(g_ps1SpuDiag.ready == 1u);
	CHECK(g_ps1SpuDiag.spuBytes == 524288u);
	CHECK(g_ps1SpuDiag.currentBytes == 0u);

	// La banque est televersee avant le premier pfPresent : frame 0.
	spuDiagTrackRangePS1(0x1010u, 378304u);
	CHECK(g_ps1SpuDiag.currentBytes == 378304u);
	CHECK(g_ps1SpuDiag.peakBytes == 378304u);
	CHECK(g_ps1SpuDiag.peakFrame == 0u);
	CHECK(g_ps1SpuDiag.bankBytes == 378304u);

	spuDiagStampPS1(10u);
	g_ps1StreamDiag.chunks = 1u;
	g_ps1StreamDiag.underruns = 0u;
	spuDiagTrackRangePS1(0x5D600u, 8192u);
	CHECK(g_ps1SpuDiag.currentBytes == 386496u);
	CHECK(g_ps1SpuDiag.streamBytes == 8192u);
	CHECK(g_ps1SpuDiag.peakFrame == 10u);

	spuDiagStampPS1(11u);
	g_ps1StreamDiag.chunks = 2u;
	spuDiagTrackRangePS1(0x5F600u, 8192u);
	CHECK(g_ps1SpuDiag.currentBytes == 394688u);
	CHECK(g_ps1SpuDiag.peakBytes == 394688u);
	CHECK(g_ps1SpuDiag.peakFrame == 11u);
	CHECK(g_ps1SpuDiag.streamBytes == 16384u);
	CHECK(g_ps1SpuDiag.transfers == 3u);

	// La reecriture du demi-buffer ne double pas l'occupation.
	g_ps1StreamDiag.chunks = 3u;
	spuDiagStampPS1(12u);
	spuDiagTrackRangePS1(0x5D600u, 8192u);
	CHECK(g_ps1SpuDiag.currentBytes == 394688u);
	CHECK(g_ps1SpuDiag.overlapBytes == 8192u);
	CHECK(g_ps1SpuDiag.peakFrame == 11u);
	CHECK(g_ps1SpuDiag.chunks == 3u);
	CHECK(g_ps1SpuDiag.underruns == 0u);

	spuDiagStampPS1(13u);
	CHECK(g_ps1SpuDiag.frame == 13u);
	CHECK(g_ps1SpuDiag.chunks == 3u);
	CHECK(spuDiagValidate(g_ps1SpuDiag) == 1u);

	std::printf("PS1_SPU_HOST: %d/%d checks PASS\n", gChecks - gFail, gChecks);
	return gFail == 0 ? 0 : 1;
}
