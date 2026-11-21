// Monstres, combat et effets.
//
// Nouveautes par rapport a Alundra 1 :
//  - combo en 3 coups (le 3e fait double degats, recul fort, tremblement d'ecran) ;
//  - esquive roulee invulnerable ;
//  - micro-pause a l'impact (hit-stop) et chiffres de degats ;
//  - gelees qui se divisent, tortues a carapace (vulnerables de dos), ogre qui charge
//    et reste etourdi (degats doubles) s'il percute un mur.
#include <algorithm>
#include <cmath>
#include <iterator>

#include "game.h"

namespace {
const EnemyDef DEFS[] = {
    //  id          sprite     pv  vitesse degats rayon  echelle
    {"gelee", "gelee", 3, 0.9f, 1, 9.f, 0.6f},
    {"minigelee", "gelee", 1, 1.2f, 1, 6.f, 0.35f},
    {"tortue", "tortue", 4, 0.45f, 2, 10.f, 1.f},
    {"gobelin", "gobelin", 4, 0.85f, 1, 8.f, 1.f},
    {"ogre", "ogre", 14, 0.5f, 2, 13.f, 1.25f},
    // Droide des pirates (Alundra 2) : sprite d'A1 recolorie (tools/recolor_a1s.py), tire a distance.
    {"droide", "droide", 6, 0.7f, 1, 9.f, 0.95f},
    // Boss et gardes de la fin : memes schemas A1, recolorations A1->A2 (voir tools/recolor_a1s.py).
    {"golem", "golem", 18, 0.38f, 3, 14.f, 1.4f},
    {"capitaine", "capitaine", 12, 1.f, 2, 11.f, 1.1f},
    {"mephisto", "mephisto", 36, 0.62f, 3, 15.f, 1.6f},
};

// Etats communs
enum { S_IDLE, S_MOVE, S_ATTACK, S_WINDUP, S_CHARGE, S_STUNNED };

float dist(float ax, float ay, float bx, float by) { return std::hypot(ax - bx, ay - by); }

Dir facing(float dx, float dy, Dir fallback) {
    if (std::abs(dx) < 0.01f && std::abs(dy) < 0.01f) return fallback;
    if (std::abs(dx) > std::abs(dy)) return dx < 0 ? LEFT : RIGHT;
    return dy < 0 ? UP : DOWN;
}

float frand() { return SDL_randf(); }
} // namespace

const EnemyDef& enemyDef(EnemyKind k) { return DEFS[int(k)]; }

std::optional<EnemyKind> enemyKindFromId(const std::string& id) {
    for (int i = 0; i < int(EnemyKind::Count); ++i)
        if (id == DEFS[i].id) return EnemyKind(i);
    return std::nullopt;
}

static Enemy makeEnemy(Game& g, EnemyKind k, float x, float y) {
    Enemy e{};
    e.kind = k;
    e.x = x;
    e.y = y;
    e.hp = enemyDef(k).hp;
    e.alive = true;
    e.lastSwing = -1;
    e.timer = int(frand() * 60);
    const Cell* c = g.map->cell(int(x / TILE_W), int(y / TILE_H));
    e.z = c ? c->height : 0;
    return e;
}

void spawnZoneEntities(Game& g) {
    g.enemies.clear();
    g.npcs.clear();
    const std::string zone = zoneName(g);
    if (!g.cleared[zone])
        for (const auto& s : g.spawns) {
            if (s.zone != zone) continue;
            // L'ogre est un evenement d'histoire : une fois vaincu, il ne revient jamais,
            // meme s'il reste d'autres monstres dans la zone (et meme apres une sauvegarde).
            if (s.kind == EnemyKind::Ogre && g.bossDown) continue;
            // Meme regle pour le combat final : apres la victoire, "Continuer" ouvre le
            // generique, il ne faut pas faire revivre Méphisto dans la salle du trone.
            if (s.kind == EnemyKind::Mephisto && g.seenEvents.count("mephisto_vaincu")) continue;
            // Les reliques et le passage du port dependent de ces evenements : un boss deja
            // vaincu ne doit jamais reapparaitre, meme s'il reste des monstres dans la zone.
            if (s.kind == EnemyKind::Golem && g.seenEvents.count("boss_" + zone)) continue;
            if (s.kind == EnemyKind::Capitaine && g.seenEvents.count("boss_medusa")) continue;
            g.enemies.push_back(makeEnemy(g, s.kind, s.x * TILE_W + TILE_W / 2.f, s.y * TILE_H + TILE_H - 1.f));
        }
    for (const auto& n : g.npcDefs)
        if (n.zone == zone) {
            Npc p;
            p.zone = n.zone;
            p.sprite = n.sprite;
            p.dialogue = n.dialogue;
            p.x = n.x * TILE_W + TILE_W / 2.f;
            p.y = n.y * TILE_H + TILE_H / 2.f;
            const Cell* c = g.map->cell(n.x, n.y);
            p.z = c ? c->height : 0;
            p.dir = n.dir;
            g.npcs.push_back(p);
        }
}

