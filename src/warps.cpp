#include "warps.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace {
const char* DIR_NAMES[4] = {"bas", "haut", "gauche", "droite"};

int parseDir(const std::string& s) {
    for (int i = 0; i < 4; ++i)
        if (s == DIR_NAMES[i]) return i;
    return 0;
}
} // namespace

bool WarpTable::load(const std::string& path) {
    path_ = path;
    warps_.clear();
    std::ifstream in(path);
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (auto c = line.find('#'); c != std::string::npos) line.resize(c);
        std::istringstream ss(line);
        Warp w;
        std::string arrow, dir;
        if (ss >> w.zone >> w.x >> w.y >> w.w >> w.h >> arrow >> w.destZone >> w.destX >> w.destY >> dir &&
            arrow == "->") {
            w.destDir = parseDir(dir);
            ss >> w.need; // optionnel : cle, relique, evenement d'histoire...
            warps_.push_back(w);
        }
    }
    return true;
}

bool WarpTable::save() const {
    std::ofstream out(path_);
    if (!out) return false;
    out << "# Passages entre zones du remake (voir src/warps.h pour le format).\n"
           "# zone x y largeur hauteur -> zone_dest x_dest y_dest direction\n";
    for (const Warp& w : warps_) {
        out << w.zone << ' ' << w.x << ' ' << w.y << ' ' << w.w << ' ' << w.h << " -> " << w.destZone << ' '
            << w.destX << ' ' << w.destY << ' ' << DIR_NAMES[w.destDir & 3];
        if (!w.need.empty()) out << ' ' << w.need;
        out << '\n';
    }
    return true;
}

const Warp* WarpTable::find(const std::string& zone, int cx, int cy) const {
    for (const Warp& w : warps_)
        if (w.zone == zone && w.contains(cx, cy)) return &w;
    return nullptr;
}

bool WarpTable::removeAt(const std::string& zone, int cx, int cy) {
    auto it = std::find_if(warps_.begin(), warps_.end(),
                           [&](const Warp& w) { return w.zone == zone && w.contains(cx, cy); });
    if (it == warps_.end()) return false;
    warps_.erase(it);
    return true;
}
