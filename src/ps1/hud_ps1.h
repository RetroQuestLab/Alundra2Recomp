#pragma once
/* HUD PS1 : 15 frames A1 reelles (a1_hud.a1s) — 4 gemmes de vie, 10 chiffres,
 * slash du suffixe "/MM" — aucun rectangle ni glyphe invente. */

#ifdef __cplusplus
extern "C" {
#endif

void hudInitPS1(void);
/* PV affiches par le HUD ; bornes applies par le pilote. */
void hudSetVitalsPS1(int hp, int maxHp);
void hudDrawPS1(void);

#ifdef __cplusplus
}
#endif