// --- Effets ------------------------------------------------------------------
void burst(Game& g, float x, float y, int z, SDL_Color c, int count, float speed) {
    for (int i = 0; i < count; ++i) {
        const float a = frand() * 6.2832f, s = speed * (0.4f + frand());
        Particle p{x, y, float(z * TILE_H) + 10.f, std::cos(a) * s, std::sin(a) * s * 0.6f, 1.f + frand() * 2.f,
                   0, 0, c, 1.f + frand() * 2.f};
        p.life = p.maxLife = 18 + int(frand() * 18);
        g.particles.push_back(p);
    }
}

void spawnEffect(Game& g, const char* sprite, float x, float y, int z, float scale, int delay) {
    SpriteBank* b = spriteBank(g, sprite);
    if (!b || !b->anim(0, DOWN)) return;
    Effect e{sprite, x, y, z, scale, delay, {}};
    e.anim.play(b->anim(0, DOWN), true);
    g.effects.push_back(e);
}

void floatText(Game& g, float x, float y, const std::string& t, SDL_Color c) {
    g.texts.push_back({x, y, 50, t, c});
}

void updateEffects(Game& g) {
    for (auto& e : g.effects) {
        if (e.delay > 0) --e.delay;
        else e.anim.update();
    }
    std::erase_if(g.effects, [](const Effect& e) { return e.anim.finished; });
    for (auto& p : g.particles) {
        p.x += p.vx;
        p.y += p.vy;
        p.z += p.vz;
        p.vz -= 0.12f;
        if (p.z < 0) { p.z = 0; p.vz *= -0.4f; p.vx *= 0.6f; p.vy *= 0.6f; }
        --p.life;
    }
    std::erase_if(g.particles, [](const Particle& p) { return p.life <= 0; });
    for (auto& t : g.texts) { t.y -= 0.4f; --t.life; }
    std::erase_if(g.texts, [](const FloatText& t) { return t.life <= 0; });

    // Coeurs a ramasser
    Hero& h = g.hero;
    for (auto& p : g.pickups) {
        --p.life;
        if (dist(p.x, p.y, h.x, h.y) < 14 && h.hp < h.maxHp) {
            h.hp = std::min(h.maxHp, h.hp + 2);
            p.life = 0;
            floatText(g, h.x, h.y - 40, "+1", {255, 120, 140, 255});
            playSfx(g, "coeur");
        }
    }
    std::erase_if(g.pickups, [](const Pickup& p) { return p.life <= 0; });
    if (g.shake > 0) --g.shake;
}

// --- Heros : degats ------------------------------------------------------------
void hurtHero(Game& g, int damage, float fromX, float fromY) {
    Hero& h = g.hero;
    if (g.godMode || h.invuln > 0 || h.dashTicks > 0 || g.mode != Mode::Play) return;
    h.hp -= damage;
    h.invuln = 70;
    playSfx(g, "degats");
    if (!h.attacking && g.heroSprites.anim(SpriteBank::HURT, h.dir)) {
        h.hurtTicks = 20;
        h.anim.play(g.heroSprites.anim(SpriteBank::HURT, h.dir), true);
    }
    const float dx = h.x - fromX, dy = h.y - fromY, l = std::max(0.01f, std::hypot(dx, dy));
    h.kx = dx / l * 3.f;
    h.ky = dy / l * 3.f;
    h.attacking = false;
    g.shake = 8;
    burst(g, h.x, h.y, h.z, {255, 60, 60, 255}, 10, 1.5f);
    if (h.hp <= 0) {
        h.hp = 0;
        g.mode = Mode::GameOver;
        g.modeTimer = 0;
        playJingle(g, "gameover");
        h.attacking = false;
        h.hurtTicks = 0;
        if (const auto* ko = g.heroSprites.anim(SpriteBank::KO, h.dir)) h.anim.play(ko, true);
    }
}

