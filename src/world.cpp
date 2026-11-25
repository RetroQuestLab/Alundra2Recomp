// Monde : zones, collisions, donnees du remake (data/world.txt, data/warps.txt, data/dialogues.txt)
#include <cmath>
#include <fstream>
#include <sstream>

#include "game.h"

namespace fs = std::filesystem;

static bool walkable(const Map& m, float x, float y, int z) {
    const Cell* c = m.cell(int(std::floor(x / TILE_W)), int(std::floor(y / TILE_H)));
    return c && c->top != 0xFFFF && !(c->attr & ATTR_BLOCK) && std::abs(int(c->height) - z) <= 1;
}

bool canStand(const Map& m, float x, float y, int z, float hw, float hd) {
    return walkable(m, x - hw, y - hd, z) && walkable(m, x + hw, y - hd, z) && walkable(m, x - hw, y, z) &&
           walkable(m, x + hw, y, z);
}

std::string zoneName(const Game& g) { return g.maps.empty() ? "" : g.maps[g.current].stem().string(); }

SpriteBank* spriteBank(Game& g, const std::string& name) {
    auto it = g.sprites.find(name);
    if (it != g.sprites.end()) return it->second.get();
    auto bank = std::make_unique<SpriteBank>();
    if (!bank->load(g.renderer, (g.assets / "sprites" / (name + ".a1s")).string())) {
        SDL_Log("Sprite introuvable : %s", name.c_str());
        g.sprites[name] = nullptr;
        return nullptr;
    }
    return (g.sprites[name] = std::move(bank)).get();
}

static Dir parseDir(const std::string& s) {
    if (s == "haut") return UP;
    if (s == "gauche") return LEFT;
    if (s == "droite") return RIGHT;
    return DOWN;
}

// Cherche une case degagee proche de (tx, ty) si aucune position n'est imposee
static void findFreeCell(const Map& m, int& cx, int& cy) {
    const int tx = cx >= 0 ? cx : m.width() / 2, ty = cy >= 0 ? cy : m.height() * 3 / 4;
    int best = -1;
    for (int y = 1; y < m.height() - 1; ++y)
        for (int x = 1; x < m.width() - 1; ++x) {
            const Cell* c = m.cell(x, y);
            if (!c || c->top == 0xFFFF || (c->attr & ATTR_BLOCK)) continue;
            bool ok = true;
            for (int j = -1; j <= 1 && ok; ++j)
                for (int i = -1; i <= 1 && ok; ++i) {
                    const Cell* n = m.cell(x + i, y + j);
                    ok = n && n->top != 0xFFFF && n->height == c->height && !(n->attr & ATTR_BLOCK);
                }
            if (!ok) continue;
            const int d = std::abs(x - tx) + std::abs(y - ty);
            if (best < 0 || d < best) { best = d; cx = x; cy = y; }
        }
    if (best < 0) { cx = tx; cy = ty; }
}

bool loadZone(Game& g, const std::string& zone, int cellX, int cellY, Dir dir) {
    int index = -1;
    for (size_t i = 0; i < g.maps.size(); ++i)
        if (g.maps[i].stem() == zone) index = int(i);
    if (index < 0) return false;
    fs::path mapPath = g.maps[index];
    if (auto it = g.mapFor.find(zone); it != g.mapFor.end()) {
        fs::path alt = g.assets / "maps" / (it->second + ".a1m");
        if (fs::exists(alt)) mapPath = alt;
        else SDL_Log("Carte de remplacement introuvable pour %s : %s", zone.c_str(), alt.string().c_str());
    }
    auto m = std::make_unique<Map>();
    if (!m->load(g.renderer, mapPath.string())) {
        SDL_Log("Echec du chargement de la zone %s", zone.c_str());
        return false;
    }
    g.current = index;
    g.map = std::move(m);

    // Position : celle demandee, ou une case libre proche du bas-centre
    const Cell* c = g.map->cell(cellX, cellY);
    if (!c || c->top == 0xFFFF) findFreeCell(*g.map, cellX, cellY);
    Hero& h = g.hero;
    h.x = cellX * TILE_W + TILE_W / 2.f;
    h.y = cellY * TILE_H + TILE_H / 2.f; // milieu de case : la rangee suivante ne recouvre pas les pieds
    if (const Cell* cc = g.map->cell(cellX, cellY)) h.z = cc->height;
    h.dir = dir;
    h.kx = h.ky = 0;
    h.dashTicks = 0;
    h.airborne = false;
    h.air = h.vz = 0;
    h.safeX = h.x;
    h.safeY = h.y;
    g.respawnX = h.x;
    g.respawnY = h.y;
    g.onWarp = true;

    g.particles.clear();
    g.effects.clear();
    g.texts.clear();
    g.pickups.clear();
    g.pendingEnemies.clear();
    spawnZoneEntities(g);
    g.chests.clear();
    spawnChests(g);
    g.music.boss = false;
    playZoneMusic(g);

    // Evenement d'histoire : joue une fois, et reste joue apres une sauvegarde.
    if (auto it = g.zoneEvents.find(zone); it != g.zoneEvents.end()) {
        const auto& [id, dialogue] = it->second;
        if (!g.seenEvents.count(id)) {
            g.seenEvents.insert(id);
            openDialogue(g, dialogue);
        }
    }

    // Nom du lieu : affiche quand on change de lieu (pas en reapparaissant au meme endroit)
    if (auto t = g.zoneTitles.find(zone); t != g.zoneTitles.end() && t->second != g.banner) {
        g.banner = t->second;
        g.bannerTicks = 200;
    }

    saveGame(g);
    const std::string title = "Alundra 2 Remake | zone " + zone;
    SDL_SetWindowTitle(g.window, title.c_str());
    return true;
}

