#pragma once
// Compléments du menu armes / objets / équipement au-delà des interfaces de game.h.
// Les états et les points d'entrée (openMenu, closeMenu, updateMenu, drawMenu)
// sont déclarés dans game.h ; ce header expose seulement les lectures dont
// Gameplay a besoin pour relier la sélection à useItem / equipWeapon.
#include <string>
#include <vector>

#include "game.h"

// Identifiants affichés sur une page, dans l'ordre exact du curseur (commun à
// updateMenu et drawMenu). Une entrée d'équipement vide a un identifiant vide.
std::vector<std::string> menuItemIds(const Game& g, MenuPage page);

// Identifiant sous le curseur, ou chaîne vide si la page est vide.
std::string menuSelectedId(const Game& g);

// Icone d'inventaire d'un objet (data/item_icons.txt) dessinee en (x, y) ; faux si aucune icone.
bool drawItemIcon(Game& g, const std::string& id, float x, float y);
// Vrai si assets/sprites/<name>.a1s existe (sans journaliser d'erreur ni charger la banque).
bool itemSpriteExists(const Game& g, const std::string& name);
