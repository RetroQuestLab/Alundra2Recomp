#pragma once
/*
 * Rendu et pilotage des interactions PS1 (011, 013, 019, 119 et salle du trone 120).
 * La logique pure est dans quest_ps1_logic.h ; ce module ne contient que des
 * appels a pf* et au dialogue natif. Aucune STL, aucune allocation dynamique.
 */
#include <stdint.h>

#include "hero_vitals_ps1_logic.h"
#include "quest_ps1_logic.h"

/* A appeler apres heroInit()/dlgInit(), et a chaque nouvelle partie. */
void interactionInitPS1(void);

/* Nouvelle partie, fin de l'intro : ouvre [intro] comme le PC (afterIntro). */
void interactionAfterIntroPS1(void);

/* Change la zone active : met a jour la quete et televerse les sprites PNJ
 * de la nouvelle zone dans les trois emplacements VRAM partages. */
void interactionSetZonePS1(int zone);

/* 1 si le front CROIX courant va ouvrir un dialogue : le saut du heros est
 * alors neutralise pour cette frame. Lis l'etat sans le modifier. */
int interactionWantsTalkPS1(uint32_t pad, int hx, int hy);

/* Met a jour la quete, ouvre le dialogue et consomme sa fin. */
void interactionUpdatePS1(uint32_t pad, int hx, int hy);

/* Constantes vitales LOCALES du heros : etat global observable par la sonde
 * Lua (adresse ELF), ecrit uniquement via l'API heroVitals* (Init/Reset/
 * Respawn/Tick/TryHit), jamais par la quete, la sauvegarde ou un warp. */
extern PS1HeroVitalsState g_ps1HeroVitals;
const PS1HeroVitalsState* interactionVitalsPS1(void);

/* Dessine les PNJ, leurs bulles et l'indicateur d'objectif (apres zoneDraw). */
void interactionDrawPS1(int camX, int camY);

/* Diagnostics deterministes pour les captures natives. */
int interactionStagePS1(void);
int interactionDialoguePS1(void);
int interactionNearNpcPS1(int hx, int hy);
int interactionNearActorPS1(int hx, int hy);

/* Masque QuestState::gates : lu par la boucle pour le passage conditionne
 * 086→120, sans exposer l'etat de quete complet au reste du jeu. */
uint32_t interactionGatesPS1(void);

/* Etat de quete complet pour la sauvegarde carte memoire. Copie POD : aucun
 * pointeur n'est expose, les offsets Lua restent ceux du harnais. */
void interactionQuestPS1(QuestState* out);
void interactionSetQuestPS1(const QuestState* in);

/* 1 des que la derniere replique d'Alexia a ete fermee apres la victoire.
 * La boucle bascule alors en mode Ending ; aucun simple retour en zone 120 ne
 * peut declencher cet etat. */
int interactionEndingPS1(void);

/* Zone simulatee par le harnais de capture pour tester 086→120 sans charger la
 * carte du manoir : 0 = aucune simulation. Le jeu n'ecrit jamais cette valeur. */
int interactionCaptureZonePS1(void);
void interactionSetCaptureZonePS1(int zone);
