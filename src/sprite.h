#pragma once
#include <SDL3/SDL.h>

#include <cstdint>
#include <string>
#include <vector>

// Banque de sprites exportee par tools/a1_sprites.py (.a1s)
class SpriteBank {
public:
    struct Frame {
        SDL_Texture* tex = nullptr;
        int w = 0, h = 0, ox = 0, oy = 0; // (ox, oy) = position des pieds dans l'image
    };
    struct Step {
        uint16_t frame, duration; // duree en ticks de 1/60 s
    };
    struct Anim {
        std::vector<Step> steps;
        bool loops = true; // faux = jouee une fois (attaque)
    };

    // Groupes d'animations du heros (voir HERO_GROUPS dans tools/a1_sprites.py)
    enum Group { IDLE = 0, WALK = 1, ATTACK = 2, RUN = 3, HURT = 4, KO = 5, JUMP = 6, ITEM = 7, CHARGE = 8 };

    ~SpriteBank();
    bool load(SDL_Renderer* renderer, const std::string& path);

    // direction : 0 bas, 1 haut, 2 gauche, 3 droite
    const Anim* anim(int group, int dir) const;
    // Acces en lecture pour les presentations qui recadrent une frame (portraits A1).
    const Frame* frame(int frame) const;
    // scale : taille ; (r, g, b) : teinte multiplicative ; a : opacite (traînees, clignotement)
    void drawFrame(SDL_Renderer* renderer, int frame, float x, float y, float scale = 1.f,
                   Uint8 r = 255, Uint8 g = 255, Uint8 b = 255, Uint8 a = 255, bool additive = false) const;

    // Dessine l'image centree en (cx, cy), tournee de `angle` degres (chute du prologue)
    void drawFrameRotated(SDL_Renderer* renderer, int frame, float cx, float cy, double angle, float scale = 1.f) const;

private:
    std::vector<Frame> frames_;
    std::vector<Anim> anims_; // groupe * 4 + direction
};

// Lecture d'une animation dans le temps
struct AnimPlayer {
    const SpriteBank::Anim* anim = nullptr;
    size_t step = 0;
    int ticks = 0;
    bool finished = false;

    void play(const SpriteBank::Anim* a, bool restart = false) {
        if (a == anim && !restart) return;
        anim = a;
        step = 0;
        ticks = 0;
        finished = false;
    }
    void update() {
        if (!anim || anim->steps.empty() || finished) return;
        if (++ticks >= anim->steps[step].duration) {
            ticks = 0;
            if (step + 1 < anim->steps.size()) ++step;
            else if (anim->loops) step = 0;
            else finished = true;
        }
    }
    int frame() const { return (anim && !anim->steps.empty()) ? anim->steps[step].frame : -1; }
};

