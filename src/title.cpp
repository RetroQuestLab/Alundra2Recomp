// Ouverture, ecran titre (menu, options) et prologue.
//
//   Splash   : carton d'ouverture, puis l'ecran titre.
//   Title    : logo dore sur une vraie zone du jeu qui defile lentement ; menu Nouvelle partie / Options / Quitter.
//   Prologue : cartes narratives (dialogue [prologue] de data/dialogues.txt) sur des decors du jeu ou un ciel
//              de nuit ; le "nom" de chaque page choisit le decor : un numero de zone (042), "ciel", "navire"
//              (ciel + navire volant des pirates) ou "chute" ; "decor/Personnage" en fait une replique.
//
// Le logo et le ciel sont generes par le code ; les decors sont les cartes du jeu (aucune image ajoutee).
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>

#include "game.h"

namespace {
constexpr int SPLASH_TICKS = 230, FADE = 50;
constexpr float TYPE_SPEED = 0.55f; // caracteres par tick dans le prologue

// --- Logo ---------------------------------------------------------------------------
// Lettres 12x16 dessinees pour le remake (capitales a empattements), agrandies 2 fois ; le "2" l'est 3 fois.
const char* const GLYPHS[7][16] = {
    {"....XXXX....", "....XXXX....", "...XXXXXX...", "...XX..XX...", "...XX..XX...", "..XXX..XXX..", "..XX....XX..",
     "..XX....XX..", ".XXX....XXX.", ".XXXXXXXXXX.", ".XXXXXXXXXX.", ".XX......XX.", "XXX......XXX", "XX........XX",
     "XX........XX", "XXXX....XXXX"}, // A
    {"XXXXXX......", ".XXXX.......", "..XXX.......", "..XXX.......", "..XXX.......", "..XXX.......", "..XXX.......",
     "..XXX.......", "..XXX.......", "..XXX.......", "..XXX.......", "..XXX.....XX", "..XXX.....XX", "..XXX....XXX",
     ".XXXXXXXXXXX", "XXXXXXXXXXXX"}, // L
    {"XXXXX..XXXXX", ".XXX....XXX.", ".XXX....XX..", ".XXX....XX..", ".XXX....XX..", ".XXX....XX..", ".XXX....XX..",
     ".XXX....XX..", ".XXX....XX..", ".XXX....XX..", ".XXX....XX..", ".XXX....XX..", ".XXX....XX..", "..XXX..XXX..",
     "..XXXXXXXX..", "....XXXX...."}, // U
    {"XXXX...XXXXX", ".XXXX...XXX.", ".XXXX....XX.", ".XXXXX...XX.", ".XX.XXX..XX.", ".XX.XXX..XX.", ".XX..XXX.XX.",
     ".XX..XXX.XX.", ".XX...XXXXX.", ".XX...XXXXX.", ".XX....XXXX.", ".XX....XXXX.", ".XX.....XXX.", ".XX.....XXX.",
     ".XXX.....XX.", "XXXXX....XX."}, // N
    {"XXXXXXXX....", ".XXXXXXXXX..", "..XXX..XXXX.", "..XXX...XXX.", "..XXX....XXX", "..XXX....XXX", "..XXX....XXX",
     "..XXX....XXX", "..XXX....XXX", "..XXX....XXX", "..XXX....XXX", "..XXX....XXX", "..XXX...XXX.", "..XXX..XXXX.",
     ".XXXXXXXXX..", "XXXXXXXX...."}, // D
    {"XXXXXXXXX...", ".XXXXXXXXXX.", "..XXX...XXX.", "..XXX....XXX", "..XXX....XXX", "..XXX...XXX.", "..XXXXXXXX..",
     "..XXXXXXX...", "..XXX.XXX...", "..XXX..XXX..", "..XXX..XXX..", "..XXX...XXX.", "..XXX...XXX.", "..XXX....XXX",
     ".XXXX....XXX", "XXXXXX...XXX"}, // R
    {"..XXXXXXX...", ".XXXXXXXXX..", "XXX.....XXX.", "XX......XXXX", ".........XXX", ".........XXX", "........XXX.",
     ".......XXX..", "......XXX...", ".....XXX....", "....XXX.....", "...XXX......", "..XXX.....XX", ".XXX......XX",
     "XXXXXXXXXXXX", "XXXXXXXXXXXX"}, // 2
};
constexpr int PAD = 5, LETTER = 26, LOGO_W = 7 * LETTER + 6 + 36 + 2 * PAD, LOGO_H = 48 + 2 * PAD;

SDL_Renderer* owner = nullptr;
SDL_Texture* logoTex = nullptr;
SDL_Texture* shineTex = nullptr;

struct Rgb { float r, g, b; };
Rgb mix(Rgb a, Rgb b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }

void buildLogo(SDL_Renderer* r) {
    if (owner == r) return;
    owner = r;
    // Masque : 0 vide, 1 lettre doree, 2 chiffre rouge ; `rel` = position verticale dans le signe (0 haut, 1 bas)
    std::vector<uint8_t> mask(size_t(LOGO_W) * LOGO_H, 0);
    std::vector<float> rel(mask.size(), 0.f);
    auto put = [&](int glyph, int x0, int y0, int scale, uint8_t kind) {
        for (int j = 0; j < 16 * scale; ++j)
            for (int i = 0; i < 12 * scale; ++i)
                if (GLYPHS[glyph][j / scale][i / scale] == 'X') {
                    const size_t p = size_t(y0 + j) * LOGO_W + x0 + i;
                    mask[p] = kind;
                    rel[p] = j / float(16 * scale - 1);
                }
    };
    const int order[7] = {0, 1, 2, 3, 4, 5, 0}; // A L U N D R A
    for (int k = 0; k < 7; ++k) put(order[k], PAD + k * LETTER, PAD + 16, 2, 1);
    put(6, PAD + 7 * LETTER + 6, PAD, 3, 2);

    auto at = [&](int x, int y) -> uint8_t {
        return (x < 0 || y < 0 || x >= LOGO_W || y >= LOGO_H) ? 0 : mask[size_t(y) * LOGO_W + x];
    };
    auto near = [&](int x, int y, int d) {
        for (int j = -d; j <= d; ++j)
            for (int i = -d; i <= d; ++i)
                if (at(x + i, y + j)) return true;
        return false;
    };
    SDL_Surface* logo = SDL_CreateSurface(LOGO_W, LOGO_H, SDL_PIXELFORMAT_RGBA32);
    SDL_Surface* shine = SDL_CreateSurface(LOGO_W, LOGO_H, SDL_PIXELFORMAT_RGBA32);
    for (int y = 0; y < LOGO_H; ++y) {
        auto* row = reinterpret_cast<Uint8*>(logo->pixels) + y * logo->pitch;
        auto* srow = reinterpret_cast<Uint8*>(shine->pixels) + y * shine->pitch;
        for (int x = 0; x < LOGO_W; ++x) {
            Uint8* p = row + x * 4;
            Uint8* s = srow + x * 4;
            const uint8_t kind = at(x, y);
            s[0] = s[1] = s[2] = 255;
            s[3] = kind ? 255 : 0;
            if (kind) { // metal poli : clair en haut, cassure sombre au milieu, reflet en bas
                const float t = rel[size_t(y) * LOGO_W + x];
                Rgb c;
                if (kind == 1)
                    c = t < 0.52f ? mix({255, 246, 186}, {246, 178, 48}, t / 0.52f)
                                  : mix({188, 104, 22}, {252, 204, 96}, (t - 0.52f) / 0.48f);
                else
                    c = t < 0.52f ? mix({255, 186, 160}, {226, 54, 56}, t / 0.52f)
                                  : mix({140, 14, 30}, {238, 96, 84}, (t - 0.52f) / 0.48f);
                if (!at(x, y - 1) || !at(x - 1, y)) c = mix(c, {255, 255, 240}, 0.55f); // arete eclairee
                if (!at(x, y + 1) || !at(x + 1, y)) c = mix(c, {60, 20, 8}, 0.35f);      // arete dans l'ombre
                p[0] = Uint8(c.r); p[1] = Uint8(c.g); p[2] = Uint8(c.b); p[3] = 255;
            } else if (near(x, y, 1)) { // contour
                p[0] = 44; p[1] = 16; p[2] = 10; p[3] = 255;
            } else if (near(x, y, 2)) {
                p[0] = 16; p[1] = 6; p[2] = 10; p[3] = 235;
            } else if (near(x - 3, y - 3, 2)) { // ombre portee
                p[0] = p[1] = 0; p[2] = 8; p[3] = 120;
            } else {
                p[0] = p[1] = p[2] = p[3] = 0;
            }
        }
    }
    logoTex = SDL_CreateTextureFromSurface(r, logo);
    shineTex = SDL_CreateTextureFromSurface(r, shine);
    SDL_DestroySurface(logo);
    SDL_DestroySurface(shine);
    SDL_SetTextureScaleMode(logoTex, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureScaleMode(shineTex, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(shineTex, SDL_BLENDMODE_ADD);
}

// --- Petits outils de dessin ----------------------------------------------------------
float hash01(unsigned n) {
    n = (n ^ 61u) ^ (n >> 16);
    n *= 9u;
    n ^= n >> 4;
    n *= 0x27d4eb2du;
    n ^= n >> 15;
    return (n & 0xFFFFFF) / float(0x1000000);
}

float clamp01(float v) { return std::clamp(v, 0.f, 1.f); }

void fill(SDL_Renderer* r, float x, float y, float w, float h) {
    const SDL_FRect rc{x, y, w, h};
    SDL_RenderFillRect(r, &rc);
}

void veil(SDL_Renderer* r, float alpha) { // voile noir sur tout l'ecran
    if (alpha <= 0.f) return;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, Uint8(255 * clamp01(alpha)));
    fill(r, 0, 0, VIEW_W, VIEW_H);
}

void disc(SDL_Renderer* r, float cx, float cy, float rad) {
    for (int dy = -int(rad); dy <= int(rad); ++dy) {
        const float w = std::floor(std::sqrt(rad * rad - float(dy * dy)));
        fill(r, std::floor(cx) - w, std::floor(cy) + dy, 2 * w + 1, 1);
    }
}

// Texte centre, avec une ombre ; `latin1` est deja passe par foldAccents
void centered(SDL_Renderer* r, float y, const std::string& latin1, SDL_Color c, float alpha = 1.f) {
    const float x = std::floor((VIEW_W - 8.f * latin1.size()) / 2);
    SDL_SetRenderDrawColor(r, 0, 0, 0, Uint8(200 * alpha));
    drawTextLatin1(r, x + 1, y + 1, latin1);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, Uint8(255 * alpha));
    drawTextLatin1(r, x, y, latin1);
}

std::vector<std::string> wrap(const std::string& t, size_t width) {
    std::vector<std::string> lines;
    std::string cur;
    size_t i = 0;
    while (i < t.size()) {
        size_t j = t.find(' ', i);
        if (j == std::string::npos) j = t.size();
        const std::string word = t.substr(i, j - i);
        if (!cur.empty() && cur.size() + 1 + word.size() > width) { lines.push_back(cur); cur.clear(); }
        if (!word.empty()) cur += (cur.empty() ? "" : " ") + word;
        i = j + 1;
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

// --- Decors -----------------------------------------------------------------------------
// Charge une zone du jeu comme toile de fond (sans monstres ni heros)
void useZone(Game& g, const std::string& zone) {
    Front& f = g.front;
    if (f.zone == zone && f.map) return;
    f.map.reset();
    f.zone = zone;
    for (const auto& path : g.maps)
        if (path.stem() == zone) {
            auto m = std::make_unique<Map>();
            if (m->load(g.renderer, path.string())) f.map = std::move(m);
        }
}

// Dessine la zone de fond centree sur la case (cx, cy), avec une lente derive de la camera
void drawBackdrop(Game& g, int cellX, int cellY, float t, float driftX, float driftY) {
    const Map* m = g.front.map.get();
    SDL_Renderer* r = g.renderer;
    SDL_SetRenderDrawColor(r, 4, 6, 14, 255);
    SDL_RenderClear(r);
    if (!m) return;
    if (cellX < 0) { cellX = m->width() / 2; cellY = m->height() * 3 / 4; }
    const Cell* c = m->cell(cellX, cellY);
    float camX = cellX * TILE_W - VIEW_W / 2.f + driftX * std::sin(t * 0.11f);
    float camY = (Map::MARGIN_ROWS + cellY - (c ? c->height : 0)) * TILE_H - VIEW_H / 2.f + driftY * std::sin(t * 0.07f + 1.f);
    camX = std::floor(std::clamp(camX, 0.f, std::max(0.f, float(m->pixelWidth() - VIEW_W))));
    camY = std::floor(std::clamp(camY, 0.f, std::max(0.f, float(m->pixelHeight() - VIEW_H))));
    m->draw(r, camX, camY, VIEW_W, VIEW_H);
    fxAmbient(g, camX, camY);
}

// Ciel de nuit : etoiles, lune, trois nappes de nuages qui filent (le navire volant des pirates)
// Navire volant des pirates, en ombre chinoise : coque, chateau arriere, deux mats, voiles, ailerons, helice
void drawShip(SDL_Renderer* r, float x, float y, float t) {
    x = std::floor(x);
    y = std::floor(y + 2.f * std::sin(t * 1.3f));
    SDL_SetRenderDrawColor(r, 8, 9, 24, 255);
    for (int j = 0; j < 11; ++j) fill(r, x - 34 + j * 1.6f, y + j, 74 - j * 4.6f, 1); // coque, proue a droite
    fill(r, x - 36, y - 7, 16, 7); // chateau arriere
    fill(r, x - 38, y - 9, 20, 2);
    fill(r, x + 26, y - 3, 12, 3); // gaillard d'avant
    for (int k = 0; k < 16; ++k) fill(r, x + 38 + k, y - 3 - k / 3, 1, 1); // beaupre
    const float masts[2] = {x - 10, x + 14};
    for (int m = 0; m < 2; ++m) {
        const float mx = masts[m];
        const int h = m ? 30 : 36;
        fill(r, mx, y - h, 2, float(h));
        for (int j = 0; j < 12; ++j) fill(r, mx - 9 - j * 0.3f, y - h + 3 + j, 22 + j * 0.5f, 1); // grand-voile
        for (int j = 0; j < 9; ++j) fill(r, mx - 8 - j * 0.3f, y - h + 18 + j, 19 + j * 0.5f, 1); // voile basse
        fill(r, mx + 2, y - h - 1, 6, 2); // flamme
        fill(r, mx + 8, y - h, 3, 1);
    }
    for (int j = 0; j < 4; ++j) { // ailerons lateraux
        fill(r, x - 20 - j * 3, y + 6 + j, 16, 1);
        fill(r, x + 6 - j * 3, y + 6 + j, 16, 1);
    }
    const float blade = 7.f * std::abs(std::sin(t * 18.f)); // helice de poupe
    fill(r, x - 40, y + 4 - blade, 1, 2 * blade + 1);
    fill(r, x - 42, y + 3, 6, 2);
    SDL_SetRenderDrawColor(r, 255, 196, 90, 255); // fanaux
    fill(r, x - 30, y - 4, 2, 2);
    fill(r, x + 4, y + 2, 1, 1);
    fill(r, x + 20, y + 2, 1, 1);
}

// `ship` : 0..1 = avancee du navire volant devant la lune ; negatif = pas de navire
void drawNightSky(Game& g, float t, float ship = -1.f) {
    SDL_Renderer* r = g.renderer;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int y = 0; y < VIEW_H; y += 2) {
        const Rgb c = mix({6, 8, 30}, {34, 44, 96}, y / float(VIEW_H));
        SDL_SetRenderDrawColor(r, Uint8(c.r), Uint8(c.g), Uint8(c.b), 255);
        fill(r, 0, float(y), VIEW_W, 2);
    }
    for (unsigned i = 0; i < 90; ++i) {
        const float x = hash01(i * 3 + 1) * VIEW_W, y = hash01(i * 3 + 2) * 170;
        const float tw = 0.5f + 0.5f * std::sin(t * (1.f + 2.f * hash01(i)) + i);
        SDL_SetRenderDrawColor(r, 210, 220, 255, Uint8(70 + 170 * tw));
        fill(r, std::floor(x), std::floor(y), 1, 1);
        if (i % 11 == 0) { // quelques etoiles plus grosses, en croix
            SDL_SetRenderDrawColor(r, 210, 220, 255, Uint8(60 * tw));
            fill(r, std::floor(x) - 1, std::floor(y), 3, 1);
            fill(r, std::floor(x), std::floor(y) - 1, 1, 3);
        }
    }
    // Lune et halo
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);
    for (int k = 0; k < 4; ++k) {
        SDL_SetRenderDrawColor(r, 14, 16, 26, 255);
        disc(r, 238, 64, 26.f + k * 7);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 238, 236, 214, 255);
    disc(r, 238, 64, 21);
    SDL_SetRenderDrawColor(r, 214, 212, 190, 255);
    disc(r, 231, 58, 4);
    disc(r, 245, 72, 6);
    disc(r, 242, 54, 2);
    // Nuages : silhouettes en disques, lisere clair cote lune ; les plus proches sont plus sombres et plus rapides
    for (int layer = 0; layer < 3; ++layer) {
        if (layer == 1 && ship >= 0.f) drawShip(r, 96.f + 150.f * ship, 92.f - 26.f * ship, t); // derriere les nuages proches
        const float speed = 7.f + 11.f * layer, baseY = 128.f + 34.f * layer, span = VIEW_W + 200.f;
        const Rgb body = mix({52, 62, 112}, {14, 16, 40}, layer / 2.f), rim = mix({120, 132, 180}, {40, 46, 86}, layer / 2.f);
        for (unsigned i = 0; i < 7; ++i) {
            const unsigned seed = layer * 100 + i;
            float x = std::fmod(hash01(seed) * span - t * speed, span);
            if (x < 0) x += span;
            x -= 100;
            const float y = baseY + 26.f * hash01(seed + 40) + 2.f * std::sin(t * 0.4f + seed);
            for (int pass = 0; pass < 2; ++pass) {
                const Rgb c = pass ? body : rim;
                SDL_SetRenderDrawColor(r, Uint8(c.r), Uint8(c.g), Uint8(c.b), 255);
                for (unsigned b = 0; b < 6; ++b) {
                    const float rad = 9.f + 12.f * hash01(seed * 7 + b) + 4.f * layer;
                    disc(r, x + b * 15.f, y + 5.f * hash01(seed * 13 + b) + (pass ? 0 : -2), rad);
                }
            }
        }
    }
    // Filets de vent
    for (unsigned i = 0; i < 10; ++i) {
        const float span = VIEW_W + 80.f;
        float x = std::fmod(hash01(i + 900) * span - t * (150.f + 90.f * hash01(i + 7)), span);
        if (x < 0) x += span;
        SDL_SetRenderDrawColor(r, 200, 214, 255, 34);
        fill(r, std::floor(x - 40), std::floor(30 + hash01(i + 950) * 180), 22 + 30 * hash01(i + 3), 1);
    }
}

// Chute de Flint : les nuages et les etoiles filent vers le haut, le heros tournoie (vrai sprite du jeu)
void drawFall(Game& g, float t) {
    SDL_Renderer* r = g.renderer;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int y = 0; y < VIEW_H; y += 2) {
        const Rgb c = mix({4, 6, 24}, {22, 34, 78}, y / float(VIEW_H));
        SDL_SetRenderDrawColor(r, Uint8(c.r), Uint8(c.g), Uint8(c.b), 255);
        fill(r, 0, float(y), VIEW_W, 2);
    }
    for (unsigned i = 0; i < 60; ++i) { // etoiles etirees par la vitesse
        float y = std::fmod(hash01(i * 5 + 2) * 300.f - t * (160.f + 120.f * hash01(i)), 300.f);
        if (y < 0) y += 300.f;
        SDL_SetRenderDrawColor(r, 200, 214, 255, Uint8(60 + 120 * hash01(i + 77)));
        fill(r, std::floor(hash01(i * 5 + 1) * VIEW_W), std::floor(y - 30), 1, 5 + 9 * hash01(i + 5));
    }
    auto clouds = [&](int layer) {
        const float speed = 150.f + 130.f * layer, span = VIEW_H + 160.f;
        const Rgb body = mix({46, 56, 104}, {16, 18, 44}, layer / 2.f), rim = mix({110, 124, 172}, {44, 50, 92}, layer / 2.f);
        for (unsigned i = 0; i < 5; ++i) {
            const unsigned seed = 300 + layer * 50 + i;
            float y = std::fmod(hash01(seed) * span - t * speed, span);
            if (y < 0) y += span;
            y -= 80;
            const float x = hash01(seed + 9) * (VIEW_W + 80.f) - 40;
            for (int pass = 0; pass < 2; ++pass) {
                const Rgb c = pass ? body : rim;
                SDL_SetRenderDrawColor(r, Uint8(c.r), Uint8(c.g), Uint8(c.b), 255);
                for (unsigned b = 0; b < 5; ++b)
                    disc(r, x + b * 16.f, y + 5.f * hash01(seed * 13 + b) + (pass ? 0 : 2), 10.f + 12.f * hash01(seed * 7 + b) + 3.f * layer);
            }
        }
    };
    clouds(0);
    clouds(1);
    // Le heros tournoie en derivant ; des filets d'air filent au-dessus de lui
    if (const auto* a = g.heroSprites.anim(SpriteBank::IDLE, DOWN); a && !a->steps.empty()) {
        const float cx = VIEW_W / 2.f + 26.f * std::sin(t * 1.1f), cy = 96.f + 8.f * std::sin(t * 2.3f);
        SDL_SetRenderDrawColor(r, 220, 230, 255, 40);
        for (int k = 0; k < 6; ++k) fill(r, std::floor(cx - 14 + k * 5.5f), std::floor(cy - 64 - 14 * hash01(k + unsigned(t * 20))), 1, 34);
        g.heroSprites.drawFrameRotated(r, a->steps[0].frame, cx, cy, double(t) * 250.0, 2.f);
    }
    clouds(2); // les nuages les plus proches passent devant lui
}

