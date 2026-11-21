// Menu armes / objets / equipement : contenu A2, presentation Alundra 1 NATIVE.
//
// Mise en page mesuree au pixel sur la reference `gp/images.png` (MONTAGE de deux
// ecrans distincts, jamais fusionnes ici) : docs/visuel/a1_menu_layout.md.
//   - ecran 1 : rangee d'armes (6) + grille d'objets 6x3, deux bandeaux de nom,
//     trois compteurs, portrait, panneau de description ;
//   - ecran 2 : equipement (emplacements d'anneaux en losange, deux pieces
//     equipees, rangee d'objets de poche), memes bandeaux/compteurs/portrait.
// Assets natifs (ressources A1 locales, jamais publiées) produits par
// tools/a1_menu_assets.py : assets/sprites/a1_ui.a1s (pierre, chiffres dores,
// compteurs, curseur) et a1_icons.a1s (icones A1). Les objets A2 recoivent l'icone
// A1 d'un objet de meme nature (data/item_icons.txt) ; les ajouts du remake ont des
// icones originales (assets/sprites/rm_icons.a1s). Parchemin : drawA1Window (WIND.TX).
//
// MenuPage::Weapons = curseur dans la rangee d'armes, MenuPage::Items = curseur dans
// la grille (meme ecran 1), MenuPage::Equipment = ecran 2, MenuPage::Forge = forge.

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "game.h"
#include "menu.h"

