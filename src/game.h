#pragma once
// Etat global du jeu : heros, monstres, PNJ, particules, dialogues.
#include <SDL3/SDL.h>

#include <array>
#include <filesystem>
#include <iosfwd>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "map.h"
#include "sprite.h"
#include "warps.h"

constexpr int VIEW_W = 320;
constexpr int VIEW_H = 240;
constexpr float TICK = 1.f / 60.f;

enum Dir { DOWN = 0, UP = 1, LEFT = 2, RIGHT = 3 };
inline float dirX(Dir d) { return d == LEFT ? -1.f : d == RIGHT ? 1.f : 0.f; }
inline float dirY(Dir d) { return d == UP ? -1.f : d == DOWN ? 1.f : 0.f; }

// --- Attributs des cases (voir tools/export_assets.py) -----------------------
constexpr uint8_t ATTR_SOLID = 0x01, ATTR_STONE = 0x02, ATTR_EDGE = 0x10, ATTR_SPECIAL = 0x80;
constexpr uint8_t ATTR_BLOCK = ATTR_SOLID | ATTR_EDGE;

bool canStand(const Map& m, float x, float y, int z, float halfW = 6, float depth = 3);

// --- Entrees (etat courant + fronts montants) --------------------------------
struct Input {
    float dx = 0, dy = 0;
    bool run = false;
    bool attack = false, attackPressed = false, attackReleased = false;
    bool dash = false, dashPressed = false;
    bool jump = false, jumpPressed = false;
    bool menu = false, menuPressed = false; // M / START : ouverture du menu A1
};

// --- Heros -------------------------------------------------------------------
// Armes du remake : l'epee d'Alundra 1, puis le gantelet d'eclair d'Alundra 2
// (objet "Gantelet permettant de lancer l'attaque Eclair" dans les menus du jeu).
enum class Weapon { Sword, Eclair, Supreme };

struct Hero {
    float x = 0, y = 0;
    int z = 0;
    Dir dir = DOWN;
    bool moving = false;
    AnimPlayer anim;

    int hp = 10, maxHp = 10;  // en demi-coeurs (5 coeurs)
    int ep = 0, maxEp = 200; // points d'energie A2 (consommables lorien_nuts / grapes_of_eru)
    int invuln = 0;         // ticks d'invulnerabilite apres un coup
    float kx = 0, ky = 0;   // recul

    // Saut (mecanique centrale d'Alundra 1) : `air` = hauteur des pieds au-dessus du sol de la case, en pixels
    bool airborne = false;
    float air = 0, vz = 0;
    int jumpZ = 0;              // niveau du sol au moment de l'appel
    float safeX = 0, safeY = 0; // derniere position sure au sol (retour apres une chute dans le vide)

    // Attaque et enchainements (nouveaute : combo en 3 coups)
    bool attacking = false;
    int attackTick = 0;
    int swingId = 0;
    int combo = 0;          // 1..3 (4 = coup charge, 5 = decharge du gantelet)
    Weapon weapon = Weapon::Sword;
    int swordPower = 1;     // puissance A2 1..6 de l'epee equipee (donnees items.txt)
    int sinceAttack = 999;  // ticks depuis la fin du dernier coup
    int charge = 0;         // ticks de maintien du bouton d'epee (coup charge d'Alundra 1 : combo = 4)
    bool queuedAttack = false; // appui memorise pendant un coup (enchainement fluide)

    // Esquive roulee (nouveaute : traversee invulnerable avec traÃ®nee)
    int dashTicks = 0, dashCooldown = 0;
    int hurtTicks = 0; // animation "touche"
    float dashX = 0, dashY = 0;
    std::array<SDL_FPoint, 4> trail{};
    std::array<int, 4> trailFrame{};
};

// --- Monstres ----------------------------------------------------------------
enum class EnemyKind { Gelee, MiniGelee, Tortue, Gobelin, Ogre, Droid, Golem, Capitaine, Mephisto, Count };

struct EnemyDef {
    const char* id;      // nom dans data/spawns.txt
    const char* sprite;  // fichier assets/sprites/<sprite>.a1s
    int hp;
    float speed;
    int damage;          // en demi-coeurs
    float radius;
    float scale;
};
const EnemyDef& enemyDef(EnemyKind k);
std::optional<EnemyKind> enemyKindFromId(const std::string& id);