// Le "nom" d'une page du prologue : "decor" ou "decor/Personnage"
std::pair<std::string, std::string> sceneOf(const DialoguePage& p) {
    const size_t s = p.speaker.find('/');
    return s == std::string::npos ? std::make_pair(p.speaker, std::string()) : std::make_pair(p.speaker.substr(0, s), p.speaker.substr(s + 1));
}

bool isZone(const std::string& scene) { return !scene.empty() && std::isdigit(static_cast<unsigned char>(scene[0])); }

// --- Options ----------------------------------------------------------------------------
std::string optionsPath() {
    char* dir = SDL_GetPrefPath("Alundra2Remake", "Alundra2Remake");
    std::string p = dir ? std::string(dir) + "options.txt" : "options.txt";
    SDL_free(dir);
    return p;
}

void saveOptions(const Game& g) {
    std::ofstream out(optionsPath());
    out << "musique " << int(std::lround(g.music.volume * 10)) << "\nbruitages " << int(std::lround(g.music.sfxVolume * 10))
        << "\nplein_ecran " << (g.front.fullscreen ? 1 : 0) << "\n";
}

void quit() {
    SDL_Event e{};
    e.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&e);
}

// Menu principal : "Continuer" n'apparait que s'il existe une sauvegarde
enum { ACT_RESUME, ACT_NEW, ACT_OPTIONS, ACT_QUIT };
constexpr int OPTION_COUNT = 4;
int mainCount() { return hasSave() ? 4 : 3; }
int mainAction(int item) { return hasSave() ? item : item + 1; }
const char* mainLabel(int item) {
    static const char* const labels[] = {"Continuer", "Nouvelle partie", "Options", "Quitter"};
    return labels[mainAction(item)];
}

