// Coffres, objets et sauvegarde.
//
// Coffres (sprite d'origine d'Alundra 1 : ferme, ouvert, ouverture) : data/world.txt
//   chest <zone> <x> <y> <objet> [ogre]      "ogre" : le coffre n'apparait qu'une fois l'ogre vaincu
// Objets : coeur (fiole de vie : +1 coeur), herbe (touche H : rend 2 coeurs), lettre (objet d'histoire), cle.
// A l'ouverture, l'objet sort du coffre et monte au-dessus du heros (voir openChest), puis le dialogue
// "objet_<objet>" s'affiche.
//
// Sauvegarde automatique (changement de zone, coffre, boss) dans le dossier de configuration SDL : sauvegarde.txt.
#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string_view>

#include "game.h"

namespace {
// --- Lecture des donnees du remake (format id|champ|...) -----------------------
std::string trimmed(std::string s) {
    const auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    const auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::vector<std::string> splitOn(const std::string& s, char sep) {
    std::vector<std::string> out;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i)
        if (i == s.size() || s[i] == sep) {
            out.push_back(trimmed(s.substr(start, i - start)));
            start = i + 1;
        }
    return out;
}

int toInt(const std::string& s, int fallback = 0) {
    int v = fallback;
    const auto r = std::from_chars(s.data(), s.data() + s.size(), v);
    return r.ec == std::errc{} ? v : fallback;
}

template <class F> void forEachRow(const Game& g, const char* name, size_t fields, F&& f) {
    std::ifstream in(g.data.empty() ? std::filesystem::path("data") / name : g.data / name);
    std::string line;
    while (std::getline(in, line)) {
        if (line.size() >= 3 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
        line = trimmed(line);
        if (line.empty() || line.front() == '#') continue;
        const auto p = splitOn(line, '|');
        if (p.size() == fields) f(p);
        else SDL_Log("%s : ligne ignoree (%zu champs, attendu %zu) : %s", name, p.size(), fields, line.c_str());
    }
}

bool listHas(const std::string& list, std::string_view value) {
    for (const auto& v : splitOn(list, ','))
        if (v == value) return true;
    return false;
}

// Objet de coffre « id » ou « id:quantite » (materiaux du remake).
std::pair<std::string, int> chestContent(const std::string& item) {
    const auto colon = item.find(':');
    if (colon == std::string::npos) return {item, 1};
    return {item.substr(0, colon), std::max(1, toInt(item.substr(colon + 1), 1))};
}
// Chemin de sauvegarde ; --savefile le remplace (tests de sauvegarde reelle et de reprise).
std::string& saveOverridePath() {
    static std::string path;
    return path;
}

std::string savePath() {
    if (!saveOverridePath().empty()) return saveOverridePath();
    char* dir = SDL_GetPrefPath("Alundra2Remake", "Alundra2Remake");
    std::string p = dir ? std::string(dir) + "sauvegarde.txt" : "sauvegarde.txt";
    SDL_free(dir);
    return p;
}

std::string chestKey(const Chest& c) { return c.zone + ":" + std::to_string(c.cx) + ":" + std::to_string(c.cy); }

// Case praticable la plus proche de (cx, cy), au meme niveau si possible
void settle(const Map& m, int& cx, int& cy) {
    int best = 1 << 30, bx = cx, by = cy;
    for (int y = cy - 4; y <= cy + 4; ++y)
        for (int x = cx - 4; x <= cx + 4; ++x) {
            const Cell* c = m.cell(x, y);
            if (!c || !canStand(m, x * TILE_W + TILE_W / 2.f, y * TILE_H + TILE_H / 2.f, c->height, 9, 5)) continue;
            const int d = (x - cx) * (x - cx) + (y - cy) * (y - cy);
            if (d < best) { best = d; bx = x; by = y; }
        }
    cx = bx;
    cy = by;
}
} // namespace

// Q : alterner epee, gantelet d'eclair et epee supreme (chacune une fois obtenue).
void cycleWeapon(Game& g) {
    Hero& h = g.hero;
    const bool eclair = g.items.count("gantelet") != 0;
    const bool supreme = g.seenEvents.count("epee_supreme") != 0;
    if (h.weapon == Weapon::Sword) {
        if (eclair) {
            h.weapon = Weapon::Eclair;
            floatText(g, h.x, h.y - 40, "GANTELET D'ECLAIR", SDL_Color{120, 220, 255, 255});
        } else if (supreme) {
            h.weapon = Weapon::Supreme;
            floatText(g, h.x, h.y - 40, "EPEE SUPREME", SDL_Color{255, 240, 150, 255});
        } else {
            floatText(g, h.x, h.y - 40, "Pas d'autre arme", SDL_Color{190, 190, 200, 255});
            return;
        }
    } else if (h.weapon == Weapon::Eclair) {
        if (supreme) {
            h.weapon = Weapon::Supreme;
            floatText(g, h.x, h.y - 40, "EPEE SUPREME", SDL_Color{255, 240, 150, 255});
        } else {
            h.weapon = Weapon::Sword;
            floatText(g, h.x, h.y - 40, "EPEE", SDL_Color{255, 220, 140, 255});
        }
    } else {
        h.weapon = Weapon::Sword;
        floatText(g, h.x, h.y - 40, "EPEE", SDL_Color{255, 220, 140, 255});
    }
    h.charge = 0;
    h.combo = 0;
    playSfx(g, "valider");
}

// Jeehan remet l'epee supreme a ceux qui lui apportent les ordres de Diaz (Alundra 2).
void grantSupreme(Game& g) {
    if (g.seenEvents.count("epee_supreme") || !g.items.count("ordre")) return;
    g.seenEvents.insert("epee_supreme");
    g.items["epee_supreme"] = 1;
    g.hero.weapon = Weapon::Supreme;
    g.hero.charge = 0;
    g.hero.combo = 0;
    floatText(g, g.hero.x, g.hero.y - 52, "Q : changer d'arme", SDL_Color{255, 235, 150, 255});
    playJingle(g, "objet");
    saveGame(g);
}

void spawnChests(Game& g, bool announce) {
    const std::string zone = zoneName(g);
    for (const auto& d : g.chestDefs) {
        if (d.zone != zone) continue;
        if (d.need == "ogre" && !g.bossDown) continue;
        if (!d.need.empty() && d.need != "ogre" && !g.items.count(d.need) && !g.seenEvents.count(d.need)) continue;
        Chest c;
        c.zone = d.zone;
        c.item = d.item;
        c.cx = d.x;
        c.cy = d.y;
        if (std::any_of(g.chests.begin(), g.chests.end(), [&](const Chest& o) { return chestKey(o) == chestKey(c); })) continue;
        int px = d.x, py = d.y;
        settle(*g.map, px, py);
        c.x = px * TILE_W + TILE_W / 2.f;
        c.y = py * TILE_H + TILE_H / 2.f + 3;
        const Cell* cell = g.map->cell(px, py);
        c.z = cell ? cell->height : 0;
        c.open = g.opened.count(chestKey(c)) > 0;
        if (SpriteBank* b = spriteBank(g, "coffre")) c.anim.play(b->anim(c.open ? 1 : 0, DOWN));
        if (announce) { // le coffre apparait dans un nuage d'etincelles
            burst(g, c.x, c.y, c.z, {255, 230, 140, 255}, 26, 2.2f);
            playSfx(g, "coffre", c.x);
        }
        g.chests.push_back(c);
    }
}

bool chestBlocks(const Game& g, float x, float y) {
    for (const auto& c : g.chests)
        if (std::abs(c.x - x) < 15 && y - c.y > -9 && y - c.y < 8) return true;
    return false;
}

Chest* chestInFront(Game& g) {
    const Hero& h = g.hero;
    const float fx = h.x + dirX(h.dir) * 14, fy = h.y + dirY(h.dir) * 10;
    for (auto& c : g.chests)
        if (!c.open && std::abs(c.z - h.z) <= 1 && std::hypot(c.x - fx, c.y - fy) < 20) return &c;
    return nullptr;
}

// Ouverture, comme dans Alundra 1 (reperee image par image sur une video du jeu) : le heros ne bouge pas ;
// le couvercle s'ouvre dans une lueur doree puis des etincelles (animation d'origine, groupe 2) ; l'objet sort
// du coffre et monte au-dessus de la tete du heros, ou il reste pendant le message.
constexpr int OPEN_TICKS = 62, RISE_TICKS = 55;

void openChest(Game& g, Chest& c) {
    c.open = true;
    g.opened.insert(c.zone + ":" + std::to_string(c.cx) + ":" + std::to_string(c.cy));
    if (SpriteBank* b = spriteBank(g, "coffre")) c.anim.play(b->anim(2, DOWN), true);
    c.openTicks = OPEN_TICKS;
    playSfx(g, "coffre", c.x);

    Hero& h = g.hero;
    // « id:n » : plusieurs unites d'un materiau du remake dans un meme coffre.
    const auto [itemId, count] = chestContent(c.item);
    g.items[itemId] += count;
    // Recuperer un objet d'histoire franchit une etape : les PNJ changent de dialogue.
    g.seenEvents.insert("objet_" + itemId);
    if (c.item == "coeur") { // fiole de vie : un coeur de plus, vie rendue
        h.maxHp += 2;
        h.hp = h.maxHp;
    }
    if (c.item == "gantelet") { // gantelet d'eclair d'Alundra 2 : equipee des que trouvee
        h.weapon = Weapon::Eclair;
        h.charge = 0;
        floatText(g, h.x, h.y - 52, "Q : changer d'arme", SDL_Color{120, 220, 255, 255});
    }
    g.gotItem = itemId;
    g.gotTicks = OPEN_TICKS + RISE_TICKS;
    g.gotShown = false;
    g.gotX = c.x;
    g.gotY = c.y - c.z * TILE_H - 12;
    h.attacking = false;
    h.anim.play(g.heroSprites.anim(SpriteBank::IDLE, h.dir));
}

// Position de l'objet obtenu (coordonnees monde, hauteur deja retranchee) ; faux s'il n'y a rien a montrer
bool gotItemPos(const Game& g, float& x, float& y) {
    if (g.gotItem.empty() || g.gotTicks > RISE_TICKS) return false;
    const Hero& h = g.hero;
    float p = 1.f - g.gotTicks / float(RISE_TICKS);
    p = 1.f - (1.f - p) * (1.f - p); // ralentit en arrivant
    const float tx = h.x + (g.gotX - h.x) * 0.08f, ty = h.y - h.z * TILE_H - 50;
    x = g.gotX + (tx - g.gotX) * p;
    y = g.gotY + (ty - g.gotY) * p + (g.gotTicks == 0 ? std::sin(g.frame * 0.08f) : 0.f);
    return true;
}

// Coffre en cours d'ouverture : le jeu attend ; ensuite le message s'ouvre, et l'objet disparait a sa fermeture
bool updateItems(Game& g) {
    for (auto& c : g.chests) {
        c.anim.update();
        if (c.openTicks > 0 && --c.openTicks == 0)
            if (SpriteBank* b = spriteBank(g, "coffre")) c.anim.play(b->anim(1, DOWN));
    }
    if (g.gotItem.empty()) return false;
    if (g.gotShown) { // le message vient d'etre ferme
        g.gotItem.clear();
        return false;
    }
    g.hero.anim.update();
    if (g.gotTicks == RISE_TICKS) playJingle(g, "objet");
    if (g.gotTicks > 0 && --g.gotTicks > 0) return true;
    burst(g, g.hero.x, g.hero.y - 34, g.hero.z, {255, 240, 170, 255}, 12, 1.3f);
    openDialogue(g, "objet_" + g.gotItem);
    g.gotShown = true;
    saveGame(g);
    return true;
}

// Herbe : rend 2 coeurs (comme les herbes medicinales d'Alundra)
void useHerb(Game& g) {
    Hero& h = g.hero;
    auto it = g.items.find("herbe");
    if (it == g.items.end() || it->second <= 0 || h.hp >= h.maxHp || g.mode != Mode::Play) return;
    --it->second;
    h.hp = std::min(h.maxHp, h.hp + 4);
    playSfx(g, "coeur");
    burst(g, h.x, h.y - 16, h.z, {120, 230, 120, 255}, 14, 1.4f);
    floatText(g, h.x, h.y - 46, "+2 coeurs", {140, 240, 140, 255});
}

// Objets A2 : les donnees viennent de data/items.txt, la logique reste ici.
// Un objet cle n'est JAMAIS consomme ; un objet inutilisable ne modifie rien.
bool useItem(Game& g, const std::string& id) {
    if (g.mode != Mode::Play || id.empty()) return false;
    const auto def = g.itemDefs.find(id);
    if (def == g.itemDefs.end()) return false;
    const ItemDef& d = def->second;
    if (d.kind == ItemKind::Weapon) return equipWeapon(g, id);
    if (d.kind == ItemKind::Ring || d.kind == ItemKind::Pocket) {
        if (!g.items.count(id) || g.items.at(id) <= 0) return false;
        if (d.kind == ItemKind::Ring) g.inventory.ring = id;
        else g.inventory.pocket = id;
        playSfx(g, "objet");
        return true;
    }
    if (d.kind == ItemKind::Key) return false; // objet d'histoire : jamais consomme
    if (d.kind != ItemKind::Consumable || !d.consumable) return false;

    const auto it = g.items.find(id);
    if (it == g.items.end() || it->second <= 0) return false;
    Hero& h = g.hero;
    int heal = 0, energy = 0;
    bool full = false;
    if (id == "herb") heal = 4;          // 100 PV = 4 demi-coeurs (50 PV / coeur)
    else if (id == "tonic") heal = 12;   // 300 PV
    else if (id == "elixir") heal = 24;  // 600 PV
    else if (id == "lorien_nuts") energy = 200;
    else if (id == "grapes_of_eru") energy = 500;
    else if (id == "panacea") full = true;
    if (!full && h.hp >= h.maxHp && h.ep >= h.maxEp) return false; // rien a restaurer

    if (full) { h.hp = h.maxHp; h.ep = h.maxEp; }
    else {
        h.hp = std::min(h.maxHp, h.hp + heal);
        h.ep = std::min(h.maxEp, h.ep + energy);
    }
    if (--it->second <= 0) g.items.erase(it);
    playSfx(g, "coeur");
    burst(g, h.x, h.y - 16, h.z, {120, 230, 120, 255}, 14, 1.4f);
    floatText(g, h.x, h.y - 46, "+" + std::to_string(heal / 2) + " coeurs", {140, 240, 140, 255});
    return true;
}

bool equipWeapon(Game& g, const std::string& id) {
    const auto def = g.itemDefs.find(id);
    if (def == g.itemDefs.end() || def->second.kind != ItemKind::Weapon) return false;
    if (!g.items.count(id) || g.items.at(id) <= 0) return false;
    g.inventory.weapon = id;
    g.hero.swordPower = std::clamp(def->second.power, 1, 6);
    // Armes du remake : puissance affichee = degats effectifs (amelioration comprise).
    // Le combat lit directement leurs statistiques (equippedRemakeWeapon).
    if (const RemakeWeaponDef* w = equippedRemakeWeapon(g)) g.hero.swordPower = std::clamp(w->damage, 1, 6);
    // Les deux etats historiques (supreme, gantelet) restent piloter par les gates :
    // changer d'epee A2 ne debloque ni n'efface ces evenements.
    g.hero.weapon = Weapon::Sword;
    playSfx(g, "objet");
    return true;
}

// --- Ajouts du remake (tranche 3) ---------------------------------------------------------
void loadRemakeData(Game& g) {
    g.rmWeapons.clear();
    g.forgeRecipes.clear();
    g.interactionDefs.clear();
    forEachRow(g, "rm_weapons.txt", 9, [&](const std::vector<std::string>& p) {
        RemakeWeaponDef w;
        w.id = p[0];
        w.name = p[1];
        w.description = p[2];
        w.damage = toInt(p[3]);
        w.speed = toInt(p[4], 14);
        w.range = toInt(p[5], 18);
        w.recoil = toInt(p[6], 2);
        w.epCost = toInt(p[7]);
        w.effect = p[8];
        g.rmWeapons[w.id] = std::move(w);
    });
    forEachRow(g, "rm_forge.txt", 13, [&](const std::vector<std::string>& p) {
        ForgeRecipe r;
        r.weaponId = p[0];
        r.branch = p[1];
        r.rank = toInt(p[2], 1);
        r.id = r.weaponId + "_" + r.branch + "_" + std::to_string(r.rank);
        r.name = p[3];
        r.description = p[4];
        for (const auto& chunk : splitOn(p[5], ',')) {
            const auto [mid, qty] = chestContent(chunk);
            if (!mid.empty()) r.cost[mid] += qty;
        }
        if (const auto base = g.rmWeapons.find(r.weaponId); base != g.rmWeapons.end()) r.result = base->second;
        r.result.id = r.weaponId;
        r.result.damage = toInt(p[6]);
        r.result.speed = toInt(p[7], 14);
        r.result.range = toInt(p[8], 18);
        r.result.recoil = toInt(p[9], 2);
        r.result.epCost = toInt(p[10]);
        r.result.effect = p[11];
        r.drawback = p[12];
        g.forgeRecipes.push_back(std::move(r));
    });
    forEachRow(g, "rm_interactions.txt", 8, [&](const std::vector<std::string>& p) {
        InteractionDef d;
        d.zone = p[0];
        d.x = toInt(p[1]);
        d.y = toInt(p[2]);
        d.id = p[3];
        d.type = p[4];
        d.solutions = p[5];
        d.reset = p[6];
        d.result = p[7];
        g.interactionDefs[d.id] = std::move(d);
    });
}

const RemakeWeaponDef* remakeWeapon(const Game& g, const std::string& id) {
    const auto it = g.rmWeapons.find(id);
    return it == g.rmWeapons.end() ? nullptr : &it->second;
}

const ForgeRecipe* forgeRecipe(const Game& g, const std::string& id) {
    for (const auto& r : g.forgeRecipes)
        if (r.id == id) return &r;
    return nullptr;
}

// Statistiques effectives de l'arme du remake equipee : celles de la branche
// forgee si elle existe (une seule branche par arme), sinon celles de base.
const RemakeWeaponDef* equippedRemakeWeapon(const Game& g) {
    const RemakeWeaponDef* base = remakeWeapon(g, g.inventory.weapon);
    if (!base) return nullptr;
    for (const auto& r : g.forgeRecipes)
        if (r.weaponId == base->id && g.upgrades.count(r.id)) return &r.result;
    return base;
}

int materialCount(const Game& g, const std::string& id) {
    const auto it = g.items.find(id);
    return it == g.items.end() ? 0 : std::max(0, it->second);
}

std::string forgeState(const Game& g, const ForgeRecipe& r) {
    if (g.upgrades.count(r.id)) return "done";
    for (const auto& other : g.forgeRecipes)
        if (other.weaponId == r.weaponId && other.id != r.id && g.upgrades.count(other.id)) return "locked";
    if (materialCount(g, r.weaponId) <= 0) return "noweapon";
    for (const auto& [mid, qty] : r.cost)
        if (materialCount(g, mid) < qty) return "missing";
    return "available";
}

// Transaction atomique : TOUTES les conditions sont verifiees avant la moindre
// mutation ; un echec ne modifie rien. Une recette forgee ne peut pas l'etre deux
// fois (double validation) et l'autre branche de la meme arme se ferme.
ForgeResult confirmForge(Game& g, const std::string& recipeId) {
    const ForgeRecipe* r = forgeRecipe(g, recipeId);
    if (!r) return ForgeResult::Unknown;
    const std::string state = forgeState(g, *r);
    if (state == "done") return ForgeResult::AlreadyDone;
    if (state == "locked") return ForgeResult::OtherBranch;
    if (state == "noweapon") return ForgeResult::NoWeapon;
    if (state == "missing") return ForgeResult::Missing;
    for (const auto& [mid, qty] : r->cost) {
        auto it = g.items.find(mid);
        it->second -= qty;
        if (it->second <= 0) g.items.erase(it);
    }
    g.upgrades.insert(r->id);
    if (g.inventory.weapon == r->weaponId) g.hero.swordPower = std::clamp(r->result.damage, 1, 6);
    return ForgeResult::Ok;
}

void openForge(Game& g) {
    g.menu.open = true;
    g.menu.page = MenuPage::Forge;
    g.menu.cursor = 0;
    g.forgeConfirm.clear();
    g.forgeMessage.clear();
    g.front.prevDx = 0;
    g.front.prevDy = 0;
}

namespace {
SDL_FPoint interactionPos(const InteractionDef& d) {
    return {d.x * float(TILE_W) + TILE_W / 2.f, d.y * float(TILE_H) + TILE_H / 2.f + 3.f};
}
bool isInput(const InteractionDef& d) { return d.type != "mechanism" && d.type != "lever"; }
} // namespace

const InteractionDef* interactionInFront(const Game& g) {
    if (!g.map) return nullptr;
    const Hero& h = g.hero;
    const std::string zone = zoneName(g);
    const float fx = h.x + dirX(h.dir) * 14, fy = h.y + dirY(h.dir) * 10;
    const InteractionDef* best = nullptr;
    float bestD = 20.f;
    for (const auto& [id, d] : g.interactionDefs) {
        if (d.zone != zone) continue;
        const Cell* c = g.map->cell(d.x, d.y);
        if (c && std::abs(c->height - h.z) > 1) continue;
        const SDL_FPoint p = interactionPos(d);
        const float dist = std::hypot(p.x - fx, p.y - fy);
        if (dist < bestD) { bestD = dist; best = &d; }
    }
    return best;
}

// Les objets du remake occupent leur case comme un coffre (le chemin gele, au ras
// du sol, se traverse).
bool interactionBlocks(const Game& g, float x, float y) {
    const std::string zone = zoneName(g);
    for (const auto& [id, d] : g.interactionDefs) {
        if (d.zone != zone || d.type == "ice_surface") continue;
        const SDL_FPoint p = interactionPos(d);
        if (std::abs(p.x - x) < 12 && y - p.y > -8 && y - p.y < 7) return true;
    }
    return false;
}

bool resolveInteraction(Game& g, const InteractionDef& d, const std::string& effect) {
    if (!isInput(d) || effect.empty() || effect == "none") return false;
    if (g.interactionStates.count(d.id) && g.interactionStates.at(d.id) == 1) return false;
    if (!listHas(d.solutions, effect)) return false; // mauvais effet : aucun changement d'etat
    g.interactionStates[d.id] = 1;
    return true;
}

int puzzleProgress(const Game& g, const std::string& gateId) {
    int n = 0;
    for (const auto& [id, d] : g.interactionDefs)
        if (isInput(d) && d.result == gateId && g.interactionStates.count(id) && g.interactionStates.at(id) == 1) ++n;
    return n;
}

// Le levier remet les entrees du mecanisme a zero. L'evenement d'ouverture et le
// coffre deja apparu restent acquis (pas de duplication), et aucun gate de l'histoire
// n'est concerne : seuls les ids de rm_interactions.txt marques « puzzle » changent.
void resetInteractions(Game& g, const std::string& gateId) {
    for (const auto& [id, d] : g.interactionDefs)
        if (isInput(d) && d.result == gateId && d.reset == "puzzle") g.interactionStates.erase(id);
}

namespace {
// Mecanisme « countN » : ouvre son evenement quand N entrees sont resolues.
bool checkMechanism(Game& g, const std::string& gateId) {
    const auto it = g.interactionDefs.find(gateId);
    if (it == g.interactionDefs.end() || it->second.type != "mechanism") return false;
    const InteractionDef& m = it->second;
    const int need = m.solutions.rfind("count", 0) == 0 ? toInt(m.solutions.substr(5), 0) : 0;
    if (need <= 0 || puzzleProgress(g, gateId) < need || g.seenEvents.count(m.result)) return false;
    g.seenEvents.insert(m.result);
    spawnChests(g, true); // la cache apparait dans un nuage d'etincelles
    const SDL_FPoint p = interactionPos(m);
    burst(g, p.x, p.y, g.hero.z, {255, 230, 140, 255}, 30, 2.4f);
    g.shake = std::max(g.shake, 10);
    saveGame(g);
    return true;
}
} // namespace

bool useInteraction(Game& g, const InteractionDef& d) {
    const RemakeWeaponDef* w = g.hero.weapon == Weapon::Sword ? equippedRemakeWeapon(g) : nullptr;
    const std::string effect = w ? w->effect : "none";
    g.lastEffect = effect;
    const SDL_FPoint p = interactionPos(d);
    if (d.type == "lever") {
        resetInteractions(g, d.result);
        playSfx(g, "valider", p.x);
        openDialogue(g, "rm_mecanisme_reinit");
        return true;
    }
    if (d.type == "mechanism") {
        const int need = d.solutions.rfind("count", 0) == 0 ? toInt(d.solutions.substr(5), 0) : 0;
        if (g.seenEvents.count(d.result)) openDialogue(g, "rm_mecanisme_ouvert");
        else floatText(g, p.x, p.y - 30, std::to_string(puzzleProgress(g, d.result)) + " / " + std::to_string(need),
                       SDL_Color{240, 230, 180, 255});
        return true;
    }
    if (g.interactionStates.count(d.id) && g.interactionStates.at(d.id) == 1) return true; // deja resolu
    if (!resolveInteraction(g, d, effect)) {
        playSfx(g, "bloque", p.x);
        openDialogue(g, "rm_interaction_sans_effet");
        return true;
    }
    applyImpact(g, p.x, p.y, g.hero.z, effect, 0);
    if (checkMechanism(g, d.result)) {
        openDialogue(g, "rm_mecanisme_ouvert");
        return true;
    }
    openDialogue(g, d.type == "flammable" ? "rm_brasero_allume" : d.type == "ice_surface" ? "rm_chemin_gele"
                                                                                          : "rm_pilier_brise");
    return true;
}

// Coup charge elementaire (applyImpact en zone) : les objets du remake dans le rayon
// recoivent l'effet ; le mecanisme s'ouvre si assez d'entrees sont resolues.
void impactInteractions(Game& g, float x, float y, float radius, const std::string& effect) {
    const std::string zone = zoneName(g);
    std::string message;
    for (const auto& [id, d] : g.interactionDefs) {
        if (d.zone != zone || !isInput(d)) continue;
        const SDL_FPoint p = interactionPos(d);
        if (std::hypot(p.x - x, p.y - y) > radius + 12.f) continue;
        if (!resolveInteraction(g, d, effect)) continue;
        if (checkMechanism(g, d.result)) message = "rm_mecanisme_ouvert";
        else if (message.empty())
            message = d.type == "flammable" ? "rm_brasero_allume" : d.type == "ice_surface" ? "rm_chemin_gele"
                                                                                           : "rm_pilier_brise";
    }
    if (!message.empty()) openDialogue(g, message);
}

// Actions de dialogue (dialogues.txt, lignes « ! ») executees a l'ouverture du dialogue :
//   ! prend <objet> <n>   ! donne <objet> <n>   ! evenement <id>   ! sinon <dialogue>
//   ! si <evenement> <dialogue> : ouvre ce dialogue a la place si l'evenement est deja vu
// Transaction atomique : toutes les prises sont verifiees d'abord ; si l'une est impossible,
// rien n'est modifie et le dialogue « sinon » (s'il existe) est ouvert a la place.
std::string runDialogueActions(Game& g, const std::string& id) {
    const auto it = g.dialogueActions.find(id);
    if (it == g.dialogueActions.end()) return {};
    struct Step { std::string verb, arg; int n = 1; };
    std::vector<Step> steps;
    std::string fallback;
    for (const auto& line : it->second) {
        std::istringstream ss(line);
        Step s;
        ss >> s.verb >> s.arg;
        if (!(ss >> s.n)) s.n = 1;
        if (s.verb == "sinon") fallback = s.arg;
        else if (s.verb == "si") { // « si <evenement> <dialogue> » : variante une fois l'evenement vu
            std::string target;
            std::istringstream again(line);
            again >> s.verb >> s.arg >> target;
            if (g.seenEvents.count(s.arg) && !target.empty()) return target;
        } else steps.push_back(s);
    }
    for (const auto& s : steps)
        if (s.verb == "prend" && materialCount(g, s.arg) < s.n) return fallback; // rien n'est modifie
    for (const auto& s : steps) {
        if (s.verb == "prend") {
            auto item = g.items.find(s.arg);
            item->second -= s.n;
            if (item->second <= 0) g.items.erase(item);
        } else if (s.verb == "donne") {
            g.items[s.arg] += s.n;
            g.seenEvents.insert("objet_" + s.arg);
        } else if (s.verb == "evenement") {
            g.seenEvents.insert(s.arg);
        }
    }
    return {};
}

// --- Sauvegarde -------------------------------------------------------------------------
// V4 (tranche 3) : lignes « ameliore <recette> » et « interaction <id> <etat> ».
// V2 et V3 restent lisibles (champs absents = rien de forge, enigme intacte).
constexpr int SAVE_VERSION = 4;

void setSavePath(const std::string& path) { saveOverridePath() = path; }

bool hasSave() {
    std::ifstream in(savePath()), bak(savePath() + ".bak");
    return in.good() || bak.good();
}

void writeSave(std::ostream& out, const Game& g) {
    if (g.maps.empty()) return;
    const Hero& h = g.hero;
    out << "version " << SAVE_VERSION << '\n';
    out << "zone " << g.maps[size_t(g.current)].stem().string() << ' ' << int(h.safeX / TILE_W) << ' ' << int(h.safeY / TILE_H) << ' '
        << int(h.dir) << "\nvie " << h.hp << ' ' << h.maxHp << "\nenergie " << h.ep << ' ' << h.maxEp
        << "\nboss " << (g.bossDown ? 1 : 0) << "\narme " << int(h.weapon) << '\n';
    auto tok = [](const std::string& s) { return s.empty() ? std::string("-") : s; };
    out << "equip " << tok(g.inventory.weapon) << ' ' << tok(g.inventory.pocket) << ' ' << tok(g.inventory.ring) << ' '
        << h.swordPower << '\n';
    for (const auto& [name, n] : g.items) out << "objet " << name << ' ' << n << '\n';
    for (const auto& key : g.opened) out << "coffre " << key << '\n';
    for (const auto& [zone, done] : g.cleared)
        if (done) out << "liberee " << zone << '\n';
    for (const auto& ev : g.seenEvents) out << "evenement " << ev << '\n';
    for (const auto& id : g.upgrades) out << "ameliore " << id << '\n';
    for (const auto& [id, state] : g.interactionStates)
        if (state) out << "interaction " << id << ' ' << state << '\n';
}

// Ecriture atomique : fichier temporaire complet, puis copie de secours de l'ancienne
// sauvegarde (.bak) et remplacement. Une coupure pendant l'ecriture ne peut plus
// laisser un fichier tronque a la place de la derniere sauvegarde valide.
void saveGame(const Game& g) {
    if (g.maps.empty()) return;
    if (!g.autoScript.empty() && g.saveOverride.empty()) return; // tests : seulement avec --savefile
    const std::filesystem::path path = savePath();
    std::filesystem::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        writeSave(out, g);
        out.flush();
        if (!out) {
            SDL_Log("Sauvegarde : ecriture impossible dans %s", tmp.string().c_str());
            return;
        }
    }
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        std::filesystem::path bak = path;
        bak += ".bak";
        std::filesystem::copy_file(path, bak, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) SDL_Log("Sauvegarde : copie de secours impossible (%s)", ec.message().c_str());
    }
    // Windows : un antivirus peut verrouiller brievement le fichier neuf (« Acces refuse ») :
    // quelques tentatives espacees, puis repli par copie ; le .tmp complet reste sinon disponible.
    for (int attempt = 0; attempt < 5; ++attempt) {
        ec.clear();
        std::filesystem::rename(tmp, path, ec);
        if (!ec) return;
        SDL_Delay(20);
    }
    std::error_code copyEc;
    std::filesystem::copy_file(tmp, path, std::filesystem::copy_options::overwrite_existing, copyEc);
    if (copyEc) {
        SDL_Log("Sauvegarde : remplacement impossible (%s / %s)", ec.message().c_str(), copyEc.message().c_str());
        return;
    }
    std::filesystem::remove(tmp, copyEc);
}

