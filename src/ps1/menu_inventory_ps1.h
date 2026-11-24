#pragma once
/*
 * Menu armes / objets / equipement PS1 natif.
 *
 * Presentation Alundra 1 : police FONT3, fenetres WIND, icones a1_icons.a1s
 * natives (CLUT partagee). Contenu Alundra 2 : les 59 objets de data/items.txt.
 * Aucun STL, aucune allocation : la logique POD vit dans inventory_ps1_logic.h.
 */
#include "../platform/pf.h"

/* Televerse les 84 icones + leur CLUT en VRAM et initialise l'inventaire A2. */
void inventoryInitPS1(void);

/* Ouvre le menu : reinitialise la page/curseur et arme le front de manette. */
void inventoryOpenPS1(uint32_t pad);

/* Traite la manette (navigation, pages, CROIX = utiliser/equipper). */
void inventoryUpdatePS1(uint32_t pad);

/* Dessine l'ecran complet (fond nu + panneaux WIND). */
void inventoryDrawPS1(void);

/* 1 si le menu est ouvert. */
int inventoryIsOpenPS1(void);

/* Ajoute un objet au jeu (pont avec la quete / les interactions). */
int inventoryAddPS1(const char* code, int qty);

/* Quantite possedee (0 si absent). */
int inventoryQtyPS1(const char* code);

/* Sauvegarde / restauration versionnee dans un bloc POD de PSI_SAVE_SIZE. */
int inventorySavePS1(uint8_t* buf);
int inventoryLoadPS1(const uint8_t* buf);
