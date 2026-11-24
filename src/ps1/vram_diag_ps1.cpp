/*
 * Table d'occupation VRAM PS1 : un bit par demi-mot (1024 x 512_bits).
 *
 * Le bitmap est statique (BSS, 65 536 octets) : aucune allocation, aucun
 * descripteur de taille figee, aucun appel au GPU. Les deux framebuffers sont
 * reserves une fois depuis pfInit ; la logique pure est dans le header partage
 * avec le test hote.
 */
#include "vram_diag_ps1.h"

#include "../platform/pf.h"

namespace {
uint8_t s_vramBits[kVramDiagBitsetBytes];
}  // namespace

PS1VramDiag g_ps1VramDiag;

void vramDiagInitPS1(void) {
	const uint32_t framebufferBytes = 2u * PF_W * PF_H * 2u;
	const uint32_t vramBytes = PF_VRAM_W * PF_VRAM_H * 2u;
	vramDiagInit(g_ps1VramDiag, framebufferBytes, vramBytes);
}

void vramDiagTrackPS1(int x, int y, int w, int h, uint32_t frame) {
	vramDiagTrackRect(g_ps1VramDiag, s_vramBits, x, y, w, h, frame);
}

void vramDiagStampPS1(uint32_t frame) {
	vramDiagStampFrame(g_ps1VramDiag, frame);
}
