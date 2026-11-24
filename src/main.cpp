// Alundra 2 Remake - tranche jouable.
//
// Commandes :
//   Fleches / WASD / stick / croix   : marcher (Maj / B = courir)
//   Espace / X / A (Croix)            : epee (3 coups enchaines = combo) ; parler aux PNJ ; ouvrir les coffres
//                                       maintenir une seconde puis relacher : coup charge (large et puissant)
//   C / Ctrl / X (Carre)              : esquive roulee (invulnerable)
//   M / START                         : menu armes / objets / equipement (A1, contenu A2)
//   Q                                 : alterner epee / gantelet d'eclair (apres l'avoir trouve)
//   H / LB                            : manger une herbe (rend 2 coeurs)
//   V / Alt / Y (Triangle)            : saut (monter sur les rebords de 2 niveaux, franchir les trous)
//   F1 debug, F2/F3/F4 passages, F5 monstre de test, PageUp/PageDown zones, Tab camera libre,
//   F11 plein ecran, Echap quitter.
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

#include "game.h"
#include "menu.h"

namespace fs = std::filesystem;

// --- Initialisation ----------------------------------------------------------------
static fs::path findAssets() {
    // SDL3 possede la chaine retournee : ne pas la lib�rer (contrairement a SDL_GetPrefPath).
    const char* path = SDL_GetBasePath();
    const fs::path base = path ? fs::path(path) : fs::path(".");
    for (const fs::path& dir : {base / "assets", base / ".." / "assets", fs::path("assets")}) {
        std::error_code ec;
        if (fs::is_directory(dir / "maps", ec)) return fs::weakly_canonical(dir);
    }
    return {};
}

void startGame(Game& g) {
    g.hero.invuln = 0;
    g.cleared.clear();
    g.items.clear();
    // Flint commence avec son epee longue (A2) : elle doit apparaitre dans
    // l'inventaire et pouvoir etre equipee des la premiere ouverture du menu.
    g.items["long_sword"] = 1;
    g.inventory = Inventory{};
    g.inventory.weapon = "long_sword";
    g.hero.swordPower = 1;
    g.hero.ep = 0;
    g.hero.maxEp = 200;
    g.menu = MenuState{};
    g.prevMenu = false;
    g.autoAttackSeen = false;
    g.opened.clear();
    g.chests.clear();
    g.pendingEnemies.clear();
    g.seenEvents.clear();
    g.prevAttack = g.prevDash = g.prevJump = false;
    g.hero.maxHp = 10;
    g.hero.weapon = Weapon::Sword;
    g.hero.charge = 0;
    g.hero.combo = 0;
    if (g.autoScript.empty()) g.bossDown = false;
    g.banner.clear();
    g.hero.hp = g.hero.maxHp;
    loadZone(g, g.startZone, g.startX, g.startY, g.startDir);
    g.mode = Mode::Play;
    if (!g.afterIntro.empty()) openDialogue(g, g.afterIntro);
    g.front.map.reset(); // le decor de l'ecran titre n'est plus utile
}