struct Enemy {
    EnemyKind kind;
    float x, y;
    int z;
    int hp;
    Dir dir = DOWN;
    int state = 0;        // propre a chaque type (voir entities.cpp)
    int timer = 0;
    float vx = 0, vy = 0; // deplacement voulu
    float kx = 0, ky = 0; // recul
    int flash = 0;        // clignotement apres un coup
    int lastSwing = -1;   // evite de toucher deux fois avec le meme coup
    int stun = 0;
    int slow = 0;         // effet "ice" (armes du remake) : ticks de deplacement ralenti
    bool alive = true;
    AnimPlayer anim;
};

// --- PNJ ---------------------------------------------------------------------
struct Npc {
    std::string zone, sprite, dialogue;
    float x, y;
    int z;
    Dir dir;
    AnimPlayer anim;
};

// --- Coffres (items.cpp) -------------------------------------------------------
struct Chest {
    std::string zone, item;
    int cx = 0, cy = 0; // case declaree dans world.txt (identifiant du coffre)
    float x = 0, y = 0;
    int z = 0;
    bool open = false;
    int openTicks = 0;
    AnimPlayer anim;
};

// --- Objets A2, inventaire et menu (items.cpp / menu.cpp) ---------------------
// Contenu A2 (noms, descriptions, types) ; la presentation reste celle d'Alundra 1.
enum class ItemKind { Consumable, Key, Weapon, Ring, Pocket };

struct ItemDef {
    std::string id, name, description;
    ItemKind kind = ItemKind::Key;
    int maxQty = 1;
    bool consumable = false;
    int power = 0;
};

struct Inventory {
    std::string weapon = "long_sword";
    std::string pocket;
    std::string ring;
};

// Forge : quatrieme page, ouverte uniquement par le forgeron Orvan (ajout du remake).
enum class MenuPage { Weapons, Items, Equipment, Forge };

// --- Ajouts du remake (tranche 3) : armes d'exploration, forge, interactions ---
// Donnees : data/rm_weapons.txt, rm_forge.txt, rm_materials.txt, rm_interactions.txt.
// Effets generiques ("none", "fire", "ice", "heavy") : aucun traitement par arme dans le code.
struct RemakeWeaponDef {
    std::string id, name, description, effect;
    int damage = 0;  // degats d'un coup normal
    int speed = 14;  // duree de la fenetre active du coup, en ticks (epee A2 : 14)
    int range = 18;  // distance de la zone de frappe, en pixels (epee A2 : 18)
    int recoil = 2;  // poussee infligee a l'ennemi
    int epCost = 0;  // energie du coup charge elementaire (zone d'effet)
};

struct ForgeRecipe {
    std::string id, weaponId, branch, name, description, drawback; // id = <arme>_<branche>_<rang>
    int rank = 1;
    std::map<std::string, int> cost; // materiau -> quantite
    RemakeWeaponDef result;          // statistiques de l'arme apres amelioration
};

struct InteractionDef {
    std::string zone, id, type, solutions, reset, result;
    int x = 0, y = 0; // case (coordonnees de world.txt)
};

// Resultat d'une tentative de forge (aucun etat modifie sauf Ok).
enum class ForgeResult { Ok, Unknown, NoWeapon, AlreadyDone, OtherBranch, Missing };

struct MenuState {
    bool open = false;
    MenuPage page = MenuPage::Weapons;
    size_t cursor = 0;
    size_t gridCursor = 0; // position memorisee dans la grille d'objets (curseur visible aussi en page armes)
};

// --- Effets ------------------------------------------------------------------
// Effet anime d'Alundra 1 (assets/sprites/fx_*.a1s), joue une fois par-dessus le decor
struct Effect {
    std::string sprite;
    float x, y;
    int z;
    float scale;
    int delay; // ticks avant l'apparition
    AnimPlayer anim;
};

struct Particle {
    float x, y, z, vx, vy, vz;
    int life, maxLife;
    SDL_Color color;
    float size;
};
struct FloatText {
    float x, y;
    int life;
    std::string text;
    SDL_Color color;
};
struct Pickup { // coeur lache par les monstres
    float x, y;
    int z;
    int life;
};

// --- Dialogues ---------------------------------------------------------------
// Choix d'une question (ligne « ? Oui > id | Non > id » de dialogues.txt) : le libelle et le
// dialogue ouvert si on le choisit (vide = la conversation s'arrete).
struct DialogueChoice {
    std::string label, target;
};
struct DialoguePage {
    std::string speaker, text;
    std::vector<DialogueChoice> choices; // non vide : la page attend une reponse (presentation A1)
};
struct DialogueBox {
    std::string id;           // identifiant ouvert en dernier (utile aux tests automatiques)
    std::vector<DialoguePage> pages;
    size_t page = 0;
    float shown = 0; // caracteres affiches (effet machine a ecrire)
    size_t choice = 0;   // reponse sous le curseur (triangle de WIND.TX)
    int prevChoiceDx = 0; // front gauche/droite pour deplacer le curseur
    bool active() const { return page < pages.size(); }
};