namespace {

// --- Banques et mise en page mesuree ---------------------------------------------
// Index des images de a1_ui.a1s (assets/ui/a1_menu/banks.txt).
constexpr int kUiStoneC = 5, kUiStoneExt = 10, kUiDigit0 = 18, kUiTimes = 28;
constexpr int kUiCoin = 29, kUiKey = 30, kUiBird = 31, kUiCursor = 32; // 32..35 : TL TR BL BR
constexpr SDL_Color kInk{41, 49, 16, 255}; // encre FONT3 (CLUT 6) = (40,48,16) mesure sur les bandeaux

struct StonePanel {
    int x, y, w, h, phaseY;
    std::vector<int> top, bottom;                 // un cx par colonne de 8 (rangees 0 et 56 de la tuile)
    std::vector<std::pair<int, int>> left, right; // un (cx, cy) par ligne de 8
};

std::vector<std::pair<int, int>> column(int cx, std::initializer_list<int> rows) {
    std::vector<std::pair<int, int>> out;
    for (int r : rows) out.emplace_back(cx, 8 * r);
    return out;
}

template <class T> std::vector<T> concat(std::vector<T> a, const std::vector<T>& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

// Sequences de morceaux de bord relevees une a une (LAYOUT de tools/a1_menu_assets.py).
const std::vector<StonePanel>& screen1Panels() {
    static const std::vector<StonePanel> panels = {
        {8, 16, 168, 48, 0,
         {0, 8, 16, 24, 32, 40, 48, 56, 64, 8, 16, 24, 32, 40, 48, 56, 64, 8, 16, -16, -8},
         {0, 40, 24, 32, 40, 48, 56, 16, 24, 32, 40, 48, 56, 40, 16, 24, 32, 40, 48, 56, 72},
         {{0, 0}, {0, 8}, {0, 32}, {0, 24}, {0, 48}, {0, 56}},
         {{-8, 0}, {-8, 8}, {72, 16}, {72, 32}, {72, 48}, {72, 56}}},
        {8, 64, 168, 104, 8,
         {0, 8, 16, 24, 32, 40, 48, 56, 64, 8, 16, 24, 32, 40, 48, 56, 64, 8, 16, -16, -8},
         {0, 8, 16, 24, 32, 40, 48, 56, 64, 8, 16, 24, 32, 40, 48, 56, 16, 24, 32, 40, 72},
         concat(concat<std::pair<int, int>>({{0, 0}}, column(0, {2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 6})), {{0, 56}}),
         concat(concat<std::pair<int, int>>({{-8, 0}}, column(-8, {2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 6})), {{72, 56}})},
    };
    return panels;
}

const std::vector<StonePanel>& screen2Panels() {
    static const std::vector<StonePanel> panels = {
        {8, 16, 120, 112, 0,
         {0, 8, 16, 24, 32, 40, 48, 56, 64, 8, 16, 24, 32, 40, -8},
         {0, 8, 16, 24, 32, 40, 48, 56, 64, 8, 16, 24, 32, 40, 72},
         column(0, {0, 1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 6, 7}),
         concat(column(-8, {0, 1, 2}), column(72, {2, 3, 4, 5, 2, 1, 2, 3, 4, 5, 7}))},
        {136, 16, 40, 112, 0,
         {0, 8, 16, 24, 72},
         {0, 8, 16, 64, 72},
         column(0, {0, 1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 6, 7}),
         column(72, {0, 1, 1, 5, 3, 4, 5, 6, 1, 2, 3, 4, 5, 7})},
        {8, 128, 168, 40, 0,
         {0, 8, 16, 24, 32, 40, 48, 56, 64, 8, 16, 24, 32, 40, 48, 56, 64, 8, 16, -16, -8},
         {0, 8, 16, 24, 32, 40, 48, 56, 64, 8, 16, 24, 32, 40, 48, 56, 64, 8, 16, 64, 72},
         {{0, 0}, {0, 8}, {0, 16}, {0, 24}, {0, 56}},
         {{-8, 0}, {-8, 8}, {-8, 16}, {-8, 24}, {72, 56}}},
    };
    return panels;
}

constexpr SDL_FRect kBanner1{176, 16, 144, 32}, kBanner2{176, 64, 144, 32};
constexpr SDL_FRect kDescription{16, 168, 288, 56};
constexpr float kPortraitX = 248, kPortraitY = 104;
constexpr int kCounterY[3] = {100, 124, 148};
constexpr float kRingSlots[7][2] = {{16, 36}, {56, 20}, {96, 36}, {56, 60}, {16, 84}, {96, 84}, {56, 100}};
constexpr float kEquipSlots[2][2] = {{144, 40}, {144, 80}};
constexpr size_t kWeaponCells = 6, kGridCols = 6, kGridRows = 3, kPocketCells = 5;

// --- Lecture des donnees ------------------------------------------------------------
std::string trim(std::string s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

ItemKind parseKind(const std::string& s) {
    if (s == "Consumable") return ItemKind::Consumable;
    if (s == "Weapon") return ItemKind::Weapon;
    if (s == "Ring") return ItemKind::Ring;
    if (s == "Pocket") return ItemKind::Pocket;
    return ItemKind::Key;
}

int parseNumber(const std::string& s, int fallback = 0) {
    int value = fallback;
    const auto result = std::from_chars(s.data(), s.data() + s.size(), value);
    return result.ec == std::errc{} ? value : fallback;
}

// Icone d'un objet : banque + image, resolue depuis data/item_icons.txt et
// assets/ui/a1_menu/banks.txt (noms stables des icones A1 extraites).
struct IconRef {
    std::string bank;
    int frame = -1;
};

const std::map<std::string, IconRef>& iconTable(const Game& g) {
    static std::map<std::string, IconRef> table;
    static bool loaded = false;
    if (loaded) return table;
    loaded = true;
    std::map<std::string, int> a1Index;
    std::ifstream banks(g.assets / "ui" / "a1_menu" / "banks.txt");
    std::string line;
    while (std::getline(banks, line)) {
        std::stringstream ss(line);
        std::string bank, index, name;
        if (std::getline(ss, bank, '|') && std::getline(ss, index, '|') && std::getline(ss, name, '|') &&
            bank == "a1_icons")
            a1Index[trim(name)] = parseNumber(trim(index), -1);
    }
    std::ifstream in(g.data.empty() ? std::filesystem::path("data/item_icons.txt") : g.data / "item_icons.txt");
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line.front() == '#') continue;
        const auto bar = line.find('|');
        if (bar == std::string::npos) continue;
        const std::string id = trim(line.substr(0, bar)), icon = trim(line.substr(bar + 1));
        if (icon.rfind("rm:", 0) == 0) table[id] = {"rm_icons", parseNumber(icon.substr(3), -1)};
        else if (const auto it = a1Index.find(icon); it != a1Index.end()) table[id] = {"a1_icons", it->second};
    }
    return table;
}

// --- Primitives de dessin -------------------------------------------------------------
void blit(SDL_Renderer* r, const SpriteBank::Frame* f, float sx, float sy, float w, float h, float dx, float dy) {
    if (!f || !f->tex || w <= 0 || h <= 0) return;
    const SDL_FRect src{sx, sy, w, h};
    const SDL_FRect dst{dx, dy, w, h};
    SDL_SetTextureColorMod(f->tex, 255, 255, 255);
    SDL_SetTextureAlphaMod(f->tex, 255);
    SDL_SetTextureBlendMode(f->tex, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(r, f->tex, &src, &dst);
}

// Panneau pierre : interieur repete (decalage vertical phaseY), bords en morceaux 8x8
// releves un par un ; les joints restent transparents (la carte se voit dessous).
void drawStone(SDL_Renderer* r, SpriteBank& ui, const StonePanel& p) {
    const SpriteBank::Frame* center = ui.frame(kUiStoneC);
    const SpriteBank::Frame* ext = ui.frame(kUiStoneExt);
    if (!center || !ext || center->w <= 0 || center->h <= 0) return;
    const int iw = p.w - 16, ih = p.h - 16;
    for (int dy = 0, sy = p.phaseY % center->h; dy < ih; sy = 0) {
        const int hh = std::min(center->h - sy, ih - dy);
        for (int dx = 0; dx < iw; dx += center->w)
            blit(r, center, 0, float(sy), float(std::min(center->w, iw - dx)), float(hh), float(p.x + 8 + dx),
                 float(p.y + 8 + dy));
        dy += hh;
    }
    auto piece = [&](int cx, int cy, int dx, int dy) {
        blit(r, ext, float(cx + 16), float(cy), 8, 8, float(dx), float(dy));
    };
    for (size_t k = 0; k < p.top.size(); ++k) piece(p.top[k], 0, p.x + 8 * int(k), p.y);
    for (size_t k = 0; k < p.bottom.size(); ++k) piece(p.bottom[k], 56, p.x + 8 * int(k), p.y + p.h - 8);
    for (size_t k = 0; k < p.left.size(); ++k) piece(p.left[k].first, p.left[k].second, p.x, p.y + 8 * int(k));
    for (size_t k = 0; k < p.right.size(); ++k)
        piece(p.right[k].first, p.right[k].second, p.x + p.w - 8, p.y + 8 * int(k));
}

void drawIcon(Game& g, const std::string& id, float x, float y) {
    if (id.empty()) return;
    const auto& table = iconTable(g);
    const auto it = table.find(id);
    if (it == table.end()) return;
    if (SpriteBank* b = spriteBank(g, it->second.bank)) b->drawFrame(g.renderer, it->second.frame, x, y);
}

void drawCursor(SpriteBank& ui, SDL_Renderer* r, float x, float y) {
    constexpr float off[4][2] = {{1, 1}, {18, 1}, {1, 26}, {18, 26}}; // TL TR BL BR / origine de l'icone
    for (int k = 0; k < 4; ++k) ui.drawFrame(r, kUiCursor + k, x + off[k][0], y + off[k][1]);
}

// Chiffres dores 8x16, alignes a droite sur `width` colonnes ; `pad` : zeros de remplissage.
void drawDigits(SpriteBank& ui, SDL_Renderer* r, float rightX, float y, int value, int width, bool pad) {
    std::string s = std::to_string(std::max(0, value));
    if (int(s.size()) > width) s = std::string(size_t(width), '9');
    if (pad && int(s.size()) < width) s = std::string(size_t(width) - s.size(), '0') + s;
    float x = rightX - 8.f * float(s.size());
    for (char c : s) {
        ui.drawFrame(r, kUiDigit0 + (c - '0'), x, y);
        x += 8.f;
    }
}

int quantity(const Game& g, const std::string& id) { return g.items.count(id) ? g.items.at(id) : 0; }

void drawCounters(Game& g, SpriteBank& ui) {
    SDL_Renderer* r = g.renderer;
    const int gold = quantity(g, "or"); // aucune economie dans le remake pour l'instant (0)
    const int keys = quantity(g, "strange_key") + quantity(g, "cle");
    const int icons[3] = {kUiCoin, kUiBird, kUiKey};
    // Ligne du milieu : energie EP d'A2 (icone native de cette ligne ; correspondance documentee).
    const int values[3] = {gold, g.hero.ep, keys};
    const int widths[3] = {4, 3, 2};
    const bool pads[3] = {true, false, true};
    for (int k = 0; k < 3; ++k) {
        const float y = float(kCounterY[k]);
        ui.drawFrame(r, icons[k], 176, y);
        ui.drawFrame(r, kUiTimes, 192, y + 3);
        drawDigits(ui, r, 232, y, values[k], widths[k], pads[k]);
    }
}

std::string itemName(const Game& g, const std::string& id) {
    const auto it = g.itemDefs.find(id);
    return it == g.itemDefs.end() ? id : it->second.name;
}

void bannerText(Game& g, const SDL_FRect& banner, const std::string& text) {
    if (text.empty()) return;
    SDL_SetRenderDrawColor(g.renderer, kInk.r, kInk.g, kInk.b, 255);
    drawText(g.renderer, banner.x + 16, banner.y + 7, text); // capitales y+9..+20 (mesure)
}

std::vector<std::string> wrapWidth(const std::string& text, float maxW) {
    std::vector<std::string> lines;
    std::string current, word;
    std::istringstream words(text);
    while (words >> word) {
        const std::string candidate = current.empty() ? word : current + " " + word;
        if (!current.empty() && textWidthLatin1(foldAccents(candidate)) > maxW) {
            lines.push_back(current);
            current = word;
        } else {
            current = candidate;
        }
    }
    if (!current.empty()) lines.push_back(current);
    return lines;
}

void descriptionText(Game& g, const std::string& id) {
    const auto it = g.itemDefs.find(id);
    if (it == g.itemDefs.end()) return;
    std::string text = it->second.description;
    if (const RemakeWeaponDef* w = remakeWeapon(g, id)) {
        // Arme du remake : statistiques effectives (amelioration forgee comprise si equipee).
        const RemakeWeaponDef* eff = g.inventory.weapon == id ? equippedRemakeWeapon(g) : w;
        text += " Dégâts " + std::to_string(eff->damage) + ", portée " + std::to_string(eff->range) + ".";
    } else if (it->second.kind == ItemKind::Weapon && it->second.power > 0) {
        text += " Puissance " + std::to_string(it->second.power) + ".";
    }
    SDL_SetRenderDrawColor(g.renderer, kInk.r, kInk.g, kInk.b, 255);
    const auto lines = wrapWidth(text, kDescription.w - 56);
    for (size_t i = 0; i < lines.size() && i < 3; ++i)
        drawText(g.renderer, kDescription.x + 16, kDescription.y + 4 + 16.f * float(i), lines[i]);
}

std::vector<std::string> weaponIds(const Game& g) {
    std::vector<std::string> ids;
    for (const auto& [id, def] : g.itemDefs)
        if (def.kind == ItemKind::Weapon && (id == "long_sword" || quantity(g, id) > 0)) ids.push_back(id);
    std::sort(ids.begin(), ids.end(), [&g](const std::string& a, const std::string& b) {
        const int pa = g.itemDefs.at(a).power, pb = g.itemDefs.at(b).power;
        return pa != pb ? pa < pb : a < b;
    });
    return ids;
}

std::vector<std::string> gridIds(const Game& g) {
    std::vector<std::string> ids;
    for (const auto& [id, def] : g.itemDefs)
        if ((def.kind == ItemKind::Consumable || def.kind == ItemKind::Key) && quantity(g, id) > 0) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    return ids;
}

// Ecran 2 : anneaux possedes (emplacements en losange) puis objets de poche (rangee basse).
std::vector<std::string> equipmentIds(const Game& g) {
    std::vector<std::string> rings, pockets;
    for (const auto& [id, def] : g.itemDefs) {
        if (quantity(g, id) <= 0) continue;
        if (def.kind == ItemKind::Ring && rings.size() < 7) rings.push_back(id);
        if (def.kind == ItemKind::Pocket && pockets.size() < kPocketCells) pockets.push_back(id);
    }
    rings.resize(7); // emplacements fixes : une case vide garde un identifiant vide
    rings.insert(rings.end(), pockets.begin(), pockets.end());
    return rings;
}

size_t equippedWeaponIndex(const Game& g, const std::vector<std::string>& weapons) {
    const auto it = std::find(weapons.begin(), weapons.end(), g.inventory.weapon);
    return it == weapons.end() ? 0 : size_t(it - weapons.begin());
}

// --- Ecrans ---------------------------------------------------------------------------
void drawScreen1(Game& g, SpriteBank& ui) {
    SDL_Renderer* r = g.renderer;
    for (const auto& p : screen1Panels()) drawStone(r, ui, p);
    const auto weapons = weaponIds(g);
    const auto items = gridIds(g);
    const bool inWeapons = g.menu.page == MenuPage::Weapons;

    // Rangee d'armes : fenetre de 6 autour du curseur ; un curseur reste sur l'arme equipee.
    const size_t focusW = inWeapons ? g.menu.cursor : equippedWeaponIndex(g, weapons);
    const size_t firstW = focusW >= kWeaponCells && focusW < weapons.size() ? focusW - kWeaponCells + 1 : 0;
    for (size_t i = 0; i < kWeaponCells && firstW + i < weapons.size(); ++i)
        drawIcon(g, weapons[firstW + i], 16.f + 24.f * float(i), 24.f);
    if (focusW < weapons.size()) drawCursor(ui, r, 16.f + 24.f * float(focusW - firstW), 24.f);

    // Grille d'objets 6x3, defilement par rangee ; quantite (dernier chiffre en +16,+16).
    const size_t focusI = inWeapons ? g.menu.gridCursor : g.menu.cursor;
    const size_t row = focusI / kGridCols;
    const size_t firstRow = row >= kGridRows ? row - kGridRows + 1 : 0;
    for (size_t k = 0; k < kGridCols * kGridRows; ++k) {
        const size_t index = firstRow * kGridCols + k;
        if (index >= items.size()) break;
        const float x = 16.f + 24.f * float(k % kGridCols), y = 72.f + 28.f * float(k / kGridCols);
        drawIcon(g, items[index], x, y);
        const auto def = g.itemDefs.find(items[index]);
        if (def != g.itemDefs.end() && def->second.maxQty != 1)
            drawDigits(ui, r, x + 24, y + 16, quantity(g, items[index]), 2, false);
    }
    if (focusI < items.size())
        drawCursor(ui, r, 16.f + 24.f * float(focusI % kGridCols), 72.f + 28.f * float(focusI / kGridCols - firstRow));

    drawA1Window(r, kBanner1.x, kBanner1.y, kBanner1.w, kBanner1.h);
    drawA1Window(r, kBanner2.x, kBanner2.y, kBanner2.w, kBanner2.h);
    if (focusW < weapons.size()) bannerText(g, kBanner1, itemName(g, weapons[focusW]));
    if (!items.empty()) bannerText(g, kBanner2, itemName(g, items[std::min(focusI, items.size() - 1)]));

    drawA1Window(r, kDescription.x, kDescription.y, kDescription.w, kDescription.h);
    if (inWeapons && focusW < weapons.size()) descriptionText(g, weapons[focusW]);
    else if (!inWeapons && focusI < items.size()) descriptionText(g, items[focusI]);
}

void drawScreen2(Game& g, SpriteBank& ui) {
    SDL_Renderer* r = g.renderer;
    for (const auto& p : screen2Panels()) drawStone(r, ui, p);
    const auto ids = equipmentIds(g);
    for (size_t k = 0; k < ids.size(); ++k) {
        if (k < 7) drawIcon(g, ids[k], kRingSlots[k][0], kRingSlots[k][1]);
        else drawIcon(g, ids[k], 16.f + 32.f * float(k - 7), 132.f);
    }
    // Pieces equipees (anneau, objet de poche) : deux curseurs permanents, comme la reference.
    const std::string equipped[2] = {g.inventory.ring, g.inventory.pocket};
    for (int k = 0; k < 2; ++k) {
        drawIcon(g, equipped[k], kEquipSlots[k][0], kEquipSlots[k][1]);
        if (!equipped[k].empty()) drawCursor(ui, r, kEquipSlots[k][0], kEquipSlots[k][1]);
    }
    if (g.menu.cursor < ids.size()) {
        const size_t k = g.menu.cursor;
        if (k < 7) drawCursor(ui, r, kRingSlots[k][0], kRingSlots[k][1]);
        else drawCursor(ui, r, 16.f + 32.f * float(k - 7), 132.f);
    }
    drawA1Window(r, kBanner1.x, kBanner1.y, kBanner1.w, kBanner1.h);
    drawA1Window(r, kBanner2.x, kBanner2.y, kBanner2.w, kBanner2.h);
    const std::string hovered = g.menu.cursor < ids.size() ? ids[g.menu.cursor] : std::string();
    const std::string ringName = g.menu.cursor < 7 && !hovered.empty() ? hovered : equipped[0];
    const std::string pocketName = g.menu.cursor >= 7 && !hovered.empty() ? hovered : equipped[1];
    if (!ringName.empty()) bannerText(g, kBanner1, itemName(g, ringName));
    if (!pocketName.empty()) bannerText(g, kBanner2, itemName(g, pocketName));
    drawA1Window(r, kDescription.x, kDescription.y, kDescription.w, kDescription.h);
    if (g.menu.cursor < ids.size() && !ids[g.menu.cursor].empty()) descriptionText(g, ids[g.menu.cursor]);
}

// --- Page de forge (ajout du remake, tranche 3) ---------------------------------
// Parchemin natif WIND.TX (drawA1Window), encre FONT3 index 6. La page ne modifie
// JAMAIS l'inventaire : la seule mutation passe par confirmForge (main.cpp).
std::string forgeStateLabel(const std::string& s) {
    if (s == "available") return "prête";
    if (s == "missing") return "manque";
    if (s == "done") return "forgée";
    if (s == "locked") return "fermée";
    return "sans arme";
}

void drawForgePage(Game& g) {
    SDL_Renderer* r = g.renderer;
    SDL_SetRenderDrawColor(r, 0, 0, 0, 170); // le decor s'efface derriere la forge
    SDL_RenderFillRect(r, nullptr);

    constexpr float kListY = 4.f, kListH = 112.f, kDetailY = 108.f, kDetailH = 126.f;
    constexpr float kTextX = 40.f, kRight = 284.f;
    drawA1Window(r, 8.f, kListY, 304.f, kListH);
    for (size_t i = 0; i < g.forgeRecipes.size() && i < 6; ++i) {
        const ForgeRecipe& rec = g.forgeRecipes[i];
        const float y = kListY + 8.f + float(i) * 16.f;
        SDL_SetRenderDrawColor(r, kInk.r, kInk.g, kInk.b, 255);
        if (i == g.menu.cursor) drawText(r, kTextX - 10.f, y, ">");
        drawText(r, kTextX, y, rec.name);
        // Recette preparee (premier appui) : l'etat laisse place a la question.
        const std::string label = g.forgeConfirm == rec.id && forgeState(g, rec) == "available"
                                      ? "forger ?" : forgeStateLabel(forgeState(g, rec));
        drawText(r, kRight - textWidthLatin1(foldAccents(label)), y, label);
    }

    drawA1Window(r, 8.f, kDetailY, 304.f, kDetailH);
    if (g.menu.cursor >= g.forgeRecipes.size()) return;
    const ForgeRecipe& rec = g.forgeRecipes[g.menu.cursor];
    std::vector<std::string> lines;
    lines.push_back(itemName(g, rec.weaponId) + " > " + rec.name);
    if (g.forgeMessage.rfind("rm_forge_", 0) == 0) { // reponse d'Orvan apres une tentative
        const auto it = g.dialogues.find(g.forgeMessage);
        if (it != g.dialogues.end() && !it->second.empty())
            for (const auto& l : wrapWidth(it->second.front().text, kRight - kTextX)) lines.push_back(l);
    } else {
        const RemakeWeaponDef* base = remakeWeapon(g, rec.weaponId);
        std::string diff;
        if (base) {
            const std::pair<const char*, std::pair<int, int>> stats[] = {
                {"Dégâts", {base->damage, rec.result.damage}}, {"Portée", {base->range, rec.result.range}},
                {"Recul", {base->recoil, rec.result.recoil}},  {"Lenteur", {base->speed, rec.result.speed}},
                {"EP", {base->epCost, rec.result.epCost}}};
            for (const auto& [label, v] : stats)
                if (v.first != v.second)
                    diff += (diff.empty() ? "" : "   ") + std::string(label) + " " + std::to_string(v.first) + " > " +
                            std::to_string(v.second);
        }
        lines.push_back(diff);
        lines.push_back("Contrepartie : " + rec.drawback);
        for (const auto& [mid, qty] : rec.cost)
            lines.push_back(itemName(g, mid) + " " + std::to_string(materialCount(g, mid)) + "/" + std::to_string(qty));
        const std::string state = forgeState(g, rec);
        if (g.forgeConfirm != rec.id) lines.push_back("Valider : choisir");
        else if (state == "available") lines.push_back("Valider à nouveau : forger");
        else lines.push_back(state == "missing" ? "Il manque des matériaux." : state == "done" ? "Déjà forgée."
                             : state == "locked" ? "L'autre voie est déjà forgée." : "Il faut d'abord cette arme.");
    }
    for (size_t i = 0; i < lines.size() && i < 6; ++i) {
        SDL_SetRenderDrawColor(r, kInk.r, kInk.g, kInk.b, 255);
        drawText(r, kTextX - 8.f, kDetailY + 16.f + float(i) * 16.f, lines[i]);
    }
}

} // namespace

void loadItemDefs(Game& g) {
    g.itemDefs.clear();
    std::ifstream in(g.data.empty() ? std::filesystem::path("data/items.txt") : g.data / "items.txt");
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.front() == '\xEF') { // BOM UTF-8
            if (line.size() >= 3 && line[1] == '\xBB' && line[2] == '\xBF') line.erase(0, 3);
        }
        line = trim(line);
        if (line.empty() || line.front() == '#') continue;
        std::vector<std::string> fields;
        size_t start = 0;
        for (size_t i = 0; i <= line.size(); ++i) {
            if (i == line.size() || line[i] == '|') {
                fields.push_back(trim(line.substr(start, i - start)));
                start = i + 1;
            }
        }
        if (fields.size() < 7 || fields[0].empty()) continue;
        ItemDef def;
        def.id = fields[0];
        def.name = fields[1];
        def.description = fields[2];
        def.kind = parseKind(fields[3]);
        def.maxQty = parseNumber(fields[4]);
        def.consumable = parseNumber(fields[5]) != 0;
        def.power = parseNumber(fields[6]);
        g.itemDefs.emplace(def.id, std::move(def));
    }
}