// --- Heros : epee ----------------------------------------------------------------
static void killEnemy(Game& g, Enemy& e) {
    e.alive = false;
    const EnemyDef& d = enemyDef(e.kind);
    burst(g, e.x, e.y, e.z, {200, 180, 255, 255}, 24, 2.f);
    burst(g, e.x, e.y, e.z, {255, 255, 255, 255}, 8, 1.f);
    g.shake = std::max(g.shake, e.kind == EnemyKind::Ogre ? 20 : 4);
    playSfx(g, e.kind == EnemyKind::Ogre ? "ogre_mort" : "mort", e.x);
    if (e.kind == EnemyKind::Gelee) { // la gelee se divise : on ne touche pas a g.enemies pendant le combat
        for (int i = 0; i < 2; ++i) {
            Enemy m = makeEnemy(g, EnemyKind::MiniGelee, e.x + (i ? 8.f : -8.f), e.y);
            m.kx = i ? 2.f : -2.f;
            m.flash = 10;
            g.pendingEnemies.push_back(m);
        }
    } else if (frand() < 0.35f || e.kind == EnemyKind::Ogre) {
        g.pickups.push_back({e.x, e.y, e.z, 600});
    }
    if (e.kind == EnemyKind::Ogre) {
        floatText(g, e.x, e.y - 50, "OGRE VAINCU !", {255, 220, 80, 255});
        g.bossDown = true;
        spawnChests(g, true); // le coffre que gardait l'ogre
        saveGame(g);
        openDialogue(g, "victoire");
        playJingle(g, "victoire");
    } else if (e.kind == EnemyKind::Golem) {
        const std::string ev = "boss_" + zoneName(g);
        g.seenEvents.insert(ev);
        floatText(g, e.x, e.y - 50, "GOLEM VAINCU !", {255, 220, 80, 255});
        spawnChests(g, true); // la relique dans le coffre qu'il gardait
        saveGame(g);
        openDialogue(g, ev);
        playJingle(g, "victoire");
    } else if (e.kind == EnemyKind::Capitaine) {
        g.seenEvents.insert("boss_medusa");
        floatText(g, e.x, e.y - 50, "CAPITAINE VAINCU !", {255, 220, 80, 255});
        spawnChests(g, true);
        saveGame(g);
        openDialogue(g, "boss_medusa");
        playJingle(g, "victoire");
    } else if (e.kind == EnemyKind::Mephisto) {
        g.seenEvents.insert("mephisto_vaincu");
        g.mode = Mode::Ending;
        g.modeTimer = 0;
        g.shake = 28;
        burst(g, e.x, e.y, e.z, {255, 240, 180, 255}, 60, 3.4f);
        playJingle(g, "victoire");
        saveGame(g);
    }
    (void)d;
}

