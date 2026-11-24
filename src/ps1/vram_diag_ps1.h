#pragma once

#include "vram_diag_ps1_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

// Table d'occupation VRAM passive : aucun appel GPU, aucune entree manette.
// Le POD est lu directement depuis la RAM emulee par le harnais de capture.
extern PS1VramDiag g_ps1VramDiag;

void vramDiagInitPS1(void);
void vramDiagTrackPS1(int x, int y, int w, int h, uint32_t frame);
void vramDiagStampPS1(uint32_t frame);

#ifdef __cplusplus
}
#endif
