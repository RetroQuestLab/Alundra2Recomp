// Dialogues : donnees A2, presentation Alundra 1 (police FONT3.TIM et fenetre WIND.TX).
//
// Le texte est UTF-8 dans data/dialogues.txt ; foldAccents le convertit en
// octets Latin-1, qui sont exactement les index de l'atlas FONT3.TIM. Aucun
// glyphe n'est approche : tout vient de src/a1_font.h, genere par tools/a1_font.py.
//
// La seule variante de fenetre attestee (captures lp11_20-Meia / lp11_21...)
// comporte un portrait a gauche chevauchant le haut du panneau, un cartouche de
// nom parchemin au-dessus, puis un panneau parchemin a trois lignes de texte au plus.
// L'indicateur de page n'etant pas confirme, il n'est pas dessine.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "a1_font.h"
#include "game.h"

// UTF-8 -> octets Latin-1 (1 octet = 1 index de glyphe).
// Les caracteres hors Latin-1 sont rendus explicitement en '?' : aucune
// disparition silencieuse.
std::string foldAccents(const std::string& s) {
    static const std::pair<const char*, const char*> map[] = {
        {"\xC5\x93", "oe"}, {"\xC5\x92", "OE"}, {"\xE2\x80\x99", "'"}, {"\xE2\x80\xA6", "..."},
        {"\xC2\xAB", "\""}, {"\xC2\xBB", "\""}, {"\xC2\xA0", " "}, {"\xE2\x80\x94", "-"}, {"\xE2\x80\x93", "-"},
    };
    std::string out;
    for (size_t i = 0; i < s.size();) {
        bool done = false;
        for (const auto& [from, to] : map) {
            const size_t n = std::strlen(from);
            if (s.compare(i, n, from) == 0) { out += to; i += n; done = true; break; }
        }
        if (done) continue;
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) { out += char(c); ++i; }
        else if (c == 0xC3 && i + 1 < s.size()) { // U+00C0..U+00FF -> Latin-1
            out += char(0xC0 + (static_cast<unsigned char>(s[i + 1]) & 0x3F));
            i += 2;
        } else { // repli explicite, jamais de saut muet
            out += '?';
            ++i;
            while (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) ++i;
        }
    }
    return out;
}