std::vector<std::string> menuItemIds(const Game& g, MenuPage page) {
    switch (page) {
    case MenuPage::Weapons: return weaponIds(g);
    case MenuPage::Items: return gridIds(g);
    case MenuPage::Equipment: return equipmentIds(g);
    case MenuPage::Forge: {
        std::vector<std::string> ids;
        for (const auto& r : g.forgeRecipes) ids.push_back(r.id);
        return ids;
    }
    }
    return {};
}

bool drawItemIcon(Game& g, const std::string& id, float x, float y) {
    const auto& table = iconTable(g);
    const auto it = table.find(id);
    if (it == table.end()) return false;
    SpriteBank* b = spriteBank(g, it->second.bank);
    if (!b || !b->frame(it->second.frame)) return false;
    b->drawFrame(g.renderer, it->second.frame, x, y);
    return true;
}

bool itemSpriteExists(const Game& g, const std::string& name) {
    std::error_code ec;
    return !name.empty() && std::filesystem::exists(g.assets / "sprites" / (name + ".a1s"), ec);
}

std::string menuSelectedId(const Game& g) {
    const auto ids = menuItemIds(g, g.menu.page);
    return g.menu.cursor < ids.size() ? ids[g.menu.cursor] : std::string{};
}

void openMenu(Game& g) {
    g.menu.open = true;
    g.menu.page = MenuPage::Weapons;
    g.menu.cursor = equippedWeaponIndex(g, weaponIds(g)); // curseur sur l'arme equipee
    g.front.prevDx = 0; // memoire de front, uniquement pendant l'ouverture du menu
    g.front.prevDy = 0;
}

