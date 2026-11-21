#include "map.h"

#include <cstring>
#include <fstream>

namespace {
template <typename T>
bool readPod(std::ifstream& f, T& v) {
    return static_cast<bool>(f.read(reinterpret_cast<char*>(&v), sizeof(T)));
}
} // namespace

Map::~Map() {
    if (atlas_) SDL_DestroyTexture(atlas_);
}

bool Map::load(SDL_Renderer* renderer, const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    char magic[4];
    f.read(magic, 4);
    if (!f || std::memcmp(magic, "A1MP", 4) != 0) return false;

    uint16_t w, h, nTiles;
    if (!readPod(f, w) || !readPod(f, h) || !readPod(f, nTiles)) return false;

    // Atlas : les tuiles colorisees rangees en grille de atlasCols_ colonnes
    const int rows = (nTiles + atlasCols_ - 1) / atlasCols_;
    SDL_Surface* surf = SDL_CreateSurface(atlasCols_ * TILE_W, std::max(rows, 1) * TILE_H,
                                          SDL_PIXELFORMAT_RGBA32);
    if (!surf) return false;
    SDL_FillSurfaceRect(surf, nullptr, 0);

    std::vector<uint8_t> buf(TILE_W * TILE_H * 4);
    for (int t = 0; t < nTiles; ++t) {
        f.read(reinterpret_cast<char*>(buf.data()), buf.size());
        const int ox = (t % atlasCols_) * TILE_W, oy = (t / atlasCols_) * TILE_H;
        auto* dst = static_cast<uint8_t*>(surf->pixels);
        for (int y = 0; y < TILE_H; ++y)
            std::memcpy(dst + (oy + y) * surf->pitch + ox * 4, buf.data() + y * TILE_W * 4, TILE_W * 4);
    }

    cells_.resize(size_t(w) * h);
    f.read(reinterpret_cast<char*>(cells_.data()), cells_.size() * sizeof(Cell));

    uint32_t nWalls;
    if (!readPod(f, nWalls)) {
        SDL_DestroySurface(surf);
        return false;
    }
    walls_.resize(nWalls);
    f.read(reinterpret_cast<char*>(walls_.data()), nWalls * sizeof(uint16_t));
    if (!f) {
        SDL_DestroySurface(surf);
        return false;
    }

    if (atlas_) SDL_DestroyTexture(atlas_);
    atlas_ = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_DestroySurface(surf);
    if (!atlas_) return false;
    SDL_SetTextureScaleMode(atlas_, SDL_SCALEMODE_NEAREST);

    w_ = w;
    h_ = h;
    return true;
}

void Map::drawTile(SDL_Renderer* renderer, uint16_t tile, float x, float y) const {
    if (tile == 0xFFFF) return;
    SDL_FRect src{float((tile % atlasCols_) * TILE_W), float((tile / atlasCols_) * TILE_H),
                  float(TILE_W), float(TILE_H)};
    SDL_FRect dst{x, y, float(TILE_W), float(TILE_H)};
    SDL_RenderTexture(renderer, atlas_, &src, &dst);
}

void Map::draw(SDL_Renderer* renderer, float camX, float camY, int viewW, int viewH,
               const std::function<void(int)>& afterRow) const {
    // Ordre : de l'arriere (haut) vers l'avant (bas), comme sur PS1
    for (int y = 0; y < h_; ++y) {
        for (int x = 0; x < w_; ++x) {
            const float sx = float(x * TILE_W) - camX;
            if (sx + TILE_W < 0 || sx >= viewW) continue;

            const Cell& c = cells_[size_t(y) * w_ + x];
            const float sy = float((MARGIN_ROWS + y - c.height) * TILE_H) - camY;

            // Murs empiles sous la face du dessus
            for (int i = 0; i < c.wallCount; ++i) {
                const float wy = sy + float((i + 1 - c.wallOffset) * TILE_H);
                if (wy + TILE_H < 0 || wy >= viewH) continue;
                drawTile(renderer, walls_[c.firstWall + i], sx, wy);
            }
            if (sy + TILE_H >= 0 && sy < viewH) drawTile(renderer, c.top, sx, sy);
        }
        if (afterRow) afterRow(y);
    }
}
