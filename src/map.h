#pragma once
#include <SDL3/SDL.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

constexpr int TILE_W = 24;
constexpr int TILE_H = 16;

struct Cell {
    uint8_t height;
    int8_t wallOffset;
    uint8_t wallCount;
    uint8_t attr;
    uint16_t top;       // 0xFFFF = rien
    uint16_t firstWall; // index dans Map::walls
};

class Map {
public:
    ~Map();
    bool load(SDL_Renderer* renderer, const std::string& path);
    // afterRow(y) est appele apres le dessin de chaque rangee : permet d'intercaler les sprites
    void draw(SDL_Renderer* renderer, float camX, float camY, int viewW, int viewH,
              const std::function<void(int)>& afterRow = {}) const;

    // Case en coordonnees de grille (nullptr hors carte)
    const Cell* cell(int x, int y) const {
        if (x < 0 || y < 0 || x >= w_ || y >= h_) return nullptr;
        return &cells_[size_t(y) * w_ + x];
    }

    int width() const { return w_; }
    int height() const { return h_; }
    // Taille en pixels, marge de hauteur comprise
    int pixelWidth() const { return w_ * TILE_W; }
    int pixelHeight() const { return (h_ + MARGIN_ROWS) * TILE_H; }

    static constexpr int MARGIN_ROWS = 16; // les cases hautes debordent vers le haut

private:
    void drawTile(SDL_Renderer* renderer, uint16_t tile, float x, float y) const;

    int w_ = 0, h_ = 0;
    int atlasCols_ = 32;
    SDL_Texture* atlas_ = nullptr;
    std::vector<Cell> cells_;
    std::vector<uint16_t> walls_;
};