void enterTitle(Game& g) {
    g.mode = Mode::Title;
    g.front.tick = 0;
    g.front.leaving = 0;
    g.front.page = 0;
    useZone(g, g.startZone);
}

void enterCard(Game& g, int card) {
    Front& f = g.front;
    f.card = card;
    f.tick = 0;
    f.leaving = 0;
    auto it = g.dialogues.find("prologue");
    if (it == g.dialogues.end() || card >= int(it->second.size())) { // fin du prologue : le jeu commence
        startGame(g);
        g.front.fadeIn = FADE;
        return;
    }
    g.mode = Mode::Prologue;
    const std::string scene = sceneOf(it->second[size_t(card)]).first;
    if (isZone(scene)) useZone(g, scene);
}
} // namespace

void loadOptions(Game& g) {
    std::ifstream in(optionsPath());
    std::string key;
    int v;
    while (in >> key >> v) {
        if (key == "musique") g.music.volume = std::clamp(v, 0, 10) / 10.f;
        else if (key == "bruitages") g.music.sfxVolume = std::clamp(v, 0, 10) / 10.f;
        else if (key == "plein_ecran") g.front.fullscreen = v != 0;
    }
    if (g.front.fullscreen) SDL_SetWindowFullscreen(g.window, true);
}

// --- Mise a jour ------------------------------------------------------------------------
void frontTick(Game& g) {
    Front& f = g.front;
    const Input& in = g.in;
    const int dx = int(in.dx), dy = int(in.dy);
    const bool up = dy < 0 && f.prevDy >= 0, down = dy > 0 && f.prevDy <= 0;
    const bool left = dx < 0 && f.prevDx >= 0, right = dx > 0 && f.prevDx <= 0;
    f.prevDx = dx;
    f.prevDy = dy;
    ++f.tick;

    if (g.mode == Mode::Splash) {
        if (f.tick >= SPLASH_TICKS || in.attackPressed || in.dashPressed) enterTitle(g);
        return;
    }

    if (g.mode == Mode::Title) {
        if (!f.map) useZone(g, g.startZone);
        if (f.leaving > 0) { // fondu vers le prologue, ou vers la partie sauvegardee
            if (--f.leaving == 0) {
                if (f.resume && loadGame(g)) g.front.fadeIn = FADE;
                else enterCard(g, 0);
                f.resume = false;
            }
            return;
        }
        if (f.tick < 130) { // l'animation d'arrivee peut etre abregee
            if (in.attackPressed || in.dashPressed) f.tick = 130;
            return;
        }
        const int count = f.page == 0 ? mainCount() : OPTION_COUNT;
        f.menu = std::min(f.menu, count - 1);
        if (up || down) {
            f.menu = (f.menu + (down ? 1 : count - 1)) % count;
            playSfx(g, "curseur");
        }
        if (f.page == 0) {
            if (in.attackPressed) {
                playSfx(g, "valider");
                const int act = mainAction(f.menu);
                if (act == ACT_RESUME || act == ACT_NEW) { f.leaving = FADE; f.resume = act == ACT_RESUME; }
                else if (act == ACT_OPTIONS) { f.page = 1; f.menu = 0; }
                else quit();
            }
            return;
        }
        // Options : gauche / droite reglent, valider sur "Retour" (ou la touche d'esquive) revient
        const int step = right ? 1 : left ? -1 : (in.attackPressed && f.menu < 3) ? 1 : 0;
        if (step && f.menu < 2) {
            float& v = f.menu == 0 ? g.music.volume : g.music.sfxVolume;
            int level = int(std::lround(v * 10)) + step;
            if (in.attackPressed && level > 10) level = 0; // valider fait tourner la valeur
            v = std::clamp(level, 0, 10) / 10.f;
            playSfx(g, "curseur");
            saveOptions(g);
        } else if (step && f.menu == 2) {
            f.fullscreen = !f.fullscreen;
            SDL_SetWindowFullscreen(g.window, f.fullscreen);
            playSfx(g, "valider");
            saveOptions(g);
        }
        if ((in.attackPressed && f.menu == 3) || in.dashPressed) {
            playSfx(g, "annuler");
            f.page = 0;
            f.menu = hasSave() ? 2 : 1;
        }
        return;
    }

    // Prologue
    auto it = g.dialogues.find("prologue");
    if (it == g.dialogues.end() || f.card >= int(it->second.size())) { enterCard(g, 9999); return; }
    if (f.leaving > 0) {
        if (--f.leaving == 0) enterCard(g, f.leaving < 0 ? 9999 : f.card + 1);
        return;
    }
    const float len = float(foldAccents(it->second[size_t(f.card)].text).size());
    const int typedAt = FADE + int(len / TYPE_SPEED);
    if (in.dashPressed) { // passer tout le prologue
        f.card = int(it->second.size()) - 1;
        f.leaving = FADE;
    } else if (in.attackPressed) {
        if (f.tick < typedAt) f.tick = typedAt; // complete le texte
        else f.leaving = FADE;
    } else if (f.tick > typedAt + 200) {
        f.leaving = FADE;
    }
}

