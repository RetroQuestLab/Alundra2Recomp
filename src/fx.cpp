// Effets visuels : ombre portee des personnages.
// Le decor A1 est dessine tel quel : aucun filtre, aucune vignette, aucun bruit.
#include <cmath>

#include "game.h"

namespace {
SDL_Renderer* owner = nullptr;
SDL_Texture* shadowTex = nullptr;

SDL_Texture* makeTexture(SDL_Renderer* r, int w, int h, auto pixel) {
    SDL_Surface* s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
    if (!s) return nullptr;
    for (int y = 0; y < h; ++y) {
        auto* row = reinterpret_cast<Uint8*>(s->pixels) + y * s->pitch;
        for (int x = 0; x < w; ++x) pixel(x, y, row + x * 4);
    }
    SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
    SDL_DestroySurface(s);
    return t;
}

void ensure(SDL_Renderer* r) {
    if (owner == r) return;
    owner = r;
    // Ombre : simple ellipse en 3 paliers, comme dans Alundra 1.
    shadowTex = makeTexture(r, 32, 12, [](int x, int y, Uint8* p) {
        const float dx = (x + 0.5f - 16) / 16.f, dy = (y + 0.5f - 6) / 6.f;
        const float d = dx * dx + dy * dy;
        p[0] = 0; p[1] = 0; p[2] = 0;
        p[3] = d < 0.45f ? 120 : d < 0.75f ? 80 : d < 1.f ? 40 : 0;
    });
    SDL_SetTextureScaleMode(shadowTex, SDL_SCALEMODE_NEAREST);
}
} // namespace

void fxShadow(SDL_Renderer* r, float x, float y, float w) {
    ensure(r);
    const SDL_FRect dst{std::floor(x - w / 2), std::floor(y - w * 0.18f), std::floor(w), std::floor(w * 0.375f)};
    SDL_RenderTexture(r, shadowTex, nullptr, &dst);
}

// Aucun effet global : les tuiles et les sprites A1 sont affiches directement.
void fxAmbient(Game&, float, float) {}