void closeMenu(Game& g) {
    g.menu.open = false;
    g.forgeConfirm.clear(); // fermer la forge annule toute confirmation en attente
    g.forgeMessage.clear();
    g.front.prevDx = 0;
    g.front.prevDy = 0;
}

void updateMenu(Game& g) {
    if (!g.menu.open) return; // aucun effet de bord quand le menu est ferme

    const int dx = int(g.in.dx), dy = int(g.in.dy);
    const int px = int(g.front.prevDx), py = int(g.front.prevDy);
    g.front.prevDx = dx;
    g.front.prevDy = dy;
    g.in.attackPressed = false; // une validation ne doit jamais devenir une attaque

    if (g.in.dashPressed || g.in.menuPressed) {
        g.in.dashPressed = false;
        closeMenu(g);
        return;
    }

    const bool left = dx < 0 && px >= 0, right = dx > 0 && px <= 0;
    const bool up = dy < 0 && py >= 0, down = dy > 0 && py <= 0;
    const std::vector<std::string> ids = menuItemIds(g, g.menu.page);
    size_t& c = g.menu.cursor;

    switch (g.menu.page) {
    case MenuPage::Forge: { // liste verticale, jamais d'autre page
        const size_t before = c;
        if (up && c > 0) --c;
        if (down && c + 1 < ids.size()) ++c;
        if (c != before) { // changer de recette annule la confirmation en cours
            g.forgeConfirm.clear();
            g.forgeMessage.clear();
        }
        return;
    }
    case MenuPage::Weapons: // ecran 1, rangee d'armes
        if (left && c > 0) --c;
        if (right && c + 1 < ids.size()) ++c;
        if (down) { g.menu.page = MenuPage::Items; c = g.menu.gridCursor; }
        else if (up) { g.menu.page = MenuPage::Equipment; c = 0; }
        break;
    case MenuPage::Items: // ecran 1, grille 6 colonnes
        if (left && c > 0) --c;
        if (right && c + 1 < ids.size()) ++c;
        if (up) {
            if (c >= kGridCols) c -= kGridCols;
            else { g.menu.page = MenuPage::Weapons; c = equippedWeaponIndex(g, weaponIds(g)); }
        } else if (down) {
            if (c + kGridCols < ids.size()) c += kGridCols;
            else { g.menu.page = MenuPage::Equipment; c = 0; }
        }
        break;
    case MenuPage::Equipment: // ecran 2 : anneaux puis objets de poche
        if (left && c > 0) --c;
        if (right && c + 1 < ids.size()) ++c;
        if (up) { g.menu.page = MenuPage::Items; c = 0; }
        else if (down) { g.menu.page = MenuPage::Weapons; c = equippedWeaponIndex(g, weaponIds(g)); }
        break;
    }
    const size_t count = std::max<size_t>(menuItemIds(g, g.menu.page).size(), 1);
    if (c >= count) c = count - 1;
    if (g.menu.page == MenuPage::Items) g.menu.gridCursor = c;
}

void drawMenu(Game& g) {
    if (!g.menu.open) return;
    if (g.menu.page == MenuPage::Forge) {
        drawForgePage(g);
        return;
    }
    SpriteBank* ui = spriteBank(g, "a1_ui");
    if (!ui) { // banque absente : le rendu A1 exige tools/a1_menu_assets.py (aucun substitut invente)
        SDL_SetRenderDrawColor(g.renderer, 0, 0, 0, 200);
        SDL_RenderFillRect(g.renderer, nullptr);
        SDL_SetRenderDrawColor(g.renderer, 255, 255, 255, 255);
        drawText(g.renderer, 16, 16, "assets/sprites/a1_ui.a1s absent");
        return;
    }
    if (g.menu.page == MenuPage::Equipment) drawScreen2(g, *ui);
    else drawScreen1(g, *ui);
    drawCounters(g, *ui);
    drawSpeakerPortrait(g, kPortraitX, kPortraitY, "Flint");
}