namespace {

// Texture de police mise en cache par renderer : table locale (pas de globale
// mutable). Les glyphes sont blancs sur transparent ; la couleur de dessin SDL
// courante est appliquee via SDL_SetTextureColorMod.
SDL_Texture* fontTexture(SDL_Renderer* r) {
    static SDL_Renderer* cachedRenderer = nullptr;
    static SDL_Texture* cached = nullptr;
    if (cached && cachedRenderer == r) return cached;
    if (cached) { SDL_DestroyTexture(cached); cached = nullptr; }

    std::vector<SDL_Color> px(size_t(256) * 256, SDL_Color{255, 255, 255, 0});
    for (int tile = 32; tile < 256; ++tile) {
        const int col = tile % 16, row = tile / 16;
        for (int y = 0; y < a1font::kCell; ++y) {
            const uint8_t bits0 = a1font::kGlyphBits[size_t(tile) * 32 + size_t(y) * 2];
            const uint8_t bits1 = a1font::kGlyphBits[size_t(tile) * 32 + size_t(y) * 2 + 1];
            for (int x = 0; x < a1font::kCell; ++x) {
                const uint8_t bit = (x < 8 ? bits0 : bits1) & (1u << (7 - (x % 8)));
                if (bit) px[size_t(row * 16 + y) * 256 + size_t(col * 16 + x)] = SDL_Color{255, 255, 255, 255};
            }
        }
    }
    SDL_Surface* s = SDL_CreateSurfaceFrom(256, 256, SDL_PIXELFORMAT_RGBA32, px.data(), 256 * 4);
    cached = SDL_CreateTextureFromSurface(r, s);
    SDL_DestroySurface(s);
    if (cached) SDL_SetTextureScaleMode(cached, SDL_SCALEMODE_NEAREST);
    cachedRenderer = r;
    return cached;
}

// Fenetre native WIND.TX (4bpp 256x256) avec une banque CLUT de WIND.CL
// (0 = parchemin, 8 = indicateur de page gris), une texture par banque,
// mise en cache de la meme facon que l'atlas de police.
SDL_Texture* windowTexture(SDL_Renderer* r, int bank) {
    static SDL_Renderer* cachedRenderer = nullptr;
    static std::array<SDL_Texture*, 16> cached{};
    bank = std::clamp(bank, 0, 15);
    if (cachedRenderer != r) {
        for (auto& t : cached)
            if (t) { SDL_DestroyTexture(t); t = nullptr; }
        cachedRenderer = r;
    }
    if (cached[size_t(bank)]) return cached[size_t(bank)];

    const auto& pal = a1font::kWindPaletteBanks[size_t(bank)];
    std::vector<SDL_Color> px(size_t(a1font::kWindW) * a1font::kWindH);
    for (size_t i = 0; i < px.size(); ++i) {
        const auto& c = pal[a1font::kWindPixels[i]];
        px[i] = SDL_Color{c[0], c[1], c[2], c[3]};
    }

    SDL_Surface* s = SDL_CreateSurfaceFrom(a1font::kWindW, a1font::kWindH, SDL_PIXELFORMAT_RGBA32,
                                           px.data(), a1font::kWindW * 4);
    SDL_Texture* t = s ? SDL_CreateTextureFromSurface(r, s) : nullptr;
    SDL_DestroySurface(s);
    if (!t) {
        SDL_Log("Texture WIND.TX (banque %d) : %s", bank, SDL_GetError());
        return nullptr;
    }
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    cached[size_t(bank)] = t;
    return t;
}

SDL_Texture* windowTexture(SDL_Renderer* r) { return windowTexture(r, a1font::kWindClutBank); }

// Texte deja converti en Latin-1 : un octet = un glyphe de l'atlas natif.
void drawLatin1(SDL_Renderer* r, float x, float y, const std::string& s) {
    SDL_Texture* tex = fontTexture(r);
    if (!tex) return;
    Uint8 red = 255, green = 255, blue = 255, alpha = 255;
    SDL_GetRenderDrawColor(r, &red, &green, &blue, &alpha);
    SDL_SetTextureColorMod(tex, red, green, blue);
    // L'alpha de la couleur de dessin sert aux fondus des rectangles ; il ne
    // doit PAS rendre le texte invisible (SDL_RenderDebugText l'ignorait).
    SDL_SetTextureAlphaMod(tex, 255);

    float rx = x;
    for (unsigned char c : s) {
        const int idx = c >= 32 ? int(c) : int('?');
        const SDL_FRect src{float((idx % 16) * 16), float((idx / 16) * 16), 16.f, 16.f};
        const SDL_FRect dst{rx, y, 16.f, 16.f};
        SDL_RenderTexture(r, tex, &src, &dst);
        rx += float(a1font::kAdvance[size_t(idx)]);
    }
}

float textWidth(const std::string& s) {
    float w = 0.f;
    for (unsigned char c : s) w += float(a1font::kAdvance[c >= 32 ? size_t(c) : size_t('?')]);
    return w;
}

// D coupe selon l'avance reelle des glyphes, pas un nombre fixe de caracteres.
std::vector<std::string> wrapLatin1(const std::string& text, float maxWidth) {
    std::vector<std::string> lines;
    std::string current, word;
    auto flush = [&] {
        if (!current.empty() && textWidth(current + " " + word) > maxWidth) {
            lines.push_back(current);
            current.clear();
        }
        // Un mot plus large que le panneau est coupe a la lettre : on ne
        // reduit jamais la police pour le faire tenir.
        while (!word.empty() && textWidth(word) > maxWidth) {
            std::string part;
            for (char ch : word) {
                if (!part.empty() && textWidth(part + ch) > maxWidth) break;
                part += ch;
            }
            if (part.empty()) break;
            lines.push_back(part);
            word.erase(0, part.size());
        }
        current += (current.empty() ? "" : " ") + word;
        word.clear();
    };
    for (char c : text) {
        if (c == '\n') { flush(); lines.push_back(current); current.clear(); }
        else if (c == ' ') flush();
        else word += c;
    }
    flush();
    if (!current.empty()) lines.push_back(current);
    return lines;
}

// WIND.TX est un plan 4bpp de 256x256 (32768 octets) et WIND.CL contient
// 16 palettes de 16 couleurs. La banque 0 est le parchemin des dialogues.
//
// Composition A1 a l'echelle 1:1 (structure relevee dans WIND.TX, voir
// tools/a1_font.py) : texture parchemin REPETEE entre deux bords dechires
// repetes en largeur, puis deux rouleaux dont seul le corps du cylindre est
// repete en hauteur. L'interieur de la banderole source est couvert de tuiles
// reperes « X » : elles ne sont jamais dessinees (cause des pointilles parasites).
// Aucun rectangle de fond : le decor reste visible entre les dents des bords,
// comme sur les captures A1, et la texture n'a aucun pixel transparent.
void drawWindow(SDL_Renderer* r, float x, float y, float w, float h) {
    using namespace a1font;
    SDL_Texture* tex = windowTexture(r, kWindClutBank);
    if (!tex) return;
    const float X = std::floor(x), Y = std::floor(y);
    const float W = std::max(float(2 * kWindRollW), std::floor(w));
    const float H = std::max(float(kWindTopH + kWindBottomH), std::floor(h));
    const float rollW = float(kWindRollW);
    const float cx = X + rollW, cw = W - 2.f * rollW;

    if (cw > 0.f) {
        const SDL_FRect fillSrc{float(kWindFillX), float(kWindFillY), float(kWindFillW), float(kWindFillH)};
        const SDL_FRect fillDst{cx, Y + float(kWindTopH), cw, H - float(kWindTopH + kWindBottomH)};
        if (fillDst.h > 0.f) SDL_RenderTextureTiled(r, tex, &fillSrc, 1.f, &fillDst);
        const SDL_FRect topSrc{float(kWindEdgeX), float(kWindTopY), float(kWindEdgeW), float(kWindTopH)};
        const SDL_FRect topDst{cx, Y, cw, float(kWindTopH)};
        SDL_RenderTextureTiled(r, tex, &topSrc, 1.f, &topDst);
        const SDL_FRect botSrc{float(kWindEdgeX), float(kWindBottomY), float(kWindEdgeW), float(kWindBottomH)};
        const SDL_FRect botDst{cx, Y + H - float(kWindBottomH), cw, float(kWindBottomH)};
        SDL_RenderTextureTiled(r, tex, &botSrc, 1.f, &botDst);
    }

    // Rouleaux : chapeau ARRONDI (WIND.TX 224,0 / 224,24) puis les H-16 dernieres
    // lignes de la colonne du rouleau, comme sur l'ecran d'inventaire A1 mesure au
    // pixel (docs/visuel/a1_menu_layout.md). Au-dela de 40 lignes, le corps
    // uniforme du cylindre (lignes 16..23) est repete au-dessus de ces 40 lignes.
    const float capH = std::min(float(kWindCapH), H);
    const float body = H - capH;
    struct Roll { int capX, capY, colX; float dstX; };
    for (const Roll& roll : {Roll{kWindCapLeftX, kWindCapLeftY, kWindRollLeftX, X},
                             Roll{kWindCapRightX, kWindCapRightY, kWindRollRightX, X + W - rollW}}) {
        const SDL_FRect capSrc{float(roll.capX), float(roll.capY), float(kWindCapW), capH};
        const SDL_FRect capDst{roll.dstX, Y, rollW, capH};
        SDL_RenderTexture(r, tex, &capSrc, &capDst);
        if (body <= 0.f) continue;
        const float tail = std::min(body, float(kWindRollH));
        const SDL_FRect tailSrc{float(roll.colX), float(kWindRollH) - tail, rollW, tail};
        const SDL_FRect tailDst{roll.dstX, Y + H - tail, rollW, tail};
        SDL_RenderTexture(r, tex, &tailSrc, &tailDst);
        if (body > tail) {
            const SDL_FRect midSrc{float(roll.colX), float(kWindRollMidY), rollW, float(kWindRollMidH)};
            const SDL_FRect midDst{roll.dstX, Y + capH, rollW, body - tail};
            SDL_RenderTextureTiled(r, tex, &midSrc, 1.f, &midDst);
        }
    }
}

// Indicateur « page suivante » : triangle de WIND.TX en banque CLUT 8, attesté
// sur les captures A1 (coin bas droit, sur le rouleau). Image fixe : une
// eventuelle animation (4 tuiles voisines) n'est pas confirmee.
void drawPageArrow(SDL_Renderer* r, float x, float y) {
    using namespace a1font;
    SDL_Texture* tex = windowTexture(r, kWindArrowBank);
    if (!tex) return;
    const float s = float(kWindArrowSize);
    const SDL_FRect src{float(kWindArrowX), float(kWindArrowY), s, s};
    const SDL_FRect dst{std::floor(x), std::floor(y), s, s};
    SDL_RenderTexture(r, tex, &src, &dst);
}

// Encre des dialogues : index CLUT 6 de FONT3.TIM (41,49,16), l'index de TOUS les
// pixels de lettres de l'atlas, et exactement la couleur relevee sur la capture A1
// lp11_21. (L'index 7 utilise auparavant donnait un texte trop sombre.)
void setInk(SDL_Renderer* r) {
    const auto& ink = a1font::kFontPalette[6];
    SDL_SetRenderDrawColor(r, ink[0], ink[1], ink[2], 255);
}

void cartouche(SDL_Renderer* r, float x, float y, float w, const std::string& name) {
    drawWindow(r, x, y, w, 28.f);
    const float tw = textWidth(name);
    setInk(r);
    drawTextLatin1(r, std::floor(x + (w - tw) / 2.f), y + 6.f, name);
}

// Portraits A2 transposés dans le style A1 : on utilise les banques A2 extraites,
// jamais un portrait A1 sans rapport avec le locuteur.
const char* portraitBank(const std::string& speaker) {
    struct Entry { const char* needle; const char* bank; };
    static const Entry entries[] = {
        {"Flint", "flint_essai_formes"}, {"Alexia", "alexia"}, {"Mephisto", "mephisto"},
        {"M\xC3\xA9phisto", "mephisto"}, {"Milena", "milena"}, {"Natasha", "natasha"},
        {"Ruby", "ruby"}, {"Zach", "zach"}, {"Belgar", "belgar"}, {"Jeehan", "jeehan"},
        {"Diaz", "diaz"}, {"Albert", "albert"}, {"Roi", "roi"},
    };
    for (const auto& e : entries)
        if (speaker.find(e.needle) != std::string::npos) return e.bank;
    return nullptr;
}

// Geometrie du texte mesuree a l'echelle native sur la capture A1 lp11_21
// (texte a x=32, fin de ligne avant le cylindre du rouleau droit) ; trois lignes
// par panneau au plus, au pas de 16 px : releve sur les 196 captures A1 locales
// (encre aux lignes y~174..187, 190..203, 206..216 ; jamais une quatrieme).
constexpr float kTextX = 32.f;
constexpr float kTextMaxW = 254.f;
constexpr size_t kLinesPerPanel = 3;

// Coupe un texte UTF-8 en lignes selon l'avance reelle des glyphes natifs
// (meme regle que wrapLatin1, mais sans perdre l'UTF-8 : le repli Latin-1 se
// fait au dessin). Un mot plus large que le panneau est coupe au caractere.
std::vector<std::string> wrapUtf8(const std::string& text, float maxWidth) {
    auto width = [](const std::string& s) { return textWidth(foldAccents(s)); };
    std::vector<std::string> lines;
    std::string current, word;
    auto push = [&] {
        while (!word.empty() && width(word) > maxWidth) {
            size_t cut = 0, next = 0;
            while (next < word.size()) {
                size_t n = next + 1;
                while (n < word.size() && (static_cast<unsigned char>(word[n]) & 0xC0) == 0x80) ++n;
                if (cut > 0 && width(word.substr(0, n)) > maxWidth) break;
                cut = next = n;
            }
            if (!current.empty()) { lines.push_back(current); current.clear(); }
            lines.push_back(word.substr(0, cut));
            word.erase(0, cut);
        }
        if (word.empty()) return;
        const std::string candidate = current.empty() ? word : current + " " + word;
        if (!current.empty() && width(candidate) > maxWidth) {
            lines.push_back(current);
            current = word;
        } else {
            current = candidate;
        }
        word.clear();
    };
    for (char c : text) {
        if (c == ' ') push();
        else if (c == '\n') { push(); lines.push_back(current); current.clear(); }
        else word += c;
    }
    push();
    if (!current.empty()) lines.push_back(current);
    return lines;
}

// Pagination A1 : chaque page des donnees est decoupee en panneaux de deux
// lignes, sans reduire la police ni tronquer le texte.
std::vector<DialoguePage> paginate(const std::vector<DialoguePage>& pages) {
    std::vector<DialoguePage> out;
    for (const auto& p : pages) {
        const auto lines = wrapUtf8(p.text, kTextMaxW);
        if (lines.empty()) { out.push_back(p); continue; }
        for (size_t i = 0; i < lines.size(); i += kLinesPerPanel) {
            DialoguePage part{p.speaker, lines[i], {}};
            for (size_t k = 1; k < kLinesPerPanel && i + k < lines.size(); ++k) part.text += "\n" + lines[i + k];
            if (i + kLinesPerPanel >= lines.size()) part.choices = p.choices; // la question attend sa reponse
            out.push_back(std::move(part));
        }
    }
    return out;
}

void drawPortrait(Game& g, float x, float y, const std::string& speaker) {
    // Aucun bloc de remplacement : si la banque A2 du locuteur n'existe pas,
    // la variante sans portrait est utilisee et la ressource manquante documentee.
    if (const char* name = portraitBank(speaker)) {
        if (SpriteBank* b = spriteBank(g, name)) {
            const SpriteBank::Anim* a = b->anim(0, DOWN);
            const int index = a && !a->steps.empty() ? int(a->steps[0].frame) : 0;
            const SpriteBank::Frame* f = b->frame(index);
            if (!f || !f->tex || f->w <= 0 || f->h <= 0) return;
            // Cadrage A1 observe : buste 48x56, pas un sprite pied entier.
            // On recadre le haut du sprite A2 dans ces proportions, sans lisser.
            constexpr float kPortraitW = 48.f, kPortraitH = 56.f;
            // Hauteur source observee sur Meia : le buste tient dans les 20
            // premieres lignes d'un sprite A2 large de 24 px ; au-dela, les
            // jambes réapparaissent sous le cadre.
            const float srcH = std::min(float(f->h), float(f->w) * 20.f / 24.f);
            const SDL_FRect src{0.f, 0.f, float(f->w), srcH};
            const SDL_FRect dst{x, y, kPortraitW, kPortraitH};
            SDL_SetTextureColorMod(f->tex, 255, 255, 255);
            SDL_SetTextureAlphaMod(f->tex, 255);
            SDL_SetTextureBlendMode(f->tex, SDL_BLENDMODE_BLEND);
            SDL_RenderTexture(g.renderer, f->tex, &src, &dst);
        }
    }
}

} // namespace