// Effet generique d'un coup d'arme du remake (donnees data/rm_weapons.txt) :
//   fire  : eclaboussure de feu, 1 degat aux ennemis voisins de la cible ;
//   ice   : ralentit les ennemis touches (90 ticks, 150 en zone) ;
//   heavy : tremblement et poussee ; brise la garde des tortues (voir plus bas).
// radius > 0 : coup charge elementaire, degats et effet a tous les ennemis de la
// zone, et aux objets interactifs du remake (impactInteractions).
void applyImpact(Game& g, float x, float y, int z, const std::string& effect, int damage, float radius) {
    g.lastEffect = effect;
    if (damage > 0) g.lastDamage = damage;
    if (effect.empty() || effect == "none") return;
    const Hero& h = g.hero;
    const bool area = radius > 0.f;
    const float reach = area ? radius : effect == "fire" ? 26.f : 12.f;
    const SDL_Color c = effect == "fire"  ? SDL_Color{255, 140, 40, 255}
                        : effect == "ice" ? SDL_Color{150, 220, 255, 255}
                                          : SDL_Color{205, 185, 140, 255};
    burst(g, x, y, z, c, area ? 26 : 10, area ? 2.6f : 1.8f);
    if (effect == "heavy") g.shake = std::max(g.shake, area ? 14 : 6);
    if (area) playSfx(g, effect == "fire" ? "coup_fort" : effect == "ice" ? "eclair" : "bloque", x);
    for (auto& e : g.enemies) {
        if (!e.alive || std::abs(e.z - z) > 1) continue;
        const float d = std::hypot(e.x - x, e.y - y);
        if (d > reach + enemyDef(e.kind).radius) continue;
        if (effect == "ice") e.slow = std::max(e.slow, area ? 150 : 90);
        if (e.lastSwing == h.swingId) continue; // cible deja comptee par ce coup
        const int dmg = area ? damage : effect == "fire" ? 1 : 0;
        if (dmg <= 0) continue;
        e.lastSwing = h.swingId;
        e.hp -= dmg;
        e.flash = 12;
        if (effect == "heavy" && d > 0.1f) {
            e.kx = (e.x - x) / d * 4.5f;
            e.ky = (e.y - y) / d * 4.5f;
        }
        floatText(g, e.x, e.y - 34, std::to_string(dmg), c);
        if (e.hp <= 0) killEnemy(g, e);
    }
    if (area) impactInteractions(g, x, y, radius, effect);
}