// --- Rendu ------------------------------------------------------------------------------
static void drawSplash(Game& g) {
    SDL_Renderer* r = g.renderer;
    const int t = g.front.tick;
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);
    const float a = clamp01(std::min(t - 25, SPLASH_TICKS - 30 - t) / 35.f);
    if (a <= 0) return;
    SDL_SetRenderDrawColor(r, 200, 180, 120, Uint8(255 * a)); // filet dore qui s'ouvre
    const float w = 120 * clamp01((t - 25) / 60.f);
    fill(r, VIEW_W / 2.f - w, 121, 2 * w, 1);
    centered(r, 100, foldAccents("Un remake de fan"), {255, 236, 180, 255}, a);
    centered(r, 130, foldAccents("d'après Alundra 2"), {170, 190, 230, 255}, a);
    centered(r, 142, foldAccents("Une légende est née"), {170, 190, 230, 255}, a);
    centered(r, 204, foldAccents("Les jeux originaux sont requis."), {110, 120, 150, 255}, a);
    centered(r, 216, foldAccents("Aucune ressource n'est distribuée."), {110, 120, 150, 255}, a);
}

static void drawMenu(Game& g, float alpha) {
    SDL_Renderer* r = g.renderer;
    const Front& f = g.front;
    const int count = f.page == 0 ? mainCount() : OPTION_COUNT;
    const float top = f.page == 0 ? (count == 4 ? 143.f : 150.f) : 143.f;
    for (int i = 0; i < count; ++i) {
        std::string label;
        if (f.page == 0) label = mainLabel(i);
        else if (i == 0) label = "Musique   " + std::string(size_t(std::lround(g.music.volume * 10)), '|') + std::string(size_t(10 - std::lround(g.music.volume * 10)), '.');
        else if (i == 1) label = "Bruitages " + std::string(size_t(std::lround(g.music.sfxVolume * 10)), '|') + std::string(size_t(10 - std::lround(g.music.sfxVolume * 10)), '.');
        else if (i == 2) label = std::string("Plein écran : ") + (f.fullscreen ? "oui" : "non");
        else label = "Retour";
        label = foldAccents(label);
        const float y = top + i * 15.f, w = 8.f * label.size();
        const bool sel = i == f.menu;
        if (sel) { // bandeau lumineux et losanges qui respirent
            const float pulse = 0.5f + 0.5f * std::sin(g.frame * 0.09f);
            for (int k = 0; k < 12; ++k) {
                const float half = 96.f - k * 8.f;
                SDL_SetRenderDrawColor(r, 8, 80, 88, Uint8((5 + 2 * pulse) * alpha));
                fill(r, VIEW_W / 2.f - half, y - 3, 2 * half, 14);
            }
            const float off = w / 2 + 10 + 2 * pulse;
            SDL_SetRenderDrawColor(r, 120, 160, 168, Uint8(255 * alpha));
            for (int s = -1; s <= 1; s += 2)
                for (int k = 0; k < 3; ++k) fill(r, std::floor(VIEW_W / 2.f + s * off) - k, y + 1 + k, 2 * k + 1, 1),
                                            fill(r, std::floor(VIEW_W / 2.f + s * off) - k, y + 5 - k, 2 * k + 1, 1);
        }
        centered(r, y, label, sel ? SDL_Color{210, 232, 225, 255} : SDL_Color{150, 164, 196, 255}, alpha);
    }
    const char* hint = f.page == 0 ? "Flèches : choisir    Espace : valider" : "Gauche / Droite : régler    C : retour";
    centered(r, VIEW_H - 14.f, foldAccents(hint), {120, 132, 166, 255}, alpha * 0.9f);
}