enum class Mode { Splash, Title, Prologue, Play, GameOver, Ending };

// Ecran d'ouverture, ecran titre (menu) et prologue : voir title.cpp
struct Front {
    std::unique_ptr<Map> map; // decor de fond (une vraie zone du jeu, camera en mouvement lent)
    std::string zone;
    int tick = 0;             // temps ecoule dans la phase courante
    int card = 0;             // carte du prologue
    int menu = 0, page = 0;   // element choisi ; page 0 = menu principal, 1 = options
    bool resume = false;      // "Continuer" a ete choisi
    int prevDx = 0, prevDy = 0;
    int leaving = 0;          // fondu de sortie en cours (ticks restants)
    int fadeIn = 0;           // fondu d'entree dans le jeu (ticks restants)
    bool fullscreen = false;
};

struct SfxVoice {
    int id;
    float pos, step, vol, pan;
};

struct SfxDef {
    int id = -1;
    float pitch = 1.f, vol = 1.f;
};

struct Music {
    std::vector<std::vector<int16_t>> sfx; // bruitages (mono, 44 100 Hz)
    std::vector<SfxVoice> voices;          // bruitages en cours (lus sous verrou du flux)
    float sfxVolume = 0.7f;
    SDL_AudioStream* stream = nullptr;
    std::vector<int16_t> pcm, pending; // stereo entrelace, 44 100 Hz
    size_t pos = 0, loop = 0, pendingLoop = 0; // loop : debut de boucle (en images stereo)
    bool pendingReady = false;
    float fade = 0.f, volume = 0.8f;
    int track = -1, count = 0;
    int shownTicks = 0; // affichage du numero du morceau
    bool boss = false;  // musique de boss en cours
    bool oneShot = false, pendingOneShot = false; // jingle : joue une fois, sans boucle
    bool ended = false; // le jingle est termine (lu sous verrou du flux)
    bool jingle = false; // un jingle est en cours
};

struct Game {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Gamepad* pad = nullptr;
    std::filesystem::path assets, data;

    Mode mode = Mode::Title;
    int modeTimer = 0;
    uint64_t frame = 0;

    // Monde
    std::vector<std::filesystem::path> maps;
    std::map<std::string, std::string> mapFor; // zone du remake -> carte A1 utilisee (si differente)
    int current = 0;
    std::unique_ptr<Map> map;
    WarpTable warps;
    bool onWarp = false;
    std::string startZone = "011";
    int startX = -1, startY = -1;
    Dir startDir = DOWN;
    float respawnX = 0, respawnY = 0;  // point de reapparition dans la zone courante
    std::string objective;

    // Personnages
    SpriteBank heroSprites;
    std::map<std::string, std::unique_ptr<SpriteBank>> sprites; // monstres et PNJ
    Hero hero;
    std::vector<Enemy> enemies;
    std::vector<Enemy> pendingEnemies; // division des gelees : ajoutees apres la boucle de combat
    std::vector<Npc> npcs;
    struct Spawn { std::string zone; int x, y; EnemyKind kind; };
    std::vector<Spawn> spawns;
    struct NpcDef { std::string zone; int x, y; std::string sprite; Dir dir; std::string dialogue; };
    std::vector<NpcDef> npcDefs;
    std::map<std::string, bool> cleared; // zones dont tous les monstres sont vaincus