void drawText(SDL_Renderer* r, float x, float y, const std::string& utf8, float scale) {
    float sx, sy;
    SDL_GetRenderScale(r, &sx, &sy);
    SDL_SetRenderScale(r, sx * scale, sy * scale);
    drawLatin1(r, x / scale, y / scale, foldAccents(utf8));
    SDL_SetRenderScale(r, sx, sy);
}

void drawTextLatin1(SDL_Renderer* r, float x, float y, const std::string& latin1) { drawLatin1(r, x, y, latin1); }
void drawA1Window(SDL_Renderer* r, float x, float y, float w, float h) { drawWindow(r, x, y, w, h); }
float textWidthLatin1(const std::string& latin1) { return textWidth(latin1); }
void drawSpeakerPortrait(Game& g, float x, float y, const std::string& speaker) { drawPortrait(g, x, y, speaker); }
void drawA1PageArrow(SDL_Renderer* r, float x, float y) { drawPageArrow(r, x, y); }

void loadDialogues(Game& g, const std::string& path) {
    std::ifstream in(path);
    std::string line, id;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() >= 3 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3); // BOM
        if (line.empty() || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') { id = line.substr(1, line.size() - 2); continue; }
        if (id.empty()) continue;
        if (line.rfind("! ", 0) == 0) { g.dialogueActions[id].push_back(line.substr(2)); continue; }
        if (line.rfind("? ", 0) == 0) { // « ? Oui > cible | Non > cible » : reponses de la page precedente
            auto& pages = g.dialogues[id];
            if (pages.empty()) continue;
            size_t start = 2;
            while (start <= line.size()) {
                const size_t bar = std::min(line.find('|', start), line.size());
                std::string option = line.substr(start, bar - start);
                const size_t arrow = option.find('>');
                auto trim = [](std::string s) {
                    const auto a = s.find_first_not_of(' ');
                    const auto b = s.find_last_not_of(' ');
                    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
                };
                DialogueChoice c{trim(option.substr(0, arrow)), arrow == std::string::npos ? "" : trim(option.substr(arrow + 1))};
                if (!c.label.empty()) pages.back().choices.push_back(c);
                start = bar + 1;
            }
            continue;
        }
        DialoguePage p;
        if (auto c = line.find(": "); c != std::string::npos && c < 24) {
            p.speaker = line.substr(0, c);
            p.text = line.substr(c + 2);
        } else {
            p.text = line;
        }
        g.dialogues[id].push_back(p);
    }
}