void updateHeroCombat(Game& g) {
    Hero& h = g.hero;
    if (!h.attacking) return;
    ++h.attackTick;
    // Gantelet d'eclair (combo 5) : decharge instantanee en cone devant le heros.
    // L'electricite traverse la carapace des tortues et perturbe les droides.
    if (h.combo == 5) {
        if (h.attackTick != 6) return;
        const float fx = dirX(h.dir), fy = dirY(h.dir);
        for (auto& e : g.enemies) {
            if (!e.alive || e.lastSwing == h.swingId || std::abs(h.z - e.z) > 1) continue;
            const float dx = e.x - h.x, dy = e.y - h.y;
            const float ahead = dx * fx + dy * fy;
            const float side = std::abs(dx * fy - dy * fx);
            if (ahead < -6.f || ahead > 84.f || side > 24.f + enemyDef(e.kind).radius) continue;
            e.lastSwing = h.swingId;
            const int dmg = e.kind == EnemyKind::Ogre ? 2 : 3;
            e.hp -= dmg;
            e.flash = 14;
            e.kx = fx * 2.f;
            e.ky = fy * 2.f;
            if (e.kind == EnemyKind::Droid) { e.stun = 45; e.state = S_STUNNED; }
            playSfx(g, "eclair", e.x);
            burst(g, e.x, e.y, e.z, {130, 220, 255, 255}, 10, 2.4f);
            floatText(g, e.x, e.y - 34, std::to_string(dmg), SDL_Color{140, 230, 255, 255});
            if (e.hp <= 0) killEnemy(g, e);
        }
        return;
    }

    // Fenetre active de l'epee. Le coup charge (combo 4) est le meme geste, avec une zone bien plus large devant.
    // L'epee supreme (Jeehan) garde lesmemes animations mais allonge la portee et augmente les degats.
    const bool charged = h.combo == 4;
    const bool supreme = h.weapon == Weapon::Supreme;
    // Armes du remake : fenetre active, portee, degats et recul lus dans les donnees,
    // jamais un cas par arme. Les epees A2 et l'epee supreme gardent leurs regles.
    const RemakeWeaponDef* rmw = h.weapon == Weapon::Sword ? equippedRemakeWeapon(g) : nullptr;
    const int lastActive = rmw ? 2 + std::max(4, rmw->speed) : 16;
    if (h.attackTick < 3 || h.attackTick > lastActive) return;
    const float baseReach = rmw ? float(rmw->range) : 18.f;
    const float reach = charged ? baseReach + 12.f : supreme ? 23.f : baseReach;
    const float hx = h.x + dirX(h.dir) * reach, hy = h.y + dirY(h.dir) * reach * 0.78f - 6.f;
    const bool vertical = h.dir == UP || h.dir == DOWN;
    const float bonus = supreme ? 8.f : 0.f;
    const float rw = (charged ? (vertical ? 70.f : 56.f) : vertical ? 22.f : 18.f) + bonus;
    const float rh = (charged ? (vertical ? 44.f : 56.f) : vertical ? 16.f : 22.f) + bonus;
    // Coup charge elementaire : une fois par coup, en zone, s'il reste assez d'energie.
    if (rmw && charged && h.attackTick == 3 && rmw->effect != "none" && h.ep >= rmw->epCost) {
        h.ep -= rmw->epCost;
        applyImpact(g, hx, hy, h.z, rmw->effect, rmw->damage + 3, 40.f); // meme bonus que le coup charge
    }
    // Impacts des armes du remake appliques APRES la boucle : l'eclaboussure ne doit
    // pas marquer comme deja touche un ennemi que ce meme coup frappe ensuite (revue QA).
    struct Impact { float x, y; int z, dmg; };
    std::vector<Impact> impacts;
    for (auto& e : g.enemies) {
        if (!e.alive || e.lastSwing == h.swingId) continue;
        const float r = enemyDef(e.kind).radius;
        if (std::abs(e.x - hx) > rw / 2 + r || std::abs((e.y - 4) - hy) > rh / 2 + r) continue;
        e.lastSwing = h.swingId;

        // Tortue : carapace de face
        const bool breaksGuard = charged || (rmw && rmw->effect == "heavy"); // coup charge ou arme lourde
        if (e.kind == EnemyKind::Tortue && e.stun == 0 && !breaksGuard) {
            const float fx = dirX(e.dir), fy = dirY(e.dir);
            if ((h.x - e.x) * fx + (h.y - e.y) * fy > 0) {
                burst(g, (e.x + h.x) / 2, (e.y + h.y) / 2, e.z, {255, 255, 160, 255}, 8, 2.f);
                floatText(g, e.x, e.y - 30, "TCHAK", {200, 200, 200, 255});
                playSfx(g, "bloque", e.x);
                h.kx = -dirX(h.dir) * 2.5f;
                h.ky = -dirY(h.dir) * 2.5f;
                g.hitStop = 4;
                continue;
            }
        }

        int dmg = charged ? 4 : h.combo >= 3 ? 2 : 1;
        // Epees A2 : la puissance 1..6 modifie reellement les degats. La epee
        // supreme (gate historique) garde exactement son comportement precedent.
        if (rmw) dmg = rmw->damage + (charged ? 3 : h.combo >= 3 ? 1 : 0);
        else if (!supreme) dmg += std::clamp(h.swordPower - 1, 0, 5);
        if (supreme) dmg += 2;
        if (e.state == S_STUNNED) dmg *= 2; // ogre ou tortue etourdis
        e.hp -= dmg;
        e.flash = 12;
        playSfx(g, h.combo >= 3 ? "coup_fort" : "coup", e.x);
        const float push = rmw ? float(rmw->recoil) + (h.combo >= 3 ? 2.f : 0.f) : h.combo >= 3 ? 4.5f : 2.5f;
        const float kscale = e.kind == EnemyKind::Ogre && e.state != S_STUNNED ? 0.3f : 1.f;
        e.kx = dirX(h.dir) * push * kscale;
        e.ky = dirY(h.dir) * push * kscale;
        if (e.kind == EnemyKind::Tortue) { e.stun = 50; e.state = S_STUNNED; }
        g.hitStop = h.combo >= 3 ? 7 : 3;
        if (h.combo >= 3) g.shake = std::max(g.shake, 10);
        burst(g, e.x, e.y, e.z, {255, 230, 120, 255}, 6, 2.f);
        floatText(g, e.x, e.y - 34, std::to_string(dmg), dmg > 1 ? SDL_Color{255, 160, 60, 255} : SDL_Color{255, 255, 255, 255});
        g.lastDamage = dmg;
        g.lastEffect = rmw ? rmw->effect : "none";
        if (rmw) impacts.push_back({e.x, e.y, e.z, dmg});
        if (e.hp <= 0) killEnemy(g, e);
    }
    for (const Impact& i : impacts) applyImpact(g, i.x, i.y, i.z, rmw->effect, i.dmg);
}