    // Coffres et objets
    struct ChestDef { std::string zone; int x, y; std::string item, need; };
    std::vector<ChestDef> chestDefs;
    std::vector<Chest> chests;          // coffres de la zone courante
    std::set<std::string> opened;       // coffres deja ouverts ("zone:x:y")
    std::map<std::string, int> items;   // inventaire : coeur, herbe, lettre, cle
    Inventory inventory;                // equippement A2 : arme, poche, anneau
    MenuState menu;                     // etat du menu armes/objets/equipement
    std::map<std::string, ItemDef> itemDefs; // data/items.txt : id -> definition A2
    // Ajouts du remake (tranche 3)
    std::map<std::string, RemakeWeaponDef> rmWeapons;      // data/rm_weapons.txt
    std::vector<ForgeRecipe> forgeRecipes;                 // data/rm_forge.txt (ordre du fichier)
    std::map<std::string, InteractionDef> interactionDefs; // data/rm_interactions.txt
    std::set<std::string> upgrades;                        // ameliorations forgees (ids de recettes)
    std::map<std::string, int> interactionStates;          // interaction -> 1 si resolue
    std::string lastEffect;                                // dernier effet applique (tests --expect)
    int lastDamage = 0;                                    // derniers degats infliges (tests --expect)
    std::string forgeConfirm;                              // recette en attente de confirmation (2e appui)
    std::string forgeMessage;                              // retour affiche dans la page de forge
    // Actions de dialogue (lignes « ! » de dialogues.txt) : donne/prend des objets, pose un
    // evenement, « sinon <id> » si une prise est impossible (rien n'est alors modifie).
    std::map<std::string, std::vector<std::string>> dialogueActions;
    std::string lastChoice;                                // derniere reponse choisie (tests --expect)
    bool forgeAfterDialogue = false;                       // ouvrir la forge a la fin du dialogue d'Orvan
    std::string gotItem;                // objet qui sort du coffre et flotte au-dessus du heros
    int gotTicks = 0;
    float gotX = 0, gotY = 0;           // point de depart de l'objet (le coffre), en coordonnees ecran-monde
    bool gotShown = false;              // le message a ete affiche : l'objet disparait a sa fermeture

    // Effets
    std::vector<Particle> particles;
    std::vector<Effect> effects;
    std::vector<FloatText> texts;
    std::vector<Pickup> pickups;
    int shake = 0;
    int hitStop = 0; // micro-pause a l'impact (nouveaute : sensation de coup)

    // Dialogues
    std::map<std::string, std::vector<DialoguePage>> dialogues;
    DialogueBox box;
    std::string afterIntro; // dialogue a lancer apres l'ecran titre
    Front front;

    // Histoire : noms de lieux affiches a l'entree d'une zone (comme dans Alundra 2), etapes franchies
    std::map<std::string, std::string> zoneTitles; // data/world.txt : name <zone> <texte>
    // evenement <zone> <id> <dialogue> : dialogue joue une seule fois a l'entree de la zone
    std::map<std::string, std::pair<std::string, std::string>> zoneEvents;
    std::string banner;
    int bannerTicks = 0;
    bool bossDown = false; // l'ogre du Mont Epervier est vaincu : les PNJ passent au dialogue "<id>_apres"
    std::set<std::string> seenEvents; // evenements d'histoire deja joues (sauvegardes)

    // Musique (Alundra 2)
    Music music;
    std::map<std::string, int> zoneMusic;              // zone -> morceau impose ("titre" : ecran titre)
    std::map<std::string, std::string> zoneKind;       // zone -> categorie (village, plaine, donjon, boss)
    std::map<std::string, std::vector<int>> musicCats; // data/music.txt : categorie -> morceaux
    std::map<std::string, SfxDef> sfxMap;              // data/sfx.txt : evenement -> bruitage
    int sfxTest = -1;                                  // F7 / F8 : test des bruitages

    // Debug / outils
    bool debug = false, freeCam = false;
    // --diag <etape> : scene de diagnostic A1 (police, fenetre, portrait, dialogues).
    // 0 = jeu normal ; 1..9 = etapes isolees. N'influe pas sur la sauvegarde ni sur les gates.
    int diagStage = 0;
    std::optional<Warp> pendingWarp;
    float camX = 0, camY = 0;
    Input in;
    bool prevAttack = false, prevDash = false, prevJump = false, prevMenu = false;

    // Mode test (sans entree reelle) : --auto "..." --shot fichier.bmp
    std::vector<std::pair<char, int>> autoScript;
    size_t autoStep = 0;
    int autoTicks = 0;
    std::string autoShot;
    std::string saveOverride;         // --savefile : sauvegarde reelle vers ce chemin, meme en mode test
    std::string autoExpect;           // --expect ending : verifie la vraie fin apres le combat
    bool godMode = false;             // --godmode : le heros encaisse sans mourir (test de boss)
    bool autoAttackSeen = false;      // --expect noattack=1 : aucune attaque déclenchée
    bool autoDone = false;
};

// audio.cpp
void initAudio(Game& g, bool muted);
void playMusic(Game& g, int track);
void playZoneMusic(Game& g);
void saveZoneMusic(Game& g);
void cycleZoneMusic(Game& g, int step);
void loadMusicCategories(Game& g);
void updateBossMusic(Game& g);
void playJingle(Game& g, const char* name);
void playSfx(Game& g, const char* name, float x = -1.f); // x : position (stereo), -1 = centre
void playSfxId(Game& g, int id, float pitch = 1.f, float vol = 1.f, float pan = 0.f);
void loadSfx(Game& g);

