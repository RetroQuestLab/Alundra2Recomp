#pragma once

#include "spu_diag_ps1_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

// Table d'occupation SPU passive : aucun appel SPU, aucune entree manette.
// Le POD est lu directement depuis la RAM emulee par le harnais de capture.
extern PS1SpuDiag g_ps1SpuDiag;

void spuDiagInitPS1(void);
void spuDiagTrackRangePS1(uint32_t addr, uint32_t bytes);
void spuDiagStampPS1(uint32_t frame);

#ifdef __cplusplus
}
#endif
