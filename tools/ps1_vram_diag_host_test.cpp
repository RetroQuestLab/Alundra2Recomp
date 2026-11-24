/*
 * Test hote des helpers VRAM PS1 partages avec le build MIPS (TDD).
 * Compile le vrai module MIPS (sans appel GPU) : il ne depend que de pf.h.
 * Compilation : clang++ -std=c++20 -o ps1\build\ps1_vram_diag_host_test.exe tools\ps1_vram_diag_host_test.cpp
 */
#include "../src/ps1/vram_diag_ps1.cpp"

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
}  // namespace

int main() {
	vramDiagInitPS1();
	CHECK(g_ps1VramDiag.magic == kVramDiagMagic);
	CHECK(g_ps1VramDiag.ready == 1u);
	CHECK(g_ps1VramDiag.framebufferBytes == 307200u);
	CHECK(g_ps1VramDiag.vramBytes == 1048576u);
	CHECK(g_ps1VramDiag.currentBytes == 0u);

	// pfInit reserve les deux framebuffers avant tout upload de texture.
	vramDiagTrackPS1(PF_FB0_X, PF_FB0_Y, PF_W, PF_H, 1u);
	CHECK(g_ps1VramDiag.currentBytes == 153600u);
	vramDiagTrackPS1(PF_FB1_X, PF_FB1_Y, PF_W, PF_H, 1u);
	CHECK(g_ps1VramDiag.currentBytes == 307200u);
	CHECK(g_ps1VramDiag.peakBytes == 307200u);
	CHECK(g_ps1VramDiag.peakFrame == 1u);

	// Atlas de police A1 : 64 x 256 demi-mots = 32768 octets.
	vramDiagTrackPS1(PF_FONT_X, PF_FONT_Y, PF_FONT_W, PF_FONT_H, 2u);
	CHECK(g_ps1VramDiag.currentBytes == 339968u);
	CHECK(g_ps1VramDiag.peakBytes == 339968u);
	CHECK(g_ps1VramDiag.texturePeakBytes == 32768u);
	CHECK(g_ps1VramDiag.peakFrame == 2u);
	CHECK(g_ps1VramDiag.uploads == 3u);

	vramDiagStampPS1(12u);
	CHECK(g_ps1VramDiag.frame == 12u);
	CHECK(g_ps1VramDiag.lastFrame == 12u);
	CHECK(g_ps1VramDiag.firstFrame == 1u);
	CHECK(vramDiagValidate(g_ps1VramDiag) == 1u);

	std::printf("PS1_VRAM_HOST: %d/%d checks PASS\n", gChecks - gFail, gChecks);
	return gFail == 0 ? 0 : 1;
}