void openDialogue(Game& g, const std::string& id) {
    auto it = g.dialogues.find(id);
    if (it == g.dialogues.end()) return;
    static int depth = 0; // une branche « sinon » ne peut pas boucler indefiniment
    if (depth < 4) {
        const std::string redirect = runDialogueActions(g, id);
        if (!redirect.empty() && redirect != id) {
            ++depth;
            openDialogue(g, redirect);
            --depth;
            return;
        }
    }
    playSfx(g, "dialogue");
    g.box.id = id;
    g.box.pages = paginate(it->second);
    g.box.page = 0;
    g.box.shown = 0;
    g.box.choice = 0;
    g.box.prevChoiceDx = 0;
}

void updateDialogue(Game& g) {
    DialogueBox& b = g.box;
    if (!b.active()) return;
    const size_t len = foldAccents(b.pages[b.page].text).size();
    b.shown = std::min<float>(float(len), b.shown + 1.2f);
    const auto& choices = b.pages[b.page].choices;
    if (!choices.empty() && b.shown >= float(len)) { // question affichee : on attend une reponse
        const int dx = g.in.dx < 0 ? -1 : g.in.dx > 0 ? 1 : 0;
        if (dx != 0 && dx != b.prevChoiceDx) {
            if (dx < 0 && b.choice > 0) { --b.choice; playSfx(g, "curseur"); }
            if (dx > 0 && b.choice + 1 < choices.size()) { ++b.choice; playSfx(g, "curseur"); }
        }
        b.prevChoiceDx = dx;
        if (g.in.attackPressed || g.in.dashPressed) {
            if (g.in.dashPressed) b.choice = choices.size() - 1; // annuler = derniere reponse (« Non »)
            const DialogueChoice chosen = choices[b.choice];
            g.lastChoice = chosen.label;
            b.page = b.pages.size(); // la question se ferme
            playSfx(g, "valider");
            if (!chosen.target.empty()) openDialogue(g, chosen.target);
        }
        return;
    }
    if (g.in.attackPressed || g.in.dashPressed) {
        if (b.shown < len) b.shown = float(len); // complete la page
        else { ++b.page; b.shown = 0; playSfx(g, "page"); }
    }
}

