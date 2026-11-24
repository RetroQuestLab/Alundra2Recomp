#pragma once
/*
 * Ecran titre et menu de depart natif PS1.
 * Rendu avec la police FONT3 et la fenetre WIND televersees par dlgInit().
 * La logique (fronts montants, navigation, options) vit dans menu_ps1_logic.h.
 */
#include <stdint.h>

/* hasSave / audioAvailable : capacites reelles du moment (0 tant que le
 * systeme de sauvegarde / SPU n'est pas porte). */
void menuInitPS1(int hasSave, int audioAvailable);
/* Met a jour la disponibilite de la sauvegarde sans reinitialiser le curseur
 * ni l'ecran courant (la detection carte est asynchrone). */
void menuSetHasSavePS1(int hasSave);
void menuUpdatePS1(uint32_t pad);
void menuDrawPS1(void);
/* "Quitter" ramene au titre : la PS1 n'a pas de processus a terminer. Le
 * front montant de la manette est conserve pour ne pas reopenir le menu. */
void menuReturnToTitlePS1(void);

int menuScreenPS1(void);
int menuActionPS1(void);