SDL_AppResult SDL_AppInit(void** state, int argc, char** argv) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_Log("SDL_Init : %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    auto* g = new Game;
    *state = g;
    std::string autoZone;
    int autoX = -1, autoY = -1;
    bool autoSupreme = false, autoAllItems = false, autoEnding = false, autoSaveTest = false;
    std::string autoGive, autoLoadSave;
    for (int i = 1; i + 1 < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--shot") g->autoShot = argv[++i];
        else if (a == "--diag") g->diagStage = std::atoi(argv[++i]);
        else if (a == "--zone") autoZone = argv[++i];
        else if (a == "--ogre") g->bossDown = std::atoi(argv[++i]) != 0; // test : l'ogre est deja vaincu
        else if (a == "--supreme") autoSupreme = true;                   // test : epee supreme debloquee
        else if (a == "--allitems") autoAllItems = true;                 // test : objets d'histoire presents
        else if (a == "--ending") autoEnding = true;                     // test : afficher le generique de fin
        else if (a == "--savetest") autoSaveTest = true;                 // test : ecrire puis relire la sauvegarde en memoire
        else if (a == "--give") autoGive = argv[++i];                    // test : id:quantite[,id:quantite]
        else if (a == "--loadsave") autoLoadSave = argv[++i];            // test : relire une sauvegarde (migration)
        else if (a == "--savefile") { g->saveOverride = argv[++i]; setSavePath(g->saveOverride); } // test : sauvegarde reelle
        else if (a == "--godmode") g->godMode = true;                    // test : combattre un boss sans risquer la mort
        else if (a == "--expect") g->autoExpect = argv[++i];             // test : etat final attendu ("ending")
        else if (a == "--at" && i + 2 < argc) { autoX = std::atoi(argv[++i]); autoY = std::atoi(argv[++i]); }
        else if (a == "--auto") {
            std::istringstream ss(argv[++i]);
            std::string tok;
            while (ss >> tok)
                if (tok.size() > 1) g->autoScript.push_back({tok[0], std::atoi(tok.c_str() + 1)});
        }
    }

    // Mode test : graine fixe, les deplacements aleatoires des monstres deviennent reproductibles
    // (SDL_srand(0) ou aucun appel = graine tiree de l'horloge).
    if (!g->autoScript.empty()) SDL_srand(20261121);

    if (!SDL_CreateWindowAndRenderer("Alundra 2 Remake", VIEW_W * 3, VIEW_H * 3, SDL_WINDOW_RESIZABLE, &g->window,
                                     &g->renderer)) {
        SDL_Log("Fenetre : %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderVSync(g->renderer, 1);
    SDL_SetRenderLogicalPresentation(g->renderer, VIEW_W, VIEW_H, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

    g->assets = findAssets();
    if (!g->assets.empty())
        for (auto& e : fs::directory_iterator(g->assets / "maps"))
            if (e.path().extension() == ".a1m") g->maps.push_back(e.path());
    std::sort(g->maps.begin(), g->maps.end());
    if (g->maps.empty() || !(g->heroSprites.load(g->renderer, (g->assets / "sprites" / "flint.a1s").string()) || g->heroSprites.load(g->renderer, (g->assets / "sprites" / "hero.a1s").string()))) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Alundra 2 Remake",
                                 "Assets introuvables.\nLance d'abord : python tools/export_assets.py "
                                 "extracted/a1/DATA/DATAS.BIN assets",
                                 g->window);
        return SDL_APP_FAILURE;
    }
    g->data = g->assets.parent_path() / "data";
    loadItemDefs(*g);
    loadRemakeData(*g); // armes, forge et interactions du remake (data/rm_*.txt)
    loadWorldData(*g);
    loadMusicCategories(*g);
    loadSfx(*g);
    initAudio(*g, !g->autoScript.empty()); // muet en mode test
    if (g->autoScript.empty()) loadOptions(*g);
    g->mode = g->autoScript.empty() || autoZone == "splash" ? Mode::Splash : Mode::Title; // test : --zone splash / titre
    if (auto t = g->zoneMusic.find("titre"); t != g->zoneMusic.end()) playMusic(*g, t->second);

    // Mode test : saute l'ecran titre (et eventuellement l'intro / la zone de depart)
    if (!g->autoScript.empty() && autoZone != "titre" && autoZone != "splash") {
        if (!autoZone.empty()) { g->startZone = autoZone; g->startX = autoX; g->startY = autoY; g->afterIntro.clear(); }
        startGame(*g);
        // Injecter l'etat de test APRES startGame() : sinon startGame() l'efface et les tests
        // auto_supreme / auto_vallee ne verifiaient rien du tout.
        if (autoSupreme) {
            g->seenEvents.insert("epee_supreme");
            g->items["epee_supreme"] = 1;
        }
        if (autoAllItems) {
            for (const char* id : {"epee_supreme", "ordre", "lampe", "prisme", "cristal",
                                   "boss_026", "boss_042", "boss_medusa"}) {
                g->items[id] = 1;
                g->seenEvents.insert(id);
            }
            // Ouvrir un coffre enregistre "objet_<objet>", pas seulement l'objet : les variantes
            // de dialogue en dependent. Le test doit reproduire le vrai etat apres ramassage.
            for (const char* id : {"ordre", "lampe", "prisme", "cristal"})
                g->seenEvents.insert(std::string("objet_") + id);
        }
        if ((autoSupreme || autoAllItems) && !g->seenEvents.count("epee_supreme")) {
            SDL_Log("Etat de test injecte avant startGame (test --supreme/--allitems)");
            return SDL_APP_FAILURE;
        }
        if (autoAllItems) {
            for (const char* id : {"ordre", "lampe", "prisme", "cristal", "boss_026", "boss_042", "boss_medusa"})
                if (!g->items.count(id) || !g->seenEvents.count(id)) {
                    SDL_Log("Objet de test manquant apres injection : %s", id);
                    return SDL_APP_FAILURE;
                }
            for (const char* id : {"ordre", "lampe", "prisme", "cristal"})
                if (!g->seenEvents.count(std::string("objet_") + id)) {
                    SDL_Log("Evenement de ramassage manquant apres injection : objet_%s", id);
                    return SDL_APP_FAILURE;
                }
        }
        if (!autoGive.empty()) { // objets d'inventaire injectes pour les tests deterministes
            std::istringstream given(autoGive);
            std::string entry;
            while (std::getline(given, entry, ',')) {
                const auto colon = entry.find(':');
                if (colon == std::string::npos || colon == 0) continue;
                const int n = std::atoi(entry.c_str() + colon + 1);
                if (n > 0) g->items[entry.substr(0, colon)] = n;
            }
        }
        if (!autoLoadSave.empty()) { // test deterministe : lecture d'une sauvegarde existante (V2/V3/V4)
            std::ifstream saved(autoLoadSave);
            if (!saved || !readSave(saved, *g)) {
                SDL_Log("Sauvegarde illisible ou refusee (test --loadsave) : %s", autoLoadSave.c_str());
                return SDL_APP_FAILURE;
            }
        }
        if (autoEnding) { // test deterministe : le generateur de fin apres la victoire sur M�phisto
            g->seenEvents.insert("mephisto_vaincu");
            g->mode = Mode::Ending;
            g->modeTimer = 0;
        }
        if (autoSaveTest) { // test deterministe : version, ecriture et relecture sans toucher au disque
            std::ostringstream buffer;
            writeSave(buffer, *g);
            std::istringstream input(buffer.str());
            if (!readSave(input, *g) || zoneName(*g) != autoZone ||
                (autoEnding && g->mode != Mode::Ending)) {
                SDL_Log("Sauvegarde versionnee invalide (test --savetest)");
                return SDL_APP_FAILURE;
            }
        }
    }
    return SDL_APP_CONTINUE;
}

// --- Evenements ----------------------------------------------------------------------
SDL_AppResult SDL_AppEvent(void* state, SDL_Event* e) {
    Game& g = *static_cast<Game*>(state);
    switch (e->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    case SDL_EVENT_KEY_DOWN: {
        const SDL_Keycode k = e->key.key;
        if (k == SDLK_ESCAPE) { // dans les options : retour au menu ; ailleurs : quitter
            if (g.mode == Mode::Title && g.front.page == 1) { g.front.page = 0; g.front.menu = hasSave() ? 2 : 1; break; }
            return SDL_APP_SUCCESS;
        }
        if (k == SDLK_F11) SDL_SetWindowFullscreen(g.window, !(SDL_GetWindowFlags(g.window) & SDL_WINDOW_FULLSCREEN));
        if (g.mode != Mode::Play || !g.map) break;
        if (k == SDLK_F1) g.debug = !g.debug;
        if (k == SDLK_H) useHerb(g);
        if (k == SDLK_Q) cycleWeapon(g);
        if (k == SDLK_M) cycleZoneMusic(g, 1);
        if (k == SDLK_N) cycleZoneMusic(g, -1);
        if (k == SDLK_F6) saveZoneMusic(g);
        if (k == SDLK_F7 || k == SDLK_F8) { // test des bruitages (numero affiche)
            const int n = int(g.music.sfx.size());
            if (n > 0) {
                for (int tries = 0; tries < n; ++tries) { // saute les numeros absents
                    g.sfxTest = ((g.sfxTest + (k == SDLK_F7 ? 1 : -1)) % n + n) % n;
                    if (g.music.sfx[size_t(g.sfxTest)].size() > 1) break;
                }
                playSfxId(g, g.sfxTest);
                floatText(g, g.hero.x, g.hero.y - 40, "Bruitage " + std::to_string(g.sfxTest), SDL_Color{160, 220, 255, 255});
            }
        }
        if (k == SDLK_TAB) g.freeCam = !g.freeCam;
        if (k == SDLK_PAGEDOWN || k == SDLK_PAGEUP) {
            const int n = int(g.maps.size());
            const int i = ((g.current + (k == SDLK_PAGEDOWN ? 1 : -1)) % n + n) % n;
            loadZone(g, g.maps[i].stem().string());
        }
        const int hx = int(g.hero.x / TILE_W), hy = int(g.hero.y / TILE_H);
        if (k == SDLK_F2) {
            Warp w;
            w.zone = zoneName(g);
            w.x = hx;
            w.y = hy;
            g.pendingWarp = w;
        }
        if (k == SDLK_F3 && g.pendingWarp) {
            Warp w = *g.pendingWarp;
            w.destZone = zoneName(g);
            w.destX = hx;
            w.destY = hy;
            w.destDir = g.hero.dir;
            g.warps.add(w);
            g.warps.save();
            g.pendingWarp.reset();
        }
        if (k == SDLK_F4 && g.warps.removeAt(zoneName(g), hx, hy)) g.warps.save();
        if (k == SDLK_F5) { // monstre de test devant le heros (cycle des types)
            static int kind = 0;
            Game::Spawn s{zoneName(g), hx + int(dirX(g.hero.dir) * 3), hy + int(dirY(g.hero.dir) * 3),
                          EnemyKind(kind++ % int(EnemyKind::Count))};
            const auto saved = g.enemies;
            g.spawns.push_back(s);
            g.cleared[s.zone] = false;
            spawnZoneEntities(g);
            g.spawns.pop_back();
            Enemy added = g.enemies.back();
            g.enemies = saved;
            g.enemies.push_back(added);
        }
        break;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        if (e->gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER && g.mode == Mode::Play) useHerb(g);
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
        if (!g.pad) g.pad = SDL_OpenGamepad(e->gdevice.which);
        break;
    default:
        break;
    }
    return SDL_APP_CONTINUE;
}

// --- Entrees ------------------------------------------------------------------------
static void readInput(Game& g) {
    Input& in = g.in;
    const bool* k = SDL_GetKeyboardState(nullptr);
    in.dx = in.dy = 0;
    if (k[SDL_SCANCODE_LEFT] || k[SDL_SCANCODE_A]) in.dx -= 1;
    if (k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_D]) in.dx += 1;
    if (k[SDL_SCANCODE_UP] || k[SDL_SCANCODE_W]) in.dy -= 1;
    if (k[SDL_SCANCODE_DOWN] || k[SDL_SCANCODE_S]) in.dy += 1;
    in.run = k[SDL_SCANCODE_LSHIFT] || k[SDL_SCANCODE_RSHIFT];
    in.attack = k[SDL_SCANCODE_SPACE] || k[SDL_SCANCODE_X] || k[SDL_SCANCODE_RETURN];
    in.dash = k[SDL_SCANCODE_C] || k[SDL_SCANCODE_LCTRL];
    in.jump = k[SDL_SCANCODE_V] || k[SDL_SCANCODE_LALT];
    in.menu = k[SDL_SCANCODE_M];
    if (g.pad) {
        const float ax = SDL_GetGamepadAxis(g.pad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.f;
        const float ay = SDL_GetGamepadAxis(g.pad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.f;
        if (std::abs(ax) > 0.3f) in.dx += ax > 0 ? 1 : -1;
        if (std::abs(ay) > 0.3f) in.dy += ay > 0 ? 1 : -1;
        if (SDL_GetGamepadButton(g.pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT)) in.dx -= 1;
        if (SDL_GetGamepadButton(g.pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) in.dx += 1;
        if (SDL_GetGamepadButton(g.pad, SDL_GAMEPAD_BUTTON_DPAD_UP)) in.dy -= 1;
        if (SDL_GetGamepadButton(g.pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) in.dy += 1;
        in.run |= SDL_GetGamepadButton(g.pad, SDL_GAMEPAD_BUTTON_EAST);
        in.attack |= SDL_GetGamepadButton(g.pad, SDL_GAMEPAD_BUTTON_SOUTH);
        in.dash |= SDL_GetGamepadButton(g.pad, SDL_GAMEPAD_BUTTON_WEST);
        in.jump |= SDL_GetGamepadButton(g.pad, SDL_GAMEPAD_BUTTON_NORTH);
        in.menu |= SDL_GetGamepadButton(g.pad, SDL_GAMEPAD_BUTTON_START);
    }
    // Mode test : le script remplace les entrees reelles.
    // U/D/L/R<n> marcher, A<n> epee, K<n> epee maintenue, C<n> esquive, J<n> saut, W<n> attendre, F1 debug,
    // T<n> = touche "parler/valider", M<n> = ouvrir/fermer le menu, B<n> = annuler le menu.
    // G<n> = ouvrir la page de forge (test, sans passer par le forgeron).
    // X<n> = valider (appui 2 ticks, relache 2 ticks) tant qu'un dialogue est ouvert, au plus n
    // ticks : les tests ne dependent plus du nombre de panneaux (pagination A1 a deux lignes).
    if (!g.autoScript.empty()) {
        in = Input{};
        while (g.autoStep < g.autoScript.size()) {
            auto [cmd, n] = g.autoScript[g.autoStep];
            if (cmd == 'F') { if (n == 1) g.debug = !g.debug; ++g.autoStep; continue; }
            if (cmd == 'X' && g.autoTicks % 4 == 0 && !g.box.active()) { g.autoTicks = 0; ++g.autoStep; continue; }
            if (g.autoTicks >= n) { g.autoTicks = 0; ++g.autoStep; continue; }
            ++g.autoTicks;
            if (cmd == 'X') in.attack = (g.autoTicks - 1) % 4 < 2;
            if (cmd == 'U') in.dy = -1;
            if (cmd == 'D') in.dy = 1;
            if (cmd == 'L') in.dx = -1;
            if (cmd == 'R') in.dx = 1;
            if (cmd == 'A' || cmd == 'T') in.attack = g.autoTicks <= 2; // appui bref
            if (cmd == 'Q' && g.autoTicks == 1) cycleWeapon(g);
            if (cmd == 'G' && g.autoTicks == 1 && g.mode == Mode::Play && !g.menu.open) openForge(g);
            if (cmd == 'C' || cmd == 'B') in.dash = g.autoTicks <= 2;
            if (cmd == 'J') in.jump = g.autoTicks <= 2;
            if (cmd == 'M') in.menu = g.autoTicks <= 2;
            if (cmd == 'K') in.attack = true; // maintenir l'epee (coup charge)
            break;
        }
        if (g.autoStep >= g.autoScript.size()) g.autoDone = true;
    }
    in.dx = std::clamp(in.dx, -1.f, 1.f);
    in.dy = std::clamp(in.dy, -1.f, 1.f);
    in.attackPressed = in.attack && !g.prevAttack;
    in.attackReleased = g.prevAttack && !in.attack;
    in.dashPressed = in.dash && !g.prevDash;
    in.jumpPressed = in.jump && !g.prevJump;
    in.menuPressed = in.menu && !g.prevMenu;
    g.prevJump = in.jump;
    g.prevAttack = in.attack;
    g.prevDash = in.dash;
    g.prevMenu = in.menu;
}

// Validation dans le menu : aucun appel a une attaque ou a une interaction monde.
static void confirmMenuSelection(Game& g) {
    if (g.menu.page == MenuPage::Forge) { // forge : deux appuis, une seule fonction de mutation
        if (g.menu.cursor >= g.forgeRecipes.size()) return;
        const ForgeRecipe& r = g.forgeRecipes[g.menu.cursor];
        if (g.forgeConfirm != r.id) {
            g.forgeConfirm = r.id;
            const std::string state = forgeState(g, r);
            g.forgeMessage = state == "available" ? "confirmer" : state;
            playSfx(g, "valider");
            return;
        }
        g.forgeConfirm.clear();
        switch (confirmForge(g, r.id)) {
        case ForgeResult::Ok: g.forgeMessage = "rm_forge_ok"; playJingle(g, "objet"); saveGame(g); break;
        case ForgeResult::Missing: g.forgeMessage = "rm_forge_manque"; break;
        case ForgeResult::AlreadyDone:
        case ForgeResult::OtherBranch: g.forgeMessage = "rm_forge_deja"; break;
        case ForgeResult::NoWeapon: g.forgeMessage = "rm_forge_sans_arme"; break;
        case ForgeResult::Unknown: g.forgeMessage.clear(); break;
        }
        return;
    }
    const std::string id = menuSelectedId(g);
    if (id.empty()) return;
    const auto def = g.itemDefs.find(id);
    if (def == g.itemDefs.end()) return;
    if (g.menu.page == MenuPage::Equipment) {
        if (def->second.kind == ItemKind::Weapon) equipWeapon(g, id);
        else if (def->second.kind == ItemKind::Ring && g.items.count(id) && g.items.at(id) > 0)
            g.inventory.ring = id;
        else if (def->second.kind == ItemKind::Pocket && g.items.count(id) && g.items.at(id) > 0)
            g.inventory.pocket = id;
        return;
    }
    useItem(g, id);
}

// --- Heros ----------------------------------------------------------------------------
// Le heros ne traverse ni le decor ni les PNJ
static bool heroCanStand(Game& g, float x, float y) {
    if (!canStand(*g.map, x, y, g.hero.z)) return false;
    if (chestBlocks(g, x, y) || interactionBlocks(g, x, y)) return false;
    for (const auto& n : g.npcs)
        if (std::abs(n.x - x) < 12 && std::abs(n.y - y) < 7) return false;
    return true;
}

// Saut : vitesse initiale (5 px par tick, la valeur d'Alundra 1 : 1280 / 256) et gravite. Le saut est ample
// et assez long (0,75 s) pour franchir la case de falaise qui borde un rebord et se poser dessus.
// JUMP_CLIMB : on ne monte pas plus haut que 2 niveaux au-dessus du point d'appel (la marche type d'Alundra 1).
constexpr float JUMP_SPEED = 5.0f, GRAVITY = 0.22f;
constexpr int JUMP_CLIMB = 2;

// En l'air, le heros passe au-dessus de tout ce qui est plus bas que ses pieds (falaises, bordures, vide)
static bool heroCanFly(Game& g, float x, float y) {
    const Hero& h = g.hero;
    const float alt = h.z * TILE_H + h.air + 3.f;
    for (float px : {x - 6, x + 6})
        for (float py : {y - 3, y}) {
            const Cell* c = g.map->cell(int(std::floor(px / TILE_W)), int(std::floor(py / TILE_H)));
            if (!c || c->height * TILE_H > alt || c->height > h.jumpZ + JUMP_CLIMB) return false;
        }
    for (const auto& n : g.npcs)
        if (std::abs(n.x - x) < 12 && std::abs(n.y - y) < 7) return false;
    if (h.air < 18 && (chestBlocks(g, x, y) || interactionBlocks(g, x, y))) return false; // on peut sauter par-dessus
    return true;
}

static bool heroCanMove(Game& g, float x, float y) { return g.hero.airborne ? heroCanFly(g, x, y) : heroCanStand(g, x, y); }

// Atterrissage : si la case n'est pas praticable (falaise, bordure, vide), on glisse vers le sol praticable
// le plus proche qui n'est pas plus haut que le point de chute ; sinon c'est une chute dans le vide.
static void landHero(Game& g) {
    Hero& h = g.hero;
    h.airborne = false;
    h.air = h.vz = 0;
    if (!canStand(*g.map, h.x, h.y, h.z)) {
        float best = 1e9f, bx = 0, by = 0;
        int bz = 0;
        for (float dy = -40; dy <= 40; dy += 2)
            for (float dx = -40; dx <= 40; dx += 2) {
                const Cell* c = g.map->cell(int(std::floor((h.x + dx) / TILE_W)), int(std::floor((h.y + dy) / TILE_H)));
                if (!c || c->height > h.z || !canStand(*g.map, h.x + dx, h.y + dy, c->height)) continue;
                // on prefere continuer dans le sens du mouvement plutot que revenir en arriere
                const float d = std::hypot(dx, dy) - 0.35f * (dx * dirX(h.dir) + dy * dirY(h.dir));
                if (d < best) { best = d; bx = h.x + dx; by = h.y + dy; bz = c->height; }
            }
        if (best < 1e8f) {
            h.x = bx;
            h.y = by;
            h.z = bz;
        } else { // rien de praticable : chute dans le vide, retour au dernier sol sur
            h.x = h.safeX;
            h.y = h.safeY;
            if (const Cell* c = g.map->cell(int(h.x / TILE_W), int(h.y / TILE_H))) h.z = c->height;
            hurtHero(g, 1, h.x, h.y);
            h.kx = h.ky = 0;
            floatText(g, h.x, h.y - 46, "CHUTE !", {150, 210, 220, 255});
            return;
        }
    }
    burst(g, h.x, h.y, h.z, {210, 200, 170, 255}, 5, 0.8f);
    playSfx(g, "atterrir");
}

static Npc* npcInFront(Game& g) {
    Hero& h = g.hero;
    const float fx = h.x + dirX(h.dir) * 14, fy = h.y + dirY(h.dir) * 10;
    for (auto& n : g.npcs)
        if (std::hypot(n.x - fx, n.y - fy) < 20) return &n;
    return nullptr;
}

static const char* warpNeedLabel(const std::string& need) {
    if (need == "epee_supreme") return "Il faut l'epee de Jeehan";
    if (need == "lampe") return "Il faut la Lampe de Lumiere";
    if (need == "prisme") return "Il faut le Prisme d'Etoile";
    if (need == "cristal") return "Il faut le Cristal d'Etoile";
    if (need == "boss_medusa") return "Le capitaine des pirates garde le passage";
    return "Un objet ou un evenement manque";
}

static void updateHero(Game& g) {
    Hero& h = g.hero;
    const Input& in = g.in;
    if (h.invuln > 0) --h.invuln;
    if (h.dashCooldown > 0) --h.dashCooldown;
    ++h.sinceAttack;

    // Recul subi
    if (std::abs(h.kx) + std::abs(h.ky) > 0.05f) {
        if (heroCanMove(g, h.x + h.kx, h.y)) h.x += h.kx;
        if (heroCanMove(g, h.x, h.y + h.ky)) h.y += h.ky;
        h.kx *= 0.82f;
        h.ky *= 0.82f;
    }

    float dx = in.dx, dy = in.dy;

    // Parler / ouvrir un coffre est prioritaire sur l'attaque, quelle que soit l'arme.
    auto tryInteract = [&]() -> bool {
        if (Npc* n = npcInFront(g)) {
            n->dir = Dir(h.dir == UP ? DOWN : h.dir == DOWN ? UP : h.dir == LEFT ? RIGHT : LEFT);
            std::string id = n->dialogue;
            // Les variants d'evenement priment sur "_apres" : sans cela, Jeehan restait sur
            // "jeehan_apres" des la lettre et ses dialogues de progression etaient perdus.
            bool eventVariant = false;
            // L'ordre des evenements fait foi : le dernier variant disponible est le plus recent.
            for (const char* ev : {"objet_gantelet", "objet_lettre", "objet_ordre", "objet_idole",
                                   "epee_supreme", "objet_lampe", "objet_prisme", "objet_cristal",
                                   "boss_medusa", "prunewell"}) {
                // Les retrouvailles Prunewell/Finrod sont la scene d'avant la
                // lettre : apres, Finrod doit reprendre son dialogue "_apres".
                if (std::string(ev) == "prunewell" && g.items.count("lettre")) continue;
                const std::string alt = id + "_" + ev;
                if (g.seenEvents.count(ev) && g.dialogues.count(alt)) {
                    id = alt;
                    eventVariant = true;
                }
            }
            if (!eventVariant && g.items.count("lettre")) {
                const std::string after = id + "_apres";
                if (g.dialogues.count(after)) id = after;
            }
            if (id.rfind("jeehan", 0) == 0) grantSupreme(g);
            // Orvan (forgeron, ajout du remake) : premiere rencontre = il confie le Marteau de
            // faille ; ensuite, la page de forge s'ouvre a la fin de sa replique.
            if (id == "rm_forgeron") {
                if (!g.seenEvents.count("rm_forgeron")) {
                    g.seenEvents.insert("rm_forgeron");
                    g.seenEvents.insert("objet_rm_rift_hammer");
                    ++g.items["rm_rift_hammer"];
                    playJingle(g, "objet");
                } else {
                    id = "rm_forgeron_forge";
                    g.forgeAfterDialogue = true;
                }
            }
            openDialogue(g, id);
            return true;
        }
        if (Chest* c = chestInFront(g)) {
            openChest(g, *c);
            return true;
        }
        if (const InteractionDef* d = interactionInFront(g)) return useInteraction(g, *d);
        return false;
    };

    // Coup normal : il part au relachement du bouton. Tant que le bouton est maintenu, le heros
    // reste sur la pose d'armement ; c'est la regle d'Alundra 1 (verifiee sur la video de reference).
    auto startSwing = [&]() {
        g.autoAttackSeen = true;
        h.combo = h.sinceAttack < 20 ? std::min(3, h.combo + 1) : 1;
        if (h.combo == 3 && h.sinceAttack >= 20) h.combo = 1;
        h.attacking = true;
        h.attackTick = 0;
        ++h.swingId;
        h.anim.play(g.heroSprites.anim(SpriteBank::ATTACK, h.dir), true);
        if (h.combo == 3) floatText(g, h.x, h.y - 46, "COMBO x3 !", {255, 200, 80, 255});
        playSfx(g, ("epee" + std::to_string(h.combo)).c_str());
    };

    // Coup charge : orbe clair a l'impact, puis gerbe d'eclats bleus en eventail.
    auto startCharged = [&]() {
        g.autoAttackSeen = true;
        h.charge = 0;
        h.combo = 4;
        h.attacking = true;
        h.attackTick = 0;
        ++h.swingId;
        h.anim.play(g.heroSprites.anim(SpriteBank::ATTACK, h.dir), true);
        playSfx(g, "epee3");
        playSfx(g, "coup_fort");
        g.shake = 8;
        const float fx = dirX(h.dir), fy = dirY(h.dir);
        spawnEffect(g, "fx_orbe", h.x + fx * 22, h.y + fy * 15 - 6, h.z, 0.9f);
        for (int k = -1; k <= 1; ++k) {
            const float side = k * 15.f, ahead = 30.f - std::abs(k) * 7.f;
            const float ex = h.x + fx * ahead - fy * side, ey = h.y + fy * ahead * 0.75f + fx * side * 0.75f - 8;
            spawnEffect(g, k ? "fx_eclat" : "fx_eclats", ex, ey, h.z, 1.f, 4 + std::abs(k) * 2);
            spawnEffect(g, "fx_etoile", ex + fx * 6, ey + fy * 4, h.z, 1.f, 14 + std::abs(k) * 3);
        }
    };

    // Gantelet d'eclair (Alundra 2) : decharge instantanee en ligne, efficace sur les droides.
    auto startBolt = [&]() {
        g.autoAttackSeen = true;
        h.charge = 0;
        h.combo = 5;
        h.attacking = true;
        h.attackTick = 0;
        ++h.swingId;
        h.anim.play(g.heroSprites.anim(SpriteBank::ATTACK, h.dir), true);
        playSfx(g, "eclair");
        const float fx = dirX(h.dir), fy = dirY(h.dir);
        for (int i = 1; i <= 3; ++i) {
            const float ax = h.x + fx * i * 18, ay = h.y + fy * i * 13 - 6;
            spawnEffect(g, "fx_orbe", ax, ay, h.z, 0.55f, i * 3);
            spawnEffect(g, "fx_eclat", ax - fy * 7, ay + fx * 7 - 4, h.z, 0.8f, i * 4);
        }
        g.shake = std::max(g.shake, 3);
    };

    if (h.attacking) {
        if (in.attackPressed && h.attackTick > 6) h.queuedAttack = true;
        h.anim.update();
        if (h.anim.finished) {
            h.attacking = false;
            h.sinceAttack = 0;
            if (h.queuedAttack) {
                h.queuedAttack = false;
                if (h.weapon == Weapon::Eclair) startBolt();
                else startSwing();
            }
        }
        dx = dy = 0;
        // L'esquive annule la fin d'un coup (enchainement attaque -> esquive)
        if (in.dashPressed && h.attackTick > 10) { h.attacking = false; h.sinceAttack = 0; h.queuedAttack = false; }
    }

    constexpr int CHARGE_POSE = 9, CHARGE_READY = 60;
    const bool canAct = !h.attacking && !h.airborne && h.dashTicks == 0 && h.hurtTicks == 0;
    if (h.weapon == Weapon::Eclair) {
        if (canAct && in.attackPressed && !tryInteract()) startBolt();
        h.charge = 0;
    } else if (canAct) {
        if (in.attackPressed) {
            if (!tryInteract()) h.charge = 1; // debut d'armement : aucun coup tant que le bouton n'est pas relache
        }
        if (h.charge > 0 && in.attack) {
            ++h.charge;
            if (h.charge == CHARGE_POSE) h.anim.play(g.heroSprites.anim(SpriteBank::ATTACK, h.dir), true);
            if (h.charge == CHARGE_READY) playSfx(g, "charge");
        }
        if (h.charge > 0 && in.attackReleased) {
            if (h.charge >= CHARGE_READY) startCharged();
            else startSwing();
        }
    } else {
        h.charge = 0;
    }
    const bool chargePose = h.weapon != Weapon::Eclair && h.charge >= CHARGE_POSE;
    if (chargePose) dx = dy = 0;

    if (in.jumpPressed && !h.airborne && !h.attacking && h.dashTicks == 0) {
        h.airborne = true;
        h.jumpZ = h.z;
        h.vz = JUMP_SPEED;
        h.air = 0;
        playSfx(g, "saut");
    }

    if (!h.attacking && h.dashTicks == 0 && !h.airborne && in.dashPressed && h.dashCooldown == 0) {
        h.dashTicks = 14;
        playSfx(g, "esquive");
        h.dashCooldown = 36;
        const float l = std::hypot(dx, dy);
        h.dashX = l > 0 ? dx / l : dirX(h.dir);
        h.dashY = l > 0 ? dy / l : dirY(h.dir);
        for (auto& t : h.trail) t = {h.x, h.y};
    }
    if (h.combo >= 3 && !h.attacking) h.combo = 0; // le combo repart de zero apres le 3e coup

    if (h.dashTicks > 0) { // esquive : rapide, invulnerable, traînee
        --h.dashTicks;
        const float s = 3.4f;
        if (heroCanStand(g, h.x + h.dashX * s, h.y)) h.x += h.dashX * s;
        if (heroCanStand(g, h.x, h.y + h.dashY * s)) h.y += h.dashY * s;
        for (size_t i = h.trail.size() - 1; i > 0; --i) { h.trail[i] = h.trail[i - 1]; h.trailFrame[i] = h.trailFrame[i - 1]; }
        h.trail[0] = {h.x, h.y};
        h.trailFrame[0] = h.anim.frame();
        if (h.dashTicks % 3 == 0) burst(g, h.x, h.y, h.z, {200, 200, 170, 255}, 2, 0.6f);
        dx = dy = 0;
    }

    h.moving = dx != 0 || dy != 0;
    if (h.moving) {
        if (dx < 0) h.dir = LEFT;
        else if (dx > 0) h.dir = RIGHT;
        if (dy != 0 && dx == 0) h.dir = dy < 0 ? UP : DOWN;
        float speed = in.run ? 2.0f : 1.3f;
        if (dx != 0 && dy != 0) speed *= 0.7071f;
        if (heroCanMove(g, h.x + dx * speed, h.y)) h.x += dx * speed;
        if (heroCanMove(g, h.x, h.y + dy * speed)) h.y += dy * speed;
    }
    if (const Cell* c = g.map->cell(int(h.x / TILE_W), int(h.y / TILE_H))) {
        if (h.airborne) { // l'altitude des pieds reste continue quand le sol change sous le heros
            h.air += (h.z - c->height) * TILE_H;
            h.z = c->height;
            h.vz -= GRAVITY;
            h.air += h.vz;
            if (h.air <= 0 && h.vz < 0) landHero(g);
            if (g.mode != Mode::Play) return;
        } else {
            h.z = c->height;
            h.safeX = h.x;
            h.safeY = h.y;
        }
    }

    if (h.hurtTicks > 0) {
        --h.hurtTicks;
        h.anim.update();
    } else if (h.airborne && g.heroSprites.anim(SpriteBank::JUMP, h.dir)) {
        h.anim.play(g.heroSprites.anim(SpriteBank::JUMP, h.dir));
    } else if (chargePose) {
        // pose d'armement tenue : l'animation ne defile pas
    } else if (!h.attacking) {
        int grp = h.moving || h.dashTicks ? SpriteBank::WALK : SpriteBank::IDLE;
        if (grp == SpriteBank::WALK && (in.run || h.dashTicks) && g.heroSprites.anim(SpriteBank::RUN, h.dir)) grp = SpriteBank::RUN;
        h.anim.play(g.heroSprites.anim(grp, h.dir));
        h.anim.update();
        if (h.dashTicks) h.anim.update(); // animation acceleree pendant l'esquive
    }
    updateHeroCombat(g);

    // Passages
    const Warp* w = h.airborne ? nullptr : g.warps.find(zoneName(g), int(h.x / TILE_W), int(h.y / TILE_H));
    if (h.airborne) {
    } else if (w && !w->need.empty() && !g.items.count(w->need) && !g.seenEvents.count(w->need)) {
        if (g.frame % 45 == 0) floatText(g, h.x, h.y - 44, warpNeedLabel(w->need), {150, 210, 220, 255});
        g.onWarp = true; // on ne reparle qu'en sortant du passage
    } else if (w && !g.onWarp) {
        const Warp dest = *w;
        loadZone(g, dest.destZone, dest.destX, dest.destY, Dir(dest.destDir));
    } else if (!w) {
        g.onWarp = false;
    }
}

// --- Mise a jour ----------------------------------------------------------------------
static void followHero(Game& g) {
    if (g.freeCam || !g.map) return;
    const Hero& h = g.hero;
    g.camX = h.x - VIEW_W / 2.f;
    // verticale adoucie : pas d'a-coup quand le heros change de niveau en sautant
    const float target = Map::MARGIN_ROWS * TILE_H - (h.z * TILE_H + h.air * 0.5f) + h.y - VIEW_H / 2.f - 16;
    g.camY = std::abs(target - g.camY) > 90 ? target : g.camY + (target - g.camY) * 0.2f;
}

static void tickGame(Game& g);
static void tick(Game& g) {
    tickGame(g);
    if (g.mode == Mode::Play || g.mode == Mode::GameOver || g.mode == Mode::Ending) followHero(g);
}

static void tickGame(Game& g) {
    ++g.frame;
    readInput(g);

    // Test de victoire reelle : des que Meghisto tombe, on arrete le script. Sinon les appuis
    // suivants feraient defiler le generateur, retourneraient au titre et rechargeraient une sauvegarde.
    if (!g.autoScript.empty() && g.autoExpect == "ending" && g.mode == Mode::Ending) {
        g.autoScript.clear();
        g.autoDone = true;
        return;
    }

    if (g.mode == Mode::Splash || g.mode == Mode::Title || g.mode == Mode::Prologue) {
        frontTick(g);
        return;
    }
    // Menu A1 : il isole completement la boucle de jeu. Aucune attaque, aucun
    // deplacement, aucune interaction ni dialogue ne peut se produire pendant
    // qu'il est ouvert ; updateMenu consomme attackPressed pour cette raison.
    if (g.mode == Mode::Play) {
        if (g.menu.open) {
            const bool confirm = g.in.attackPressed;
            updateMenu(g);
            if (confirm) confirmMenuSelection(g);
            g.hero.anim.play(g.heroSprites.anim(SpriteBank::IDLE, g.hero.dir));
            return;
        }
        if (g.in.menuPressed && !g.box.active() && g.gotItem.empty()) {
            openMenu(g);
            return;
        }
    }
    if (g.mode == Mode::Ending) {
        updateEffects(g);
        g.hero.anim.update();
        if (++g.modeTimer > 420 || (g.modeTimer > 90 && g.in.attackPressed)) {
            g.mode = Mode::Title;
            g.modeTimer = 0;
            g.front.fadeIn = 50;
        }
        return;
    }
    if (g.mode == Mode::GameOver) {
        updateEffects(g);
        g.hero.anim.update();
        if (++g.modeTimer > 150 || (g.modeTimer > 40 && g.in.attackPressed)) {
            // Reapparition a l'entree de la zone, monstres remis en place
            g.hero.hp = g.hero.maxHp;
            g.mode = Mode::Play;
            loadZone(g, zoneName(g), int(g.respawnX / TILE_W), int(g.respawnY / TILE_H), DOWN);
            g.hero.invuln = 90;
        }
        return;
    }

    if (g.box.active()) { // les dialogues mettent le jeu en pause
        updateDialogue(g);
        g.hero.anim.play(g.heroSprites.anim(SpriteBank::IDLE, g.hero.dir));
        if (!g.box.active() && g.forgeAfterDialogue) { // fin de la replique d'Orvan
            g.forgeAfterDialogue = false;
            openForge(g);
        }
        return;
    }
    if (updateItems(g)) { // objet obtenu : le heros le leve, le jeu attend
        updateEffects(g);
        return;
    }
    if (g.hitStop > 0) { --g.hitStop; return; }

    if (g.freeCam) {
        const float s = g.in.run ? 10.f : 3.5f;
        g.camX += g.in.dx * s;
        g.camY += g.in.dy * s;
        return;
    }
    updateHero(g);
    if (g.box.active()) return; // un dialogue vient de s'ouvrir
    updateEnemies(g);
    updateBossMusic(g);
    for (auto& n : g.npcs) {
        if (SpriteBank* b = spriteBank(g, n.sprite)) n.anim.play(b->anim(0, n.dir));
        n.anim.update();
    }
    updateEffects(g);
}

// --- Rendu ----------------------------------------------------------------------------
static float screenY(float y, float z, float cy) { return std::floor(Map::MARGIN_ROWS * TILE_H + y - z * TILE_H - cy); }

static void drawHeart(SDL_Renderer* r, float x, float y, int fill) { // fill : 0 vide, 1 moitie, 2 plein
    static const char* shape[] = {".XX.XX.", "XXXXXXX", "XXXXXXX", ".XXXXX.", "..XXX..", "...X..."};
    for (int j = 0; j < 6; ++j)
        for (int i = 0; i < 7; ++i) {
            if (shape[j][i] != 'X') continue;
            const bool on = fill == 2 || (fill == 1 && i < 4);
            if (on) SDL_SetRenderDrawColor(r, 205, 55, 55, 255);
            else SDL_SetRenderDrawColor(r, 8, 56, 64, 255);
            SDL_FRect px{x + i, y + j, 1, 1};
            SDL_RenderFillRect(r, &px);
        }
}

static void drawWorld(Game& g, float cx, float cy) {
    SDL_Renderer* r = g.renderer;
    const Hero& h = g.hero;

    // Entites intercalees entre les rangees du decor (profondeur)
    struct Drawable { float y; int kind; size_t i; };
    std::vector<Drawable> ds;
    ds.push_back({h.y, 0, 0});
    for (size_t i = 0; i < g.enemies.size(); ++i) ds.push_back({g.enemies[i].y, 1, i});
    for (size_t i = 0; i < g.npcs.size(); ++i) ds.push_back({g.npcs[i].y, 2, i});
    for (size_t i = 0; i < g.pickups.size(); ++i) ds.push_back({g.pickups[i].y, 3, i});
    for (size_t i = 0; i < g.chests.size(); ++i) ds.push_back({g.chests[i].y, 4, i});
    std::vector<const InteractionDef*> objects; // objets interactifs du remake (zone courante)
    const std::string zoneId = zoneName(g);
    for (const auto& [id, o] : g.interactionDefs)
        if (o.zone == zoneId) {
            ds.push_back({o.y * float(TILE_H) + TILE_H / 2.f + 3.f, 5, objects.size()});
            objects.push_back(&o);
        }
    std::sort(ds.begin(), ds.end(), [](const Drawable& a, const Drawable& b) { return a.y < b.y; });
    size_t next = 0;

    auto shadow = [&](float x, float y, int z, float w) {
        fxShadow(r, x - cx, screenY(y, z, cy), w + 4);

    };

    auto drawOne = [&](const Drawable& d) {
        if (d.kind == 0) {
            if (g.freeCam) return;
            if (h.dashTicks > 0) // traînee de l'esquive
                for (size_t i = 1; i < h.trail.size(); ++i)
                    g.heroSprites.drawFrame(r, h.trailFrame[i], h.trail[i].x - cx, screenY(h.trail[i].y, h.z, cy), 1.f,
                                            255, 255, 255, Uint8(110 - i * 25), true);
            const bool blink = h.invuln > 0 && (h.invuln / 4) % 2;
            if (!blink) {
                shadow(h.x, h.y, h.z, std::max(6.f, 14 - h.air * 0.15f));
                g.heroSprites.drawFrame(r, h.anim.frame(), h.x - cx, screenY(h.y, h.z, cy) - std::floor(h.air));
                if (h.charge >= 60 && h.charge < 66) // bref eclat : la charge est prete
                    g.heroSprites.drawFrame(r, h.anim.frame(), h.x - cx, screenY(h.y, h.z, cy), 1.f, 255, 255, 255, 120, true);
            }
        } else if (d.kind == 1) {
            const Enemy& e = g.enemies[d.i];
            const EnemyDef& def = enemyDef(e.kind);
            SpriteBank* b = spriteBank(g, def.sprite);
            if (!b) return;
            float ex = e.x - cx;
            Uint8 cr = 255, cg = 255, cb = 255;
            if (e.state == 3) { // l'ogre prepare sa charge : tremble et rougit
                cg = cb = Uint8(110 + 100 * ((g.frame / 3) % 2));
                ex += ((g.frame / 2) % 2) ? 1.f : -1.f;
            }
            if (e.stun > 0) { cr = 210; cg = 200; cb = 170; }
            shadow(e.x, e.y, e.z, def.radius * 2);
            const float sy = screenY(e.y, e.z, cy);
            b->drawFrame(r, e.anim.frame(), ex, sy, def.scale, cr, cg, cb);
            if (e.flash > 0 && (e.flash / 2) % 2) // eclair blanc a l'impact
                b->drawFrame(r, e.anim.frame(), ex, sy, def.scale, 255, 255, 255, 220, true);
            if (e.stun > 0) {
                SDL_SetRenderDrawColor(r, 255, 255, 120, 255);
                for (int k = 0; k < 3; ++k) { // etoiles qui tournent
                    const float a = g.frame * 0.15f + k * 2.094f;
                    SDL_FRect st{std::floor(ex + std::cos(a) * 9), std::floor(sy - 36 * def.scale + std::sin(a) * 3), 2, 2};
                    SDL_RenderFillRect(r, &st);
                }
            }
            if (e.kind == EnemyKind::Ogre) { // barre de vie du mini-boss
                SDL_SetRenderDrawColor(r, 40, 0, 0, 220);
                SDL_FRect bg{std::floor(ex) - 16, sy - 58, 32, 3};
                SDL_RenderFillRect(r, &bg);
                SDL_SetRenderDrawColor(r, 230, 60, 40, 255);
                SDL_FRect fg{bg.x, bg.y, 32.f * std::max(0, e.hp) / def.hp, 3};
                SDL_RenderFillRect(r, &fg);
            }
        } else if (d.kind == 2) {
            const Npc& n = g.npcs[d.i];
            shadow(n.x, n.y, n.z, 14);
            if (SpriteBank* b = spriteBank(g, n.sprite)) b->drawFrame(r, n.anim.frame(), n.x - cx, screenY(n.y, n.z, cy));
            if (std::hypot(n.x - h.x, n.y - h.y) < 34 && !g.box.active()) { // bulle "parler"
                const float bx = std::floor(n.x - cx) - 2, by = screenY(n.y, n.z, cy) - 58 + ((g.frame / 15) % 2);
                SDL_SetRenderDrawColor(r, 24, 41, 8, 255); // contour sombre (encre FONT3)
                for (const auto& o : {SDL_FPoint{-1, 0}, SDL_FPoint{1, 0}, SDL_FPoint{0, -1}, SDL_FPoint{0, 1}})
                    drawText(r, bx + o.x, by + o.y, "!");
                SDL_SetRenderDrawColor(r, 248, 248, 232, 255);
                drawText(r, bx, by, "!");
            }
        } else if (d.kind == 4) {
            const Chest& c = g.chests[d.i];
            shadow(c.x, c.y, c.z, 20);
            if (SpriteBank* b = spriteBank(g, "coffre")) b->drawFrame(r, c.anim.frame(), c.x - cx, screenY(c.y, c.z, cy));
        } else if (d.kind == 5) {
            const InteractionDef& o = *objects[d.i];
            const float ox = o.x * float(TILE_W) + TILE_W / 2.f, oy = o.y * float(TILE_H) + TILE_H / 2.f + 3.f;
            const Cell* cell = g.map->cell(o.x, o.y);
            const int oz = cell ? cell->height : h.z;
            const bool done = (g.interactionStates.count(o.id) && g.interactionStates.at(o.id) == 1) ||
                              (o.type == "mechanism" && g.seenEvents.count(o.result));
            if (SpriteBank* b = spriteBank(g, "rm_" + o.type)) {
                shadow(ox, oy, oz, 16);
                b->drawFrame(r, done ? 1 : 0, ox - cx, screenY(oy, oz, cy));
            } else { // repere provisoire tant que le sprite original n'existe pas (pas un asset final)
                const SDL_Color c = o.type == "flammable"     ? SDL_Color{230, 120, 40, 255}
                                    : o.type == "ice_surface" ? SDL_Color{140, 210, 240, 255}
                                    : o.type == "fragile"     ? SDL_Color{170, 160, 140, 255}
                                    : o.type == "lever"       ? SDL_Color{200, 180, 90, 255}
                                                              : SDL_Color{120, 120, 130, 255};
                SDL_SetRenderDrawColor(r, done ? 90 : c.r, done ? 200 : c.g, done ? 90 : c.b, 255);
                const SDL_FRect mark{std::floor(ox - cx) - 5, screenY(oy, oz, cy) - 12, 10, 10};
                SDL_RenderFillRect(r, &mark);
            }
        } else {
            const Pickup& p = g.pickups[d.i];
            if (p.life < 120 && (p.life / 5) % 2) return;
            drawHeart(r, std::floor(p.x - cx) - 3, screenY(p.y, p.z, cy) - 10 - std::sin(g.frame * 0.1f) * 2, 2);
        }
    };

    g.map->draw(r, cx, cy, VIEW_W, VIEW_H, [&](int row) {
        while (next < ds.size() && int(ds[next].y / TILE_H) <= row) drawOne(ds[next++]);
    });
    while (next < ds.size()) drawOne(ds[next++]);

    for (const auto& p : g.particles) {
        SDL_SetRenderDrawColor(r, p.color.r, p.color.g, p.color.b, Uint8(255 * p.life / std::max(1, p.maxLife)));
        SDL_FRect rc{std::floor(p.x - cx), std::floor(Map::MARGIN_ROWS * TILE_H + p.y - cy - p.z), p.size, p.size};
        SDL_RenderFillRect(r, &rc);
    }
    SpriteBank* ui = spriteBank(g, "a1_ui"); // chiffres dores natifs (images 18..27)
    for (const auto& t : g.texts) {
        const float ty = screenY(t.y, h.z, cy);
        const Uint8 alpha = Uint8(std::min(255, t.life * 8));
        const bool number = !t.text.empty() && std::all_of(t.text.begin(), t.text.end(), [](char c) { return c >= '0' && c <= '9'; });
        if (number && ui) { // degats : chiffres dores 8x16 d'Alundra 1
            float x = std::floor(t.x - cx - 4.f * float(t.text.size()));
            for (char c : t.text) {
                ui->drawFrame(r, 18 + (c - '0'), x, ty - 8, 1.f, 255, 255, 255, alpha);
                x += 8.f;
            }
            continue;
        }
        const std::string latin1 = foldAccents(t.text);
        const float tx = std::floor(t.x - cx - textWidthLatin1(latin1) / 2.f);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        drawTextLatin1(r, tx + 1, ty - 7, latin1);
        SDL_SetRenderDrawColor(r, t.color.r, t.color.g, t.color.b, 255);
        drawTextLatin1(r, tx, ty - 8, latin1);
    }
}

// Objet sorti d'un coffre : il monte du coffre jusqu'au-dessus du heros et y reste pendant le message
static void drawGotItem(Game& g, float cx, float cy) {
    float x, y;
    if (!gotItemPos(g, x, y)) return;
    // Le gantelet et les ordres n'ont pas de sprite d'objet : on reutilise les eclats / la lettre.
    const std::string sprite = g.gotItem == "gantelet" ? "fx_orbe" : g.gotItem == "ordre" ? "lettre" : g.gotItem;
    const float sy = std::floor(Map::MARGIN_ROWS * TILE_H + y - cy);
    // Objet sans sprite de terrain (cle etrange, materiaux et armes du remake...) : son icone
    // d'inventaire (A1 native ou originale), centree au-dessus du heros.
    if (!itemSpriteExists(g, sprite)) {
        drawItemIcon(g, g.gotItem, std::floor(x - cx) - 12.f, sy - 24.f);
        return;
    }
    SpriteBank* b = spriteBank(g, sprite);
    const auto* a = b ? b->anim(0, DOWN) : nullptr;
    if (!a || a->steps.empty()) return;
    b->drawFrame(g.renderer, a->steps[0].frame, x - cx, sy + 8);
    if (g.gotTicks == 0 && (g.frame / 6) % 8 == 0) // reflet bref, comme l'eclat du lingot dans le jeu
        b->drawFrame(g.renderer, a->steps[0].frame, x - cx, sy + 8, 1.f, 255, 255, 255, 140, true);
}

// Effets animes d'Alundra 1 : dessines deux fois (normal puis additif) pour retrouver leur eclat
static void drawEffects(Game& g, float cx, float cy) {
    for (const auto& e : g.effects) {
        if (e.delay > 0) continue;
        SpriteBank* b = spriteBank(g, e.sprite);
        if (!b) continue;
        const float sy = std::floor(Map::MARGIN_ROWS * TILE_H + e.y - e.z * TILE_H - cy);
        b->drawFrame(g.renderer, e.anim.frame(), e.x - cx, sy, e.scale);
        b->drawFrame(g.renderer, e.anim.frame(), e.x - cx, sy, e.scale, 255, 255, 255, 60, true);
    }
}

static void drawHud(Game& g) {
    SDL_Renderer* r = g.renderer;
    const Hero& h = g.hero;
    // Petite etiquette avec ombre 1 px : rendu "jeu" au lieu du texte de debug SDL
    auto label = [&](float x, float y, const std::string& s, SDL_Color c) {
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        drawText(r, x + 1, y + 1, s);
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
        drawText(r, x, y, s);
    };
    // HUD d'Alundra 1, mesure au pixel sur les captures A1 (docs/visuel/a1_hud_layout.md) et
    // relu par superposition : banque native a1_hud.a1s (WIND.TX, banques CLUT 5 et 7).
    // Correspondance A2 : 1 petite gemme = 1 demi-coeur ; 4 cristaux de 50 EP ; arme equipee et
    // objet de soin rapide dans les deux cases ; or (pas encore d'economie : 0000).
    if (SpriteBank* hud = spriteBank(g, "a1_hud")) {
        enum : int { kBox = 0, kBigFull = 1, kBigEmpty = 4, kSmallFull = 5, kSmallEmpty = 8, kDigit0 = 9,
                     kSlash = 19, kMpFull0 = 20, kMpEmpty = 24, kGoldBag = 32 };
        auto digits = [&](float x, float y, int value, int width) {
            std::string s = std::to_string(std::clamp(value, 0, width >= 4 ? 9999 : 99));
            s = std::string(size_t(std::max(0, width - int(s.size()))), '0') + s;
            for (char c : s) { hud->drawFrame(r, kDigit0 + (c - '0'), x, y); x += 8.f; }
        };
        // Cases : arme equipee (16,16) et objet de soin rapide (48,16), icone du menu a l'origine.
        const std::string weaponId = h.weapon == Weapon::Eclair ? "gantelet"
                                     : h.weapon == Weapon::Supreme ? "epee_supreme" : g.inventory.weapon;
        const std::string itemId = g.items.count("herbe") && g.items.at("herbe") > 0 ? "herbe" : g.inventory.pocket;
        const std::string boxes[2] = {weaponId, itemId};
        for (int k = 0; k < 2; ++k) {
            const float bx = k == 0 ? 16.f : 48.f;
            hud->drawFrame(r, kBox, bx, 16.f);
            if (!boxes[k].empty()) drawItemIcon(g, boxes[k], bx, 16.f);
        }
        // Vie : grosses gemmes (une par dizaine au-dela de 10, la plus a droite se remplit d'abord),
        // puis 10 petites gemmes en deux rangees de 5.
        const int nBig = std::max(0, (h.maxHp + 9) / 10 - 1);
        const int fullBig = h.hp > 0 ? std::min(nBig, (h.hp - 1) / 10) : 0;
        const int small = std::max(0, h.hp - 10 * fullBig);
        for (int k = 0; k < nBig; ++k) hud->drawFrame(r, k < fullBig ? kBigFull : kBigEmpty, 104.f - 8.f * float(k), 16.f);
        for (int i = 0; i < 10; ++i)
            hud->drawFrame(r, i < small ? kSmallFull : kSmallEmpty, 120.f + 8.f * float(i % 5), 16.f + 8.f * float(i / 5));
        hud->drawFrame(r, kSlash, 160.f, 16.f);
        digits(168.f, 16.f, h.maxHp, 2);
        // Energie : 4 cristaux de 50 EP ; un cristal plein s'anime (4 images, decalees d'une image).
        const int crystals = std::max(1, std::min(4, h.maxEp / 50));
        for (int k = 0; k < crystals; ++k) {
            const bool full = h.ep >= 50 * (k + 1);
            const int frame = full ? kMpFull0 + int((g.frame / 8 + uint64_t(k)) % 4) : kMpEmpty;
            hud->drawFrame(r, frame, 216.f + 8.f * float(k), 16.f);
        }
        digits(256.f, 16.f, g.items.count("or") ? g.items.at("or") : 0, 4);
        hud->drawFrame(r, kGoldBag, 288.f, 16.f);
    }
    // Nom du lieu, comme dans A1 : une ligne centree dans le parchemin de dialogue standard
    // (16,168,288,56), en 2e ligne, sans portrait ni cartouche ; attend la fin d'un dialogue.
    if (g.bannerTicks > 0 && !g.box.active()) {
        --g.bannerTicks;
        drawA1Window(r, 16, 168, 288, 56);
        const std::string latin1 = foldAccents(g.banner);
        SDL_SetRenderDrawColor(r, 41, 49, 16, 255);
        drawTextLatin1(r, std::floor(160.f - (textWidthLatin1(latin1) + 1.f) / 2.f), 188.f, latin1);
    }
    // Aides au dev/au r�glage : invisibles en jeu, comme sur la PS1 d'origine
    if (g.debug) {
        std::string obj;
        if (!g.enemies.empty()) obj = "Monstres : " + std::to_string(g.enemies.size());
        else if (g.cleared[zoneName(g)]) obj = "Zone liberee";
        if (!obj.empty()) {
            SDL_SetRenderDrawColor(r, 0, 0, 0, 140);
            SDL_FRect bg{VIEW_W - 8.f * obj.size() - 10, 4, 8.f * obj.size() + 6, 11};
            SDL_RenderFillRect(r, &bg);
            label(VIEW_W - 8.f * obj.size() - 7, 6, obj, SDL_Color{255, 230, 160, 255});
        }
        SDL_SetRenderDrawColor(r, 45, 32, 24, 200);
        SDL_FRect bg{6, 15, 25, 2};
        SDL_RenderFillRect(r, &bg);
        SDL_SetRenderDrawColor(r, 210, 170, 110, 255);
        SDL_FRect bar{6, 15, 25.f * (1.f - h.dashCooldown / 36.f), 2};
        SDL_RenderFillRect(r, &bar);
        if (g.items.count("gantelet") || g.seenEvents.count("epee_supreme")) { // arme equipee (Q pour alterner)
            const char* weapon = h.weapon == Weapon::Eclair ? "Q: ECLAIR" : h.weapon == Weapon::Supreme ? "Q: SUPREME" : "Q: EPEE";
            label(36, 13, weapon, SDL_Color{220, 200, 150, 255});
        }
        // Numero du morceau (apres M / N) : pour choisir la musique de la zone (F6)
        if (g.music.shownTicks > 0) {
            --g.music.shownTicks;
            auto kind = g.zoneKind.find(zoneName(g));
            const std::string t = "Musique " + std::to_string(g.music.track) +
                                  (kind != g.zoneKind.end() ? " (" + kind->second + ")" : "") + "  F6 : garder";
            SDL_SetRenderDrawColor(r, 0, 0, 0, 150);
            SDL_FRect mb{4, VIEW_H - 16.f, 8.f * t.size() + 6, 11};
            SDL_RenderFillRect(r, &mb);
            label(7, VIEW_H - 14.f, t, SDL_Color{220, 225, 190, 255});
        }
    }
}

static void drawDebug(Game& g, float cx, float cy) {
    SDL_Renderer* r = g.renderer;
    const Map& m = *g.map;
    const std::string zone = zoneName(g);
    for (int y = 0; y < m.height(); ++y)
        for (int x = 0; x < m.width(); ++x) {
            const Cell* c = m.cell(x, y);
            if (!c || c->top == 0xFFFF) continue;
            SDL_FRect rc{x * float(TILE_W) - cx, (Map::MARGIN_ROWS + y - c->height) * float(TILE_H) - cy, float(TILE_W), float(TILE_H)};
            if (rc.x + rc.w < 0 || rc.x > VIEW_W || rc.y + rc.h < 0 || rc.y > VIEW_H) continue;
            bool fill = true;
            if (g.warps.find(zone, x, y)) SDL_SetRenderDrawColor(r, 0, 220, 255, 120);
            else if (c->attr & ATTR_SOLID) SDL_SetRenderDrawColor(r, 255, 0, 0, 70);
            else if (c->attr & ATTR_EDGE) SDL_SetRenderDrawColor(r, 255, 140, 0, 90);
            else if (c->attr & ATTR_STONE) SDL_SetRenderDrawColor(r, 0, 255, 0, 40);
            else fill = false;
            if (fill) SDL_RenderFillRect(r, &rc);
        }
    const Hero& h = g.hero;
    const int hx = int(h.x / TILE_W), hy = int(h.y / TILE_H);
    const Cell* c = m.cell(hx, hy);
    char line[160];
    SDL_SetRenderDrawColor(r, 0, 0, 0, 170);
    SDL_FRect bg{0, VIEW_H - 24.f, float(VIEW_W), 24};
    SDL_RenderFillRect(r, &bg);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_snprintf(line, sizeof line, "zone %s case %d,%d h=%d attr=%02X", zone.c_str(), hx, hy, c ? c->height : -1, c ? c->attr : 0);
    SDL_RenderDebugText(r, 4, VIEW_H - 21.f, line);
    SDL_snprintf(line, sizeof line, "%sF2/F3 passage F4 suppr F5 monstre", g.pendingWarp ? "[depart] " : "");
    SDL_RenderDebugText(r, 4, VIEW_H - 11.f, line);
}

// Verifications deterministes supplementaires (menu, equipement, quantites).
// Les tokens inconnus sont ignores : les verifications historiques restent telles quelles.
static bool checkInventoryExpect(const Game& g, const std::string& token) {
    auto value = [&](const std::string& prefix) -> std::string {
        return token.rfind(prefix, 0) == 0 ? token.substr(prefix.size()) : std::string();
    };
    if (token == "menu=open") return g.menu.open;
    if (token == "menu=closed") return !g.menu.open;
    if (token == "noattack=1") return !g.autoAttackSeen;
    if (token.rfind("page=", 0) == 0) {
        const std::string p = value("page=");
        return (p == "weapons" && g.menu.page == MenuPage::Weapons) ||
               (p == "items" && g.menu.page == MenuPage::Items) ||
               (p == "equipment" && g.menu.page == MenuPage::Equipment) ||
               (p == "forge" && g.menu.page == MenuPage::Forge);
    }
    if (token.rfind("weapon=", 0) == 0) return g.inventory.weapon == value("weapon=");
    if (token.rfind("choice=", 0) == 0) return g.lastChoice == value("choice=");
    if (token.rfind("gridcursor=", 0) == 0) return int(g.menu.gridCursor) == std::atoi(value("gridcursor=").c_str());
    if (token.rfind("ring=", 0) == 0) return g.inventory.ring == (value("ring=") == "none" ? "" : value("ring="));
    if (token.rfind("pocket=", 0) == 0) return g.inventory.pocket == (value("pocket=") == "none" ? "" : value("pocket="));
    if (token.rfind("swordPower=", 0) == 0) return g.hero.swordPower == std::atoi(value("swordPower=").c_str());
    if (token.rfind("ep=", 0) == 0) return g.hero.ep == std::atoi(value("ep=").c_str());
    // Tranche 3 (ajouts du remake)
    if (token.rfind("zone=", 0) == 0) return zoneName(g) == value("zone=");
    if (token.rfind("dialogue=", 0) == 0) return g.box.id == value("dialogue=");
    if (token == "ending") return true; // verifie avec diagnostic detaille plus bas
    if (token.rfind("rmweapon=", 0) == 0) return g.rmWeapons.count(value("rmweapon=")) > 0;
    if (token.rfind("rmrecipe=", 0) == 0) return int(g.forgeRecipes.size()) == std::atoi(value("rmrecipe=").c_str());
    if (token.rfind("effect=", 0) == 0) return g.lastEffect == value("effect=");
    if (token.rfind("damage=", 0) == 0) return g.lastDamage == std::atoi(value("damage=").c_str());
    if (token.rfind("forge=", 0) == 0) {
        const std::string want = value("forge=");
        return want == "none" ? g.upgrades.empty() : g.upgrades.count(want) > 0;
    }
    if (token.rfind("upgrades=", 0) == 0) return int(g.upgrades.size()) == std::atoi(value("upgrades=").c_str());
    if (token.rfind("puzzle=", 0) == 0) return puzzleProgress(g, "rm_demo_gate") == std::atoi(value("puzzle=").c_str());
    if (token.rfind("interaction:", 0) == 0 || token.rfind("event:", 0) == 0) {
        const bool isEvent = token.rfind("event:", 0) == 0;
        const std::string rest = value(isEvent ? "event:" : "interaction:");
        const auto eq = rest.find('=');
        if (eq == std::string::npos) return false;
        const std::string key = rest.substr(0, eq);
        const int want = std::atoi(rest.substr(eq + 1).c_str());
        if (isEvent) return int(g.seenEvents.count(key)) == want;
        const auto it = g.interactionStates.find(key);
        return (it == g.interactionStates.end() ? 0 : it->second) == want;
    }
    if (token.rfind("qty:", 0) == 0) {
        const std::string rest = value("qty:");
        const auto eq = rest.find('=');
        if (eq == std::string::npos) return false;
        const std::string id = rest.substr(0, eq);
        const int expected = std::atoi(rest.substr(eq + 1).c_str());
        const auto it = g.items.find(id);
        return expected == 0 ? (it == g.items.end() || it->second == 0)
                             : (it != g.items.end() && it->second == expected);
    }
    // Jeton inconnu (faute de frappe) : le test echoue au lieu de passer a vide (revue QA).
    SDL_Log("Jeton --expect inconnu : %s", token.c_str());
    return false;
}

SDL_AppResult SDL_AppIterate(void* state) {
    Game& g = *static_cast<Game*>(state);
    static Uint64 last = SDL_GetTicks();
    static float acc = 0;
    const Uint64 now = SDL_GetTicks();
    acc += std::min(0.25f, (now - last) / 1000.f);
    last = now;
    if (!g.autoScript.empty()) acc = TICK; // mode test : un tick par image, deterministe
    while (acc >= TICK) {
        tick(g);
        acc -= TICK;
    }

    SDL_Renderer* r = g.renderer;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    if (g.diagStage > 0) { // diagnostic visuel deterministe : aucun decor, fond uni
        SDL_SetRenderDrawColor(r, 48, 48, 48, 255);
        SDL_RenderFillRect(r, nullptr);
        drawDiagnostic(g);
    } else if (g.mode == Mode::Splash || g.mode == Mode::Title || g.mode == Mode::Prologue) {
        frontDraw(g);
    } else {
        if (g.map) {
            g.camX = std::clamp(g.camX, 0.f, std::max(0.f, float(g.map->pixelWidth() - VIEW_W)));
            g.camY = std::clamp(g.camY, 0.f, std::max(0.f, float(g.map->pixelHeight() - VIEW_H)));
        }
        float cx = std::floor(g.camX), cy = std::floor(g.camY);
        if (g.shake > 0) { cx += float(SDL_rand(5) - 2); cy += float(SDL_rand(5) - 2); }
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        SDL_RenderClear(r);
        if (g.map) {
            drawWorld(g, cx, cy);
            drawEffects(g, cx, cy);
            drawGotItem(g, cx, cy);
            fxAmbient(g, cx, cy);
            if (g.debug) drawDebug(g, cx, cy);
        }
        if (!g.menu.open && g.mode == Mode::Play) drawHud(g); // ni HUD ni bandeau dans le menu A1, le Game Over et la fin
        drawDialogue(g);
        drawMenu(g); // par-dessus le decor ; ne dessine rien quand le menu est ferme
        // Game Over et generique : voile noir puis messages dans le parchemin A1 natif (encre
        // FONT3), comme les panneaux de message d'Alundra 1 ; plus de voile teal (style A2 abandonne).
        auto centeredInk = [&](float panelX, float panelW, float y, const std::string& utf8) {
            const std::string latin1 = foldAccents(utf8);
            SDL_SetRenderDrawColor(r, 41, 49, 16, 255);
            drawTextLatin1(r, std::floor(panelX + (panelW - textWidthLatin1(latin1)) / 2.f), y, latin1);
        };
        if (g.mode == Mode::GameOver) {
            SDL_SetRenderDrawColor(r, 0, 0, 0, Uint8(std::min(170, g.modeTimer * 4)));
            SDL_FRect full{0, 0, float(VIEW_W), float(VIEW_H)};
            SDL_RenderFillRect(r, &full);
            if (g.modeTimer > 20) {
                drawA1Window(r, 16, 92, 288, 54);
                centeredInk(16, 288, 96, "Flint est tomb�...");
                if (g.modeTimer > 40) centeredInk(16, 288, 112, "Valider : se relever");
            }
        }
        if (g.mode == Mode::Ending) {
            SDL_SetRenderDrawColor(r, 0, 0, 0, Uint8(std::min(225.f, float(g.modeTimer) * 2.2f)));
            SDL_FRect full{0, 0, float(VIEW_W), float(VIEW_H)};
            SDL_RenderFillRect(r, &full);
            // Generique simple : les etapes de la trame d'Alundra 2, dans l'ordre (resume du remake).
            const char* lines[] = {
                "Le roi revient au tr�ne.",
                "Alexia reprend sa place.",
                "Diaz est arr�t�.",
                "M�phisto est vaincu.",
                "Flint rentre � Paco.",
                "Varuna est lib�r�e.",
            };
            if (g.modeTimer >= 25) drawA1Window(r, 16, 24, 288, 112);
            for (int i = 0; i < 6; ++i) {
                if (g.modeTimer < 25 + i * 45) break;
                centeredInk(16, 288, 28.f + float(i) * 16.f, lines[i]);
            }
            if (g.modeTimer > 320) {
                drawA1Window(r, 64, 150, 192, 54);
                centeredInk(64, 192, 154, "Fin");
                centeredInk(64, 192, 170, "Valider : retour au titre");
            }
        }
        if (g.front.fadeIn > 0) { // on sort du prologue : le jeu apparait en fondu
            SDL_SetRenderDrawColor(r, 0, 0, 0, Uint8(255 * g.front.fadeIn / 50));
            SDL_FRect full{0, 0, float(VIEW_W), float(VIEW_H)};
            SDL_RenderFillRect(r, &full);
            --g.front.fadeIn;
        }
    }

    if (g.autoDone) {
        if (g.autoExpect == "ending" &&
            (g.mode != Mode::Ending || !g.seenEvents.count("mephisto_vaincu"))) {
            SDL_Log("Test --expect ending : fin non atteinte (mode %d, evenement mephisto_vaincu %d)",
                    int(g.mode), int(g.seenEvents.count("mephisto_vaincu")));
            SDL_Log("  zone %s ; heros (%.0f,%.0f) dir %d pv %d/%d ; ennemis :", zoneName(g).c_str(),
                    g.hero.x, g.hero.y, int(g.hero.dir), g.hero.hp, g.hero.maxHp);
            for (const auto& e : g.enemies)
                if (e.alive) SDL_Log("  ennemi %d (%.0f,%.0f) pv %d", int(e.kind), e.x, e.y, e.hp);
            return SDL_APP_FAILURE;
        }
        const std::string firstExpect = g.autoExpect.substr(0, g.autoExpect.find(',')); // premier jeton
        if (firstExpect.rfind("dialogue=", 0) == 0 && g.box.id != firstExpect.substr(9)) {
            SDL_Log("Test --expect %s : dernier dialogue '%s'", g.autoExpect.c_str(), g.box.id.c_str());
            return SDL_APP_FAILURE;
        }
        if (firstExpect.rfind("zone=", 0) == 0 && zoneName(g) != firstExpect.substr(5)) {
            SDL_Log("Test --expect %s : zone finale %s, heros (%.0f,%.0f) cellule (%d,%d), dialogue %d, onWarp %d",
                    g.autoExpect.c_str(), zoneName(g).c_str(), g.hero.x, g.hero.y,
                    int(g.hero.x / TILE_W), int(g.hero.y / TILE_H), int(g.box.active()), int(g.onWarp));
            return SDL_APP_FAILURE;
        }
        if (!g.autoExpect.empty()) {
            std::istringstream expected(g.autoExpect);
            std::string token;
            while (std::getline(expected, token, ',')) {
                if (!checkInventoryExpect(g, token)) {
                    SDL_Log("Test --expect %s : etat final invalide", token.c_str());
                    SDL_Log("  menu=%d page=%d arme=%s puissance=%d energie=%d effet=%s degats=%d enigme=%d forge=%zu",
                            int(g.menu.open), int(g.menu.page), g.inventory.weapon.c_str(), g.hero.swordPower,
                            g.hero.ep, g.lastEffect.c_str(), g.lastDamage, puzzleProgress(g, "rm_demo_gate"),
                            g.upgrades.size());
                    return SDL_APP_FAILURE;
                }
            }
        }
        if (!g.autoShot.empty())
            if (SDL_Surface* s = SDL_RenderReadPixels(r, nullptr)) {
                SDL_SaveBMP(s, g.autoShot.c_str());
                SDL_DestroySurface(s);
            }
        return SDL_APP_SUCCESS;
    }
    SDL_RenderPresent(r);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* state, SDL_AppResult) {
    auto* g = static_cast<Game*>(state);
    if (!g) return;
    g->map.reset();
    g->sprites.clear();
    if (g->pad) SDL_CloseGamepad(g->pad);
    if (g->music.stream) SDL_DestroyAudioStream(g->music.stream);
    delete g;
}