// data/world.txt :
//   start <zone> <x> <y> <direction>
//   spawn <zone> <x> <y> <monstre>                (gelee, tortue, gobelin, ogre)
//   npc <zone> <x> <y> <sprite> <direction> <dialogue>
//   event <zone> <id> <dialogue>                  (dialogue joue une seule fois a l'entree de la zone)
//   zone <zone> <categorie>                       (village, plaine, donjon, boss : voir data/music.txt)
//   map <zone> <carte A1>                         (remplace assets/maps/<zone>.a1m par <carte>.a1m)
//   music <zone|titre> <morceau>                  (assets/music/NN.wav)
void loadWorldData(Game& g) {
    g.warps.load((g.data / "warps.txt").string());
    loadDialogues(g, (g.data / "dialogues.txt").string());

    std::ifstream in(g.data / "world.txt");
    std::string line;
    while (std::getline(in, line)) {
        if (auto c = line.find('#'); c != std::string::npos) line.resize(c);
        std::istringstream ss(line);
        std::string cmd;
        if (!(ss >> cmd)) continue;
        if (cmd == "start") {
            std::string d;
            ss >> g.startZone >> g.startX >> g.startY >> d;
            g.startDir = parseDir(d);
        } else if (cmd == "spawn") {
            Game::Spawn s;
            std::string kind;
            if (ss >> s.zone >> s.x >> s.y >> kind)
                if (auto k = enemyKindFromId(kind)) {
                    s.kind = *k;
                    g.spawns.push_back(s);
                }
        } else if (cmd == "npc") {
            Game::NpcDef n;
            std::string d;
            if (ss >> n.zone >> n.x >> n.y >> n.sprite >> d >> n.dialogue) {
                n.dir = parseDir(d);
                std::string opt;
                if (ss >> opt && opt.rfind("masque:", 0) == 0) n.hideAfter = opt.substr(7);
                g.npcDefs.push_back(n);
            }
        } else if (cmd == "zone") {
            std::string zone, kind;
            if (ss >> zone >> kind) g.zoneKind[zone] = kind;
        } else if (cmd == "map") {
            std::string zone, mapId;
            if (ss >> zone >> mapId) g.mapFor[zone] = mapId;
        } else if (cmd == "music") {
            std::string zone;
            int track;
            if (ss >> zone >> track) g.zoneMusic[zone] = track;
        } else if (cmd == "chest") { // chest <zone> <x> <y> <objet> [ogre]
            Game::ChestDef c;
            if (ss >> c.zone >> c.x >> c.y >> c.item) {
                ss >> c.need;
                g.chestDefs.push_back(c);
            }
        } else if (cmd == "name") { // name <zone> <nom du lieu> : bandeau a l'entree de la zone
            std::string zone, title;
            if (ss >> zone && std::getline(ss >> std::ws, title)) {
                while (!title.empty() && (title.back() == ' ' || title.back() == '\r')) title.pop_back();
                g.zoneTitles[zone] = title;
            }
        } else if (cmd == "event") { // event <zone> <id> <dialogue>
            std::string zone, id, dialogue;
            if (ss >> zone >> id >> dialogue) g.zoneEvents[zone] = {id, dialogue};
        } else if (cmd == "intro") {
            ss >> g.afterIntro;
        }
    }
}



