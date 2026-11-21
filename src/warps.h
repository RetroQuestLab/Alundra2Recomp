#pragma once
#include <string>
#include <vector>

// Passages entre zones, definis par nous (data/warps.txt) : le monde d'Alundra 2 est assemble
// a partir des zones d'Alundra 1.
//
// Format d'une ligne :
//   <zone> <x> <y> <largeur> <hauteur> -> <zone_dest> <x_dest> <y_dest> <direction> [objet_ou_evenement]
//   ex. : 011 24 50 2 1 -> 050 25 40 haut
// Le dernier mot, optionnel, est un objet/evenement indispensable pour franchir le passage.
// Coordonnees en cases. Direction : bas | haut | gauche | droite. '#' = commentaire.
struct Warp {
    std::string zone;
    int x = 0, y = 0, w = 1, h = 1;
    std::string destZone;
    int destX = 0, destY = 0;
    int destDir = 0; // 0 bas, 1 haut, 2 gauche, 3 droite
    std::string need; // objet ou evenement requis (vide = libre)

    bool contains(int cx, int cy) const { return cx >= x && cx < x + w && cy >= y && cy < y + h; }
};

class WarpTable {
public:
    bool load(const std::string& path);
    bool save() const;
    const Warp* find(const std::string& zone, int cx, int cy) const;
    void add(const Warp& w) { warps_.push_back(w); }
    bool removeAt(const std::string& zone, int cx, int cy);
    const std::vector<Warp>& all() const { return warps_; }
    const std::string& path() const { return path_; }

private:
    std::string path_;
    std::vector<Warp> warps_;
};