bool readSave(std::istream& in, Game& g) {
    if (!in) return false;
    // Lecture complete puis validation AVANT toute modification de l'etat (revue QA) :
    // une sauvegarde refusee ne vide plus l'inventaire en memoire.
    std::vector<std::string> lines;
    for (std::string l; std::getline(in, l);) lines.push_back(l);
    {
        int v = 0;
        bool hasZone = false;
        for (const auto& l : lines) {
            std::istringstream ss(l);
            std::string key;
            ss >> key;
            if (key == "version") ss >> v;
            else if (key == "zone") hasZone = true;
        }
        if (v > SAVE_VERSION) SDL_Log("Sauvegarde version %d refusee (version maximale %d)", v, SAVE_VERSION);
        if (!hasZone || (v != 0 && v < 2) || v > SAVE_VERSION) return false;
    }
    std::string zone;
    int x = -1, y = -1, dir = 0, version = 0;
    Hero& h = g.hero;
    g.items.clear();
    g.opened.clear();
    g.cleared.clear();
    g.seenEvents.clear();
    g.bossDown = false;
    h.weapon = Weapon::Sword;
    h.maxHp = 10;
    h.hp = 10;
    h.ep = 0;
    h.maxEp = 200;
    h.swordPower = 1;
    g.inventory = Inventory{};
    g.upgrades.clear();
    g.interactionStates.clear();
    for (const std::string& line : lines) {
        std::istringstream ss(line);
        std::string key, a;
        int n = 0;
        ss >> key;
        if (key == "version") ss >> version;
        else if (key == "zone") ss >> zone >> x >> y >> dir;
        else if (key == "vie") ss >> h.hp >> h.maxHp;
        else if (key == "energie") ss >> h.ep >> h.maxEp;
        else if (key == "equip") {
            std::string w, p, ring;
            if (ss >> w >> p >> ring >> n) {
                auto untok = [](const std::string& s) { return s == "-" ? std::string() : s; };
                g.inventory.weapon = untok(w);
                g.inventory.pocket = untok(p);
                g.inventory.ring = untok(ring);
                h.swordPower = std::clamp(n, 1, 6);
            }
        }
        else if (key == "boss" && ss >> n) g.bossDown = n != 0;
        else if (key == "arme" && ss >> n)
            h.weapon = n == 2 ? Weapon::Supreme : n == 1 ? Weapon::Eclair : Weapon::Sword;
        else if (key == "objet" && ss >> a >> n) g.items[a] = n;
        else if (key == "coffre" && ss >> a) g.opened.insert(a);
        else if (key == "liberee" && ss >> a) g.cleared[a] = true;
        else if (key == "evenement" && ss >> a) g.seenEvents.insert(a);
        else if (key == "ameliore" && ss >> a) {
            const ForgeRecipe* rec = forgeRecipe(g, a);
            const bool otherBranch = rec && std::any_of(g.upgrades.begin(), g.upgrades.end(), [&](const std::string& u) {
                const ForgeRecipe* o = forgeRecipe(g, u);
                return o && o->weaponId == rec->weaponId;
            });
            if (rec && !otherBranch) g.upgrades.insert(a); // une seule branche par arme, meme si le fichier en cite deux
        }
        else if (key == "interaction" && ss >> a >> n && g.interactionDefs.count(a) && n == 1)
            g.interactionStates[a] = 1;
    }
    if (zone.empty() || (version != 0 && version < 2) || version > SAVE_VERSION)
        return false; // sauvegarde future ou incompatible ; V2 migre sans perte
    h.maxHp = std::clamp(h.maxHp, 10, 40);
    h.hp = std::clamp(h.hp, 2, h.maxHp);
    h.maxEp = std::clamp(h.maxEp, 1, 1000);
    h.ep = std::clamp(h.ep, 0, h.maxEp);
    // Effets d'equipement recalcules depuis les ids restaures (jamais lus tels quels).
    if (const RemakeWeaponDef* w = equippedRemakeWeapon(g)) h.swordPower = std::clamp(w->damage, 1, 6);
    h.invuln = 0;
    g.banner.clear();
    g.chests.clear();
    g.box = DialogueBox{}; // une partie chargee repart sans dialogue en cours
    g.menu.open = false;
    if (!loadZone(g, zone, x, y, Dir(std::clamp(dir, 0, 3)))) return false;
    g.mode = g.seenEvents.count("mephisto_vaincu") ? Mode::Ending : Mode::Play;
    g.modeTimer = 0;
    g.front.map.reset();
    return true;
}

// Reprise : la sauvegarde principale, ou la copie de secours si elle est illisible
// (fichier corrompu ou tronque). readSave ne modifie rien tant qu'un fichier est refuse.
bool loadGame(Game& g) {
    {
        std::ifstream in(savePath());
        if (in && readSave(in, g)) return true;
    }
    std::ifstream bak(savePath() + ".bak");
    if (bak && readSave(bak, g)) {
        SDL_Log("Sauvegarde principale illisible : reprise depuis la copie de secours");
        return true;
    }
    return false;
}
