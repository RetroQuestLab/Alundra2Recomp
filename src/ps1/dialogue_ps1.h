#pragma once
/*
 * Dialogue A1 natif pour la cible PS1 : police FONT3.TIM et fenetre WIND.TX
 * televersees en VRAM, panneaux de narration A2 generes depuis data/dialogues.txt.
 * Aucun STL, aucune allocation dynamique (toolchain MIPS PSn00bSDK 0.24).
 */
#include <stdint.h>

/* Televerse atlas + CLUT en VRAM. A appeler apres pfInit() et zoneInit().
 * Apres dlgInit(), aucun dialogue n'est ouvert : la fenetre n'est pas dessinee. */
void dlgInit(void);

/* Ouvre un des identifiants exportes dans dialogue_data_ps1.h (kDlgTable). */
void dlgOpen(int dialogueId);

/* 0/1 : un dialogue est-il en cours ? */
int dlgActive(void);

/* Identifiant courant (kDlgTable) ou -1. */
int dlgId(void);

/* 1 si le dernier panneau vient d'etre ferme, puis remis a 0 (front consomme). */
int dlgConsumeFinished(void);

/* Ferme immediatement le dialogue courant (nouvelle partie / retour titre). */
void dlgClose(void);

/* Typewriter et navigation (START : terminer le panneau puis passer au suivant). */
void dlgUpdate(uint32_t pad);

/* Dessine le panneau courant dans l'ordering table, uniquement si ouvert. */
void dlgDraw(void);

/* Index du panneau courant (diagnostic / captures deterministes). */
int dlgPanel(void);

/* Ecran de fin natif : fenetre A1 et six lignes A2 exportees. Appele uniquement
 * apres mephisto_vaincu et la derniere replique d'Alexia. */
void endingDrawPS1(int frame);

/* Ecran local de Game Over : voile noir progressif, fenetre WIND.TX et deux
 * lignes FONT3.TIM. Aucun dialogue, aucune quete, aucun fichier de donnees. */
void gameOverDrawPS1(int frame);
