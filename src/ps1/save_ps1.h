#pragma once
/*
 * Pilote de sauvegarde PS1 natif (carte memoire, BIOS via PSn00bSDK).
 * Aucun STL, aucune allocation dynamique. Le format POD et ses tests sont dans
 * save_ps1_logic.h ; ce module ne fait que de l'E/S et l'orchestration.
 *
 * Toutes les E/S carte sont asynchrones : le BIOS traite un secteur par VBlank.
 * savePumpPS1() doit etre appele une fois par image ; il fait avancer la
 * detection et les ecritures sans bloquer la boucle de jeu ni le pilote PAD.
 */
#include <stdint.h>

#include "save_ps1_logic.h"

/* Amorce le BIOS carte AVANT le premier VSync. A ce stade, psxgpu n'a pas
 * encore installe son callback VBlank : les appels BIOS ne peuvent donc pas
 * l'ecraser. Ne fait aucune E/S carte. */
void saveEarlyInitPS1(void);

/* Lance la detection des deux slots. Retourne 1 des que les lectures sont
 * terminees, 0 tant que la pompe n'a pas fini. */
int saveInitPS1(void);

/* Fait avancer d'un cran les operations carte en attente. */
void savePumpPS1(void);

/* Primitives utilisees par le halt VSync de pf_ps1.cpp. Le BIOS doit garder
 * l'auto-ack VBlank actif pendant une operation carte (sinon le verificateur
 * SIO0 re-entre firstStageCardAction() sur chaque ACK et pose le statut 0x11).
 * Tant que l'auto-ack est actif, le compteur VBlank de psxgpu est gele : le halt
 * doit donc polling _card_status() et desactiver l'auto-ack des que l'operation
 * se termine, pour rendre la main a VSync(). */
int saveCardOpPendingPS1(void);
int saveCardStatusPS1(void);

/* 1 si une sauvegarde valide a ete detectee au demarrage ou apres ecriture. */
int saveHasGamePS1(void);

/* Retourne la version courante du bloc (sequence) ou 0. */
uint16_t saveSequencePS1(void);

/* Prepare l'ecriture de l'etat courant dans le slot inactif. La verification
 * par relecture est faite par savePumpPS1(). Retourne 1 si l'operation a pu
 * etre lancee. */
int saveWriteGamePS1(int zone, int hx, int hy, int dir);

/* Restaure l'etat courant depuis le meilleur slot. Renseigne zone/hx/hy/dir
 * pour la boucle de jeu, recharge l'inventaire et la quete. Retourne 0 si
 * aucune sauvegarde valide. */
int saveReadGamePS1(int* zone, int* hx, int* hy, int* dir);
