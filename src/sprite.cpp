#include "sprite.h"

#include <cmath>
#include <cstring>
#include <fstream>

SpriteBank::~SpriteBank() {
    for (auto& f : frames_)
        if (f.tex) SDL_DestroyTexture(f.tex);
}

bool SpriteBank::load(SDL_Renderer* renderer, const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    char magic[4];
    in.read(magic, 4);
    if (!in || std::memcmp(magic, "A1SP", 4) != 0) return false;

    auto rd16 = [&](auto& v) { in.read(reinterpret_cast<char*>(&v), 2); };
    uint16_t n;
    rd16(n);
    frames_.resize(n);
    std::vector<uint8_t> px;
    for (auto& f : frames_) {
        uint16_t w, h;
        int16_t ox, oy;
        rd16(w); rd16(h); rd16(ox); rd16(oy);
        px.resize(size_t(w) * h * 4);
        in.read(reinterpret_cast<char*>(px.data()), px.size());
        if (!in) return false;
        SDL_Surface* s = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, px.data(), w * 4);
        f.tex = SDL_CreateTextureFromSurface(renderer, s);
        SDL_DestroySurface(s);
        if (!f.tex) return false;
        SDL_SetTextureScaleMode(f.tex, SDL_SCALEMODE_NEAREST);
        f.w = w; f.h = h; f.ox = ox; f.oy = oy;
    }

    uint16_t groups;
    rd16(groups);
    anims_.resize(size_t(groups) * 4);
    for (auto& a : anims_) {
        uint16_t count;
        rd16(count);
        a.loops = !(count & 0x8000); // bit 15 = animation jouee une fois
        a.steps.resize(count & 0x7FFF);
        for (auto& s : a.steps) { rd16(s.frame); rd16(s.duration); }
    }
    return static_cast<bool>(in);
}

const SpriteBank::Anim* SpriteBank::anim(int group, int dir) const {
    const size_t i = size_t(group) * 4 + dir;
    return i < anims_.size() ? &anims_[i] : nullptr;
}

const SpriteBank::Frame* SpriteBank::frame(int index) const {
    return index >= 0 && index < int(frames_.size()) ? &frames_[size_t(index)] : nullptr;
}

void SpriteBank::drawFrame(SDL_Renderer* renderer, int frame, float x, float y, float scale, Uint8 r, Uint8 g,
                           Uint8 b, Uint8 a, bool additive) const {
    if (frame < 0 || frame >= int(frames_.size())) return;
    const Frame& f = frames_[frame];
    SDL_FRect dst{std::floor(x - f.ox * scale), std::floor(y - f.oy * scale), f.w * scale, f.h * scale};
    SDL_SetTextureColorMod(f.tex, r, g, b);
    SDL_SetTextureAlphaMod(f.tex, a);
    SDL_SetTextureBlendMode(f.tex, additive ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(renderer, f.tex, nullptr, &dst);
}

void SpriteBank::drawFrameRotated(SDL_Renderer* renderer, int frame, float cx, float cy, double angle, float scale) const {
    if (frame < 0 || frame >= int(frames_.size())) return;
    const Frame& f = frames_[frame];
    const SDL_FRect dst{std::floor(cx - f.w * scale / 2), std::floor(cy - f.h * scale / 2), f.w * scale, f.h * scale};
    SDL_SetTextureColorMod(f.tex, 255, 255, 255);
    SDL_SetTextureAlphaMod(f.tex, 255);
    SDL_SetTextureBlendMode(f.tex, SDL_BLENDMODE_BLEND);
    SDL_RenderTextureRotated(renderer, f.tex, nullptr, &dst, angle, nullptr, SDL_FLIP_NONE);
}
