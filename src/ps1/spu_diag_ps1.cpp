/*
 * Table d'occupation SPU PS1 : un bit par bloc de 8 octets (8192 octets de
 * BSS). Le bitmap est statique : aucune allocation, aucun appel SPU. Les deux
 * seuls transferts trackes sont la banque VAG (audioInitPS1) et le double
 * buffer musical (handler IRQ de cd_stream.cpp). La logique pure est dans le
 * header partage avec les tests hotes.
 */
#include "spu_diag_ps1.h"

#include "audio_ps1.h"

namespace {
uint8_t s_spuBits[kSpuDiagBitsetBytes];
uint32_t s_lastFrame = 0u;
}  // namespace

PS1SpuDiag g_ps1SpuDiag;

void spuDiagInitPS1(void) {
	s_lastFrame = 0u;
	spuDiagInit(g_ps1SpuDiag);
}

void spuDiagTrackRangePS1(uint32_t addr, uint32_t bytes) {
	spuDiagTrackRange(g_ps1SpuDiag, s_spuBits, addr, bytes, s_lastFrame);
}

void spuDiagStampPS1(uint32_t frame) {
	s_lastFrame = frame;
	g_ps1SpuDiag.chunks = g_ps1StreamDiag.chunks;
	g_ps1SpuDiag.underruns = g_ps1StreamDiag.underruns;
	spuDiagStampFrame(g_ps1SpuDiag, frame);
}