void drawDialogue(Game& g) {
    const DialogueBox& b = g.box;
    if (!b.active()) return;
    SDL_Renderer* r = g.renderer;
    const DialoguePage& p = b.pages[b.page];
    const std::string text = foldAccents(p.text).substr(0, size_t(b.shown));
    const std::string speaker = foldAccents(p.speaker);

    // Geometrie native observee : panneau x~16..302, y~170..223 ; portrait
    // x~7..55, y~117..173 ; cartouche x~64..175, y~139..167.
    constexpr float BX = 16.f, BY = 170.f, BW = 286.f, BH = 54.f;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    drawWindow(r, BX, BY, BW, BH);

    // La table des portraits contient les noms UTF-8 (« Méphisto ») : on la
    // consulte avec le nom d'origine, pas avec sa forme Latin-1.
    const char* bank = p.speaker.empty() ? nullptr : portraitBank(p.speaker);
    const bool withPortrait = bank && spriteBank(g, bank) != nullptr;
    if (withPortrait) {
        drawPortrait(g, 7.f, 117.f, p.speaker);
        cartouche(r, 64.f, 139.f, 112.f, speaker);
    }

    const float ty = BY + 4.f;
    size_t line = 0;
    for (const auto& l : wrapLatin1(text, kTextMaxW)) {
        if (line >= kLinesPerPanel) break; // paginate() garantit trois lignes par panneau
        setInk(r);
        drawLatin1(r, kTextX, ty + float(line) * 16.f, l);
        ++line;
    }
    const bool complete = size_t(b.shown) >= foldAccents(p.text).size();
    if (complete && !p.choices.empty()) {
        // Reponses (capture A1 lp07_15 « YES / NO ») : petit parchemin au-dessus du panneau, a
        // droite ; reponses cote a cote, triangle gris de WIND.TX au-dessus de la reponse choisie.
        // Mesures de la reference : parchemin de 128 px dont le bord droit tombe en x~304, haut
        // en y~139 ; reponses a 16 px du bord. Les mots francais etant plus longs que YES/NO,
        // les positions suivent la largeur reelle des mots et le parchemin s'elargit vers la gauche.
        constexpr float CY = 139.f, CH = 28.f, RIGHT = 304.f, GAP = 16.f;
        std::vector<std::string> labels;
        float total = 0.f;
        for (const auto& c : p.choices) {
            labels.push_back(foldAccents(c.label));
            total += textWidth(labels.back());
        }
        const float CW = std::max(128.f, 32.f + total + GAP * float(labels.size() - 1));
        const float CX = RIGHT - CW;
        drawWindow(r, CX, CY, CW, CH);
        float ox = CX + 16.f;
        for (size_t i = 0; i < labels.size(); ++i) {
            setInk(r);
            drawLatin1(r, ox, CY + 6.f, labels[i]);
            if (i == b.choice) drawPageArrow(r, ox + 4.f, CY - 8.f);
            ox += textWidth(labels[i]) + GAP;
        }
        return;
    }
    // Indicateur de page (attesté : captures A1 lp01_7, lp02_19...) quand la
    // page est entierement affichee et qu'une suite existe.
    if (complete && b.page + 1 < b.pages.size())
        drawPageArrow(r, BX + BW - 14.f, BY + 31.f);
}