static void drawTitle(Game& g) {
    SDL_Renderer* r = g.renderer;
    buildLogo(r);
    const Front& f = g.front;
    const float t = g.frame / 60.f;
    drawBackdrop(g, g.startX, g.startY, t, 70.f, 18.f);

    // Voile : plus sombre en haut (logo) et en bas (menu), le decor respire au milieu
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int y = 0; y < VIEW_H; y += 2) {
        const float v = std::abs(y - 112.f) / 128.f;
        SDL_SetRenderDrawColor(r, 4, 6, 22, Uint8(70 + 150 * clamp01(v * v * 1.6f)));
        fill(r, 0, float(y), VIEW_W, 2);
    }

    // Logo : descend en fondu, puis un eclat le traverse de temps en temps
    const float la = clamp01((f.tick - 30) / 50.f);
    const float lx = std::floor((VIEW_W - LOGO_W) / 2.f), ly = std::floor(26 - 10 * (1 - la) * (1 - la));
    const SDL_FRect dst{lx, ly, float(LOGO_W), float(LOGO_H)};
    SDL_SetTextureAlphaMod(logoTex, Uint8(255 * la));
    SDL_RenderTexture(r, logoTex, nullptr, &dst);
    const int cycle = (f.tick - 90) % 300;
    if (f.tick > 90 && cycle < 70) {
        const float sx = lx - 30 + (LOGO_W + 60) * cycle / 70.f;
        for (int k = 0; k < 3; ++k) { // bande inclinee, plus vive au centre
            const float half = 14.f - k * 5;
            for (int y = 0; y < LOGO_H; y += 4) {
                const SDL_Rect clip{int(sx - half - y * 0.5f), int(ly) + y, int(2 * half), 4};
                SDL_SetRenderClipRect(r, &clip);
                SDL_SetTextureAlphaMod(shineTex, 70);
                SDL_RenderTexture(r, shineTex, nullptr, &dst);
            }
        }
        SDL_SetRenderClipRect(r, nullptr);
    }

    // Sous-titre entre deux filets
    const float sa = clamp01((f.tick - 80) / 40.f);
    if (sa > 0) {
        const std::string sub = foldAccents("UNE LÉGENDE EST NÉE");
        const float half = 8.f * sub.size() / 2 + 10, y = ly + LOGO_H + 4;
        centered(r, y, sub, {190, 208, 244, 255}, sa);
        SDL_SetRenderDrawColor(r, 200, 180, 120, Uint8(200 * sa));
        fill(r, VIEW_W / 2.f - half - 34 * sa, y + 3, 34 * sa, 1);
        fill(r, VIEW_W / 2.f + half, y + 3, 34 * sa, 1);
        centered(r, y + 13, foldAccents("- remake -"), {226, 150, 110, 255}, sa * 0.9f);
    }

    const float ma = clamp01((f.tick - 110) / 30.f);
    if (ma > 0) drawMenu(g, ma);

    veil(r, 1.f - clamp01(f.tick / 60.f));                    // arrivee depuis le noir
    if (f.leaving > 0) veil(r, 1.f - f.leaving / float(FADE)); // depart vers le prologue
}