// --- Monstres : IA -------------------------------------------------------------
static void moveEnemy(Game& g, Enemy& e, float dx, float dy) {
    const float r = std::min(6.f, enemyDef(e.kind).radius * 0.6f);
    if (canStand(*g.map, e.x + dx, e.y, e.z, r, 3)) e.x += dx;
    else if (e.state == S_CHARGE) { e.state = S_STUNNED; e.stun = 90; g.shake = 12; burst(g, e.x, e.y, e.z, {180, 160, 120, 255}, 14, 2.f); floatText(g, e.x, e.y - 50, "ETOURDI", {255, 255, 120, 255}); }
    if (canStand(*g.map, e.x, e.y + dy, e.z, r, 3)) e.y += dy;
    else if (e.state == S_CHARGE) { e.state = S_STUNNED; e.stun = 90; g.shake = 12; floatText(g, e.x, e.y - 50, "ETOURDI", {255, 255, 120, 255}); }
}

void updateEnemies(Game& g) {
    Hero& h = g.hero;
    // Ajouts differes (division des gelees) : jamais pendant la boucle de combat.
    if (!g.pendingEnemies.empty()) {
        g.enemies.insert(g.enemies.end(),
                         std::make_move_iterator(g.pendingEnemies.begin()),
                         std::make_move_iterator(g.pendingEnemies.end()));
        g.pendingEnemies.clear();
    }
    for (auto& e : g.enemies) {
        if (!e.alive) continue;
        const EnemyDef& d = enemyDef(e.kind);
        SpriteBank* bank = spriteBank(g, d.sprite);
        const float tx = h.x - e.x, ty = h.y - e.y, dh = std::hypot(tx, ty);
        const bool sameLevel = std::abs(h.z - e.z) <= 1;

        // Recul
        if (std::abs(e.kx) + std::abs(e.ky) > 0.05f) {
            moveEnemy(g, e, e.kx, e.ky);
            e.kx *= 0.8f;
            e.ky *= 0.8f;
        }
        if (e.flash > 0) --e.flash;
        ++e.timer;
        float mx = 0, my = 0;

        if (e.stun > 0) {
            if (--e.stun == 0) { e.state = S_IDLE; e.timer = 0; }
        } else switch (e.kind) {
        case EnemyKind::Gelee:
        case EnemyKind::MiniGelee: {
            // Bonds : 20 ticks de saut, puis une pause
            const int period = e.kind == EnemyKind::MiniGelee ? 40 : 60;
            const int t = e.timer % period;
            if (t == 0) {
                if (dh < 130 && sameLevel) { e.vx = tx / dh; e.vy = ty / dh; }
                else { const float a = frand() * 6.2832f; e.vx = std::cos(a); e.vy = std::sin(a); }
                e.dir = facing(e.vx, e.vy, e.dir);
            }
            if (t < 22) { mx = e.vx * d.speed * 1.6f; my = e.vy * d.speed * 1.6f; }
            break;
        }
        case EnemyKind::Tortue:
            if (dh < 110 && sameLevel) { mx = tx / dh * d.speed; my = ty / dh * d.speed; }
            else if (e.timer % 120 < 50) { mx = dirX(e.dir) * d.speed; my = dirY(e.dir) * d.speed; }
            else if (e.timer % 120 == 50) e.dir = Dir(int(frand() * 4) & 3);
            if (mx || my) e.dir = facing(mx, my, e.dir);
            break;
        case EnemyKind::Gobelin:
        case EnemyKind::Capitaine:
            if (e.state == S_ATTACK) {
                // Coup de faux : touche si le heros est proche au milieu de l'animation
                if (e.timer == 14 && dh < 30 && h.air < 14) hurtHero(g, d.damage, e.x, e.y);
                if (e.timer > 30) { e.state = S_IDLE; e.timer = -40; }
            } else if (dh < 24 && sameLevel && e.timer > 0) {
                e.state = S_ATTACK;
                e.timer = 0;
                e.dir = facing(tx, ty, e.dir);
                if (bank) e.anim.play(bank->anim(2, e.dir), true);
            } else if (dh < 150 && sameLevel) {
                // Approche en zigzag (plus vivant qu'une ligne droite)
                const float side = std::sin(e.timer * 0.08f) * 0.5f;
                mx = (tx / dh - ty / dh * side) * d.speed;
                my = (ty / dh + tx / dh * side) * d.speed;
                e.dir = facing(tx, ty, e.dir);
            }
            break;
        case EnemyKind::Ogre:
        case EnemyKind::Golem:
            if (e.state == S_WINDUP) { // il se prepare (tremble, rougit)
                if (e.timer > 40) { e.state = S_CHARGE; e.timer = 0; e.vx = tx / dh; e.vy = ty / dh; e.dir = facing(tx, ty, e.dir); }
            } else if (e.state == S_CHARGE) {
                mx = e.vx * 3.2f;
                my = e.vy * 3.2f;
                if (e.timer > 50) { e.state = S_IDLE; e.timer = 0; }
            } else if (e.state == S_STUNNED) {
                e.state = S_IDLE;
            } else {
                if (dh < 170 && sameLevel) {
                    mx = tx / dh * d.speed;
                    my = ty / dh * d.speed;
                    e.dir = facing(tx, ty, e.dir);
                    if (e.timer > 150 && dh > 40) { e.state = S_WINDUP; e.timer = 0; }
                }
            }
            break;
        case EnemyKind::Droid:
        case EnemyKind::Mephisto: {
            // Garde mecanique de Diaz : approche, puis tire une decharge d'energie.
            // Diaz tire trois decharges en eventail, plus vite.
            const bool boss = e.kind == EnemyKind::Mephisto;
            const int period = boss ? 62 : 100;
            if (dh < (boss ? 170.f : 120.f) && sameLevel) {
                mx = tx / dh * d.speed;
                my = ty / dh * d.speed;
                e.dir = facing(tx, ty, e.dir);
                if (e.timer % period == 0) {
                    const float fx = dirX(e.dir), fy = dirY(e.dir);
                    for (int k = boss ? -1 : 0; k <= (boss ? 1 : 0); ++k) {
                        const float side = k * 16.f;
                        spawnEffect(g, "fx_orbe", e.x + fx * 18 - fy * side, e.y + fy * 14 + fx * side - 6, e.z, boss ? 0.8f : 0.6f);
                        const float px = e.x + fx * 60 - fy * side, py = e.y + fy * 45 + fx * side;
                        if (std::abs(h.x - px) < (boss ? 30.f : 26.f) && std::abs(h.y - py) < (boss ? 26.f : 22.f))
                            hurtHero(g, d.damage, e.x, e.y);
                    }
                    playSfx(g, boss ? "eclair" : "epee3", e.x);
                    if (boss) g.shake = std::max(g.shake, 5);
                }
            }
            break;
        }
        default:
            break;
        }
        if (e.slow > 0) { --e.slow; mx *= 0.4f; my *= 0.4f; } // effet "ice" des armes du remake
        if (mx || my) moveEnemy(g, e, mx, my);

        // Contact avec le heros
        // on peut sauter par-dessus les petits monstres
        if (dh < d.radius + 6 && sameLevel && h.air < 14 && e.state != S_STUNNED) hurtHero(g, d.damage, e.x, e.y);

        // Animation
        if (bank && e.state != S_ATTACK) e.anim.play(bank->anim((mx || my) ? 1 : 0, e.dir));
        e.anim.update();
    }
    std::erase_if(g.enemies, [](const Enemy& e) { return !e.alive; });

    // Zone nettoyee
    const std::string zone = zoneName(g);
    bool hadSpawns = false;
    for (const auto& s : g.spawns) hadSpawns |= s.zone == zone;
    if (hadSpawns && g.enemies.empty() && !g.cleared[zone]) {
        g.cleared[zone] = true;
        floatText(g, h.x, h.y - 60, "ZONE LIBEREE !", {120, 255, 160, 255});
        if (!g.music.jingle) playJingle(g, "liberee");
    }
}