// world.cpp
std::string zoneName(const Game& g);
bool loadZone(Game& g, const std::string& zone, int cellX = -1, int cellY = -1, Dir dir = DOWN);
void loadWorldData(Game& g);
SpriteBank* spriteBank(Game& g, const std::string& name);

// entities.cpp
void spawnZoneEntities(Game& g);
void updateEnemies(Game& g);
void updateHeroCombat(Game& g);
void updateEffects(Game& g);
void hurtHero(Game& g, int damage, float fromX, float fromY);
void burst(Game& g, float x, float y, int z, SDL_Color c, int count, float speed);
void floatText(Game& g, float x, float y, const std::string& t, SDL_Color c);
void spawnEffect(Game& g, const char* sprite, float x, float y, int z, float scale = 1.f, int delay = 0);

// items.cpp
void loadItemDefs(Game& g);
bool useItem(Game& g, const std::string& id);
bool equipWeapon(Game& g, const std::string& id);
void cycleWeapon(Game& g);
void grantSupreme(Game& g);
void spawnChests(Game& g, bool announce = false);
bool chestBlocks(const Game& g, float x, float y);
Chest* chestInFront(Game& g);
void openChest(Game& g, Chest& c);
bool updateItems(Game& g);
bool gotItemPos(const Game& g, float& x, float& y);
void useHerb(Game& g);
bool hasSave();
void setSavePath(const std::string& path); // --savefile (tests)
void writeSave(std::ostream& out, const Game& g);
void saveGame(const Game& g);
bool readSave(std::istream& in, Game& g);
bool loadGame(Game& g);
// items.cpp : ajouts du remake (tranche 3)
void loadRemakeData(Game& g);
const RemakeWeaponDef* remakeWeapon(const Game& g, const std::string& id);
const RemakeWeaponDef* equippedRemakeWeapon(const Game& g); // statistiques effectives (amelioration incluse)
int materialCount(const Game& g, const std::string& id);
const ForgeRecipe* forgeRecipe(const Game& g, const std::string& id);
std::string forgeState(const Game& g, const ForgeRecipe& r); // available | missing | done | locked | noweapon
ForgeResult confirmForge(Game& g, const std::string& recipeId);  // transaction atomique
void openForge(Game& g);
const InteractionDef* interactionInFront(const Game& g);
bool interactionBlocks(const Game& g, float x, float y);
bool resolveInteraction(Game& g, const InteractionDef& d, const std::string& effect);
void resetInteractions(Game& g, const std::string& gateId);
int puzzleProgress(const Game& g, const std::string& gateId);
bool useInteraction(Game& g, const InteractionDef& d); // appui "parler" devant un objet interactif
void impactInteractions(Game& g, float x, float y, float radius, const std::string& effect);
// entities.cpp : effet d'impact generique des armes du remake (ennemis + interactions proches)
void applyImpact(Game& g, float x, float y, int z, const std::string& effect, int damage, float radius = 0.f);

// menu.cpp : pages armes / objets / equipement ( donnees A2, rendu A1 )
void openMenu(Game& g);
void closeMenu(Game& g);
void updateMenu(Game& g);
void drawMenu(Game& g);

// dialogue.cpp
void loadDialogues(Game& g, const std::string& path);
void openDialogue(Game& g, const std::string& id);
void updateDialogue(Game& g);
void drawDialogue(Game& g);
void drawDiagnostic(Game& g); // --diag : scene deterministe d'isolement du rendu A1
void drawText(SDL_Renderer* r, float x, float y, const std::string& utf8, float scale = 1.f);
void drawTextLatin1(SDL_Renderer* r, float x, float y, const std::string& latin1); // chaine deja passee par foldAccents
void drawA1Window(SDL_Renderer* r, float x, float y, float w, float h); // parchemin natif WIND.TX (1:1)
float textWidthLatin1(const std::string& latin1); // largeur reelle en police FONT3
void drawSpeakerPortrait(Game& g, float x, float y, const std::string& speaker); // buste 48x56 (sprite A2)
void drawA1PageArrow(SDL_Renderer* r, float x, float y); // triangle « suite » de WIND.TX (banque 8)
// items.cpp : actions de dialogue atomiques ; renvoie le dialogue a ouvrir a la place (« sinon »), ou vide.
std::string runDialogueActions(Game& g, const std::string& id);

// title.cpp : ouverture, menu principal, options, prologue
void frontTick(Game& g);
void frontDraw(Game& g);
void loadOptions(Game& g);
void startGame(Game& g); // main.cpp
std::string foldAccents(const std::string& utf8);
void fxShadow(SDL_Renderer* r, float x, float y, float w);
void fxAmbient(Game& g, float cx, float cy);