// --diag : chaque etape isole une seule source de corruption. Les images sont
// prises avec le meme binaire et le meme --shot que le jeu, sans decor.
void drawDiagnostic(Game& g) {
    SDL_Renderer* r = g.renderer;
    SDL_Texture* font = fontTexture(r);
    SDL_Texture* wind = windowTexture(r);
    auto label = [&](const char* s) {
        SDL_SetRenderDrawColor(r, 255, 240, 90, 255);
        drawText(r, 8.f, 6.f, s);
    };
    if (!font || !wind) { // echec SDL explicite, jamais un rendu silencieux
        SDL_SetRenderDrawColor(r, 255, 60, 60, 255);
        drawText(r, 8.f, 6.f, "TEXTURE SDL ABSENTE");
        return;
    }
    switch (g.diagStage) {
    case 1: { // atlas complet, en deux moitiemes 256x128
        const SDL_FRect src{0.f, 0.f, 256.f, 128.f};
        const SDL_FRect dst{32.f, 56.f, 256.f, 128.f};
        SDL_RenderTexture(r, font, &src, &dst);
        label("1 ATLAS FONT3 0..127");
        break;
    }
    case 2: {
        const SDL_FRect src{0.f, 128.f, 256.f, 128.f};
        const SDL_FRect dst{32.f, 56.f, 256.f, 128.f};
        SDL_RenderTexture(r, font, &src, &dst);
        label("2 ATLAS FONT3 128..255");
        break;
    }
    case 3: {
        setInk(r);
        drawText(r, 16.f, 60.f, "A b 0 1 accents: é è ç ù ? , . !");
        drawText(r, 16.f, 84.f, "Do, you mustn't. You're wrong.");
        label("3 LIGNE CONNUE");
        break;
    }
    case 4: { // WIND.TX brut, lectures haute et basse cote a cote
        const SDL_FRect top{0.f, 0.f, float(a1font::kWindW), 128.f};
        const SDL_FRect bottom{0.f, 128.f, float(a1font::kWindW), 128.f};
        const SDL_FRect dstTop{8.f, 8.f, float(a1font::kWindW), 128.f};
        SDL_RenderTexture(r, wind, &top, &dstTop);
        const SDL_FRect dstBottom{152.f, 8.f, float(a1font::kWindW), 128.f};
        SDL_RenderTexture(r, wind, &bottom, &dstBottom);
        label("4 WIND.TX BRUT 256x256 4bpp");
        break;
    }
    case 5: {
        drawWindow(r, 16.f, 96.f, 288.f, 64.f);
        label("5 FENETRE SEULE");
        break;
    }
    case 6: {
        drawWindow(r, 16.f, 96.f, 288.f, 64.f);
        setInk(r);
        drawLatin1(r, 28.f, 104.f, foldAccents("Do, you mustn't. You're wrong."));
        label("6 TEXTE DANS FENETRE");
        break;
    }
    case 7: {
        drawPortrait(g, 40.f, 52.f, "Alexia");
        label("7 PORTRAIT SEUL");
        break;
    }
    case 8: {
        if (!g.box.active()) openDialogue(g, "prunewell");
        drawDialogue(g);
        label("8 DIALOGUE COMPLET");
        break;
    }
    case 9: { // variante avec portrait + cartouche de nom (Alexia, banque A2 presente)
        if (!g.box.active() || g.box.id != "alexia_epave") openDialogue(g, "alexia_epave");
        if (g.box.active())
            g.box.shown = float(foldAccents(g.box.pages[g.box.page].text).size());
        drawDialogue(g);
        label("9 PORTRAIT + CARTOUCHE");
        break;
    }
    default: {
        label("0 FOND UNI");
        break;
    }
    }
}
