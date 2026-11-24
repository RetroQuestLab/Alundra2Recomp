#pragma once

#include "diag_ps1_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

// Etat lisible depuis le harnais de capture (RAM emulee) sans modifier le jeu.
extern PS1DiagState g_ps1Diag;

void diagInitPS1(void);
void diagSamplePS1(void);

#ifdef __cplusplus
}
#endif