static void drawPrologue(Game& g) {
    SDL_Renderer* r = g.renderer;
    const Front& f = g.front;
    auto it = g.dialogues.find("prologue");
    if (it == g.dialogues.end() || f.card >= int(it->second.size())) {
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        SDL_RenderClear(r);
        return;
    }
    const DialoguePage& page = it->second[size_t(f.card)];
    const float t = f.tick / 60.f;
    const auto [scene, name] = sceneOf(page);
    if (isZone(scene)) drawBackdrop(g, -1, -1, t, 46.f, 10.f); // travelling lent a travers la zone
    else if (scene == "chute") drawFall(g, t);
    else if (scene == "navire") { // le navire poursuit sa route d'une carte a l'autre
        int first = f.card, last = f.card;
        while (first > 0 && sceneOf(it->second[size_t(first - 1)]).first == "navire") --first;
        while (last + 1 < int(it->second.size()) && sceneOf(it->second[size_t(last + 1)]).first == "navire") ++last;
        drawNightSky(g, t + f.card * 10.f, clamp01((f.card - first + clamp01(t / 9.f)) / float(last - first + 1)));
    } else drawNightSky(g, t + f.card * 10.f);

    // Bandes de cinema et degrade sous le texte
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    fill(r, 0, 0, VIEW_W, 20);
    fill(r, 0, VIEW_H - 62.f, VIEW_W, 62);
    for (int k = 0; k < 16; ++k) {
        SDL_SetRenderDrawColor(r, 0, 0, 0, Uint8(240 - k * 15));
        fill(r, 0, VIEW_H - 63.f - k, VIEW_W, 1);
        fill(r, 0, 20.f + k / 2, VIEW_W, 1);
    }

    // Texte : machine a ecrire, lignes centrees
    const std::string text = foldAccents(page.text);
    const size_t shown = size_t(std::clamp((f.tick - FADE) * TYPE_SPEED, 0.f, float(text.size())));
    const auto lines = wrap(text, 37);
    size_t left = shown;
    float y = VIEW_H - 54.f + std::max(0.f, (4.f - lines.size()) * 5.f);
    if (!name.empty()) { // replique : nom du personnage au-dessus, en teal A1
        y = VIEW_H - 46.f;
        centered(r, VIEW_H - 59.f, foldAccents("- " + name + " -"), {180, 225, 225, 255}, clamp01((f.tick - FADE / 2) / 20.f));
    }
    for (const auto& l : lines) {
        const size_t n = std::min(left, l.size());
        // la ligne garde sa position finale pendant la frappe
        const float x = std::floor((VIEW_W - 8.f * l.size()) / 2);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        drawTextLatin1(r, x + 1, y + 1, l.substr(0, n));
        SDL_SetRenderDrawColor(r, 225, 238, 232, 255);
        drawTextLatin1(r, x, y, l.substr(0, n));
        left -= std::min(left, l.size() + 1);
        y += 11;
    }
    if (shown >= text.size() && (g.frame / 24) % 2) {
        drawA1PageArrow(r, VIEW_W - 22.f, VIEW_H - 20.f); // triangle natif « suite » (WIND.TX, banque 8)
    }
    SDL_SetRenderDrawColor(r, 90, 98, 124, 255);
    drawTextLatin1(r, 6, 6, foldAccents("C : passer"));

    veil(r, 1.f - clamp01(f.tick / float(FADE)));
    if (f.leaving > 0) veil(r, 1.f - f.leaving / float(FADE));
}

void frontDraw(Game& g) {
    if (g.mode == Mode::Splash) drawSplash(g);
    else if (g.mode == Mode::Title) drawTitle(g);
    else drawPrologue(g);
}
