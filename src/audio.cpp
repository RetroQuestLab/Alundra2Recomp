// Musique : BGM d'Alundra 2 (partitions SSsq + instruments de PACK0.PAK), rendues en WAV par
// tools/a2_music.py dans assets/music/NN.wav (+ NN.loop = echantillon de debut de boucle).
// Lecture en boucle avec fondu entre deux morceaux. La musique de chaque zone est choisie dans
// data/world.txt (ligne "music <zone> <numero>") ; en jeu, M / N parcourent les morceaux et F6
// enregistre le morceau courant pour la zone.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "game.h"

static constexpr int RATE = 44100;
static constexpr int FADE_SAMPLES = RATE / 2; // 0,5 s

static void SDLCALL feed(void* user, SDL_AudioStream* stream, int additional, int) {
    Music& m = *static_cast<Music*>(user);
    if (additional <= 0) return;
    std::vector<int16_t> buf(size_t(additional) / 2, 0);
    for (size_t i = 0; i + 1 < buf.size(); i += 2) {
        // fondu : on descend jusqu'a 0, on change de morceau, puis on remonte
        if (m.pendingReady) {
            m.fade -= 1.f / FADE_SAMPLES;
            if (m.fade <= 0.f) {
                m.fade = 0.f;
                m.pcm.swap(m.pending);
                m.pending.clear();
                m.pos = 0;
                m.loop = m.pendingLoop;
                m.oneShot = m.pendingOneShot;
                m.ended = false;
                m.pendingReady = false;
            }
        } else if (m.fade < 1.f) {
            m.fade = std::min(1.f, m.fade + 1.f / FADE_SAMPLES);
        }
        float l = 0.f, r = 0.f;
        // bruitages : interpolation lineaire (hauteur variable), panoramique simple
        for (SfxVoice& v : m.voices) {
            const auto& s = m.sfx[size_t(v.id)];
            const size_t k = size_t(v.pos);
            if (k + 1 >= s.size()) { v.vol = 0.f; continue; }
            const float f = v.pos - float(k);
            const float x = (s[k] * (1.f - f) + s[k + 1] * f) * v.vol * m.sfxVolume;
            l += x * (v.pan > 0.f ? 1.f - v.pan : 1.f);
            r += x * (v.pan < 0.f ? 1.f + v.pan : 1.f);
            v.pos += v.step;
        }
        if (m.pcm.size() >= 2 && !(m.pos + 1 >= m.pcm.size() && m.oneShot)) {
            if (m.pos + 1 >= m.pcm.size()) m.pos = m.loop * 2 < m.pcm.size() ? m.loop * 2 : 0; // boucle
            const float g = m.fade * m.volume;
            l += m.pcm[m.pos] * g;
            r += m.pcm[m.pos + 1] * g;
            m.pos += 2;
        } else if (m.oneShot && m.pcm.size() >= 2) {
            m.ended = true; // jingle fini : silence
        }
        buf[i] = int16_t(std::clamp(l, -32768.f, 32767.f));
        buf[i + 1] = int16_t(std::clamp(r, -32768.f, 32767.f));
    }
    std::erase_if(m.voices, [](const SfxVoice& v) { return v.vol <= 0.f; });
    SDL_PutAudioStreamData(stream, buf.data(), int(buf.size() * 2));
}

void initAudio(Game& g, bool muted) {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("Audio indisponible : %s", SDL_GetError());
        return;
    }
    SDL_AudioSpec spec{SDL_AUDIO_S16, 2, RATE};
    g.music.volume = muted ? 0.f : 0.8f;
    g.music.sfxVolume = muted ? 0.f : 0.7f;
    g.music.stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feed, &g.music);
    if (!g.music.stream) {
        SDL_Log("Sortie audio : %s", SDL_GetError());
        return;
    }
    // nombre de morceaux disponibles
    for (int n = 0; n < 1000; ++n) {
        char name[16];
        std::snprintf(name, sizeof name, "%02d.wav", n);
        if (!std::filesystem::exists(g.assets / "music" / name)) break;
        g.music.count = n + 1;
    }
    SDL_ResumeAudioStreamDevice(g.music.stream);
}

static void startTrack(Game& g, int track, bool oneShot) {
    Music& m = g.music;
    if (!m.stream || track < 0 || track >= m.count || (track == m.track && !oneShot)) return;
    char name[16];
    std::snprintf(name, sizeof name, "%02d.wav", track);
    const auto path = (g.assets / "music" / name).string();
    size_t loop = 0;
    {
        std::ifstream lf((g.assets / "music" / name).replace_extension(".loop"));
        lf >> loop;
    }
    SDL_AudioSpec spec;
    Uint8* data = nullptr;
    Uint32 len = 0;
    if (!SDL_LoadWAV(path.c_str(), &spec, &data, &len)) {
        SDL_Log("Musique %s : %s", path.c_str(), SDL_GetError());
        return;
    }
    std::vector<int16_t> pcm(reinterpret_cast<int16_t*>(data), reinterpret_cast<int16_t*>(data) + len / 2);
    SDL_free(data);
    SDL_LockAudioStream(m.stream);
    m.pending.swap(pcm);
    m.pendingLoop = loop;
    m.pendingOneShot = oneShot;
    m.pendingReady = true;
    if (m.pcm.empty()) m.fade = 0.f;
    SDL_UnlockAudioStream(m.stream);
    m.track = track;
    m.jingle = oneShot;
    m.shownTicks = oneShot ? 0 : 120;
}

void playMusic(Game& g, int track) { startTrack(g, track, false); }

// Jingle (victoire, game over, zone liberee) : premier morceau de la ligne <name> de music.txt,
// joue une seule fois, puis la musique de la zone reprend.
void playJingle(Game& g, const char* name) {
    auto c = g.musicCats.find(name);
    if (c == g.musicCats.end() || c->second.empty()) return;
    g.music.boss = false;
    startTrack(g, c->second.front(), true);
}

void loadMusicCategories(Game& g) {
    std::ifstream in(g.data / "music.txt");
    std::string line;
    while (std::getline(in, line)) {
        if (auto c = line.find('#'); c != std::string::npos) line.resize(c);
        std::istringstream ss(line);
        std::string cat;
        if (!(ss >> cat)) continue;
        for (int n; ss >> n;) g.musicCats[cat].push_back(n);
    }
}

static const std::vector<int>* zoneCategory(Game& g) {
    auto k = g.zoneKind.find(zoneName(g));
    if (k == g.zoneKind.end()) return nullptr;
    auto c = g.musicCats.find(k->second);
    return c == g.musicCats.end() || c->second.empty() ? nullptr : &c->second;
}

// Musique de la zone : celle imposee par world.txt, sinon la premiere de sa categorie.
void playZoneMusic(Game& g) {
    auto it = g.zoneMusic.find(zoneName(g));
    if (it != g.zoneMusic.end()) playMusic(g, it->second);
    else if (auto* cat = zoneCategory(g)) playMusic(g, cat->front());
}

// M / N : morceau suivant / precedent, sans sortir de la categorie de la zone (coherence).
void cycleZoneMusic(Game& g, int step) {
    const std::vector<int>* cat = zoneCategory(g);
    std::vector<int> all;
    if (!cat) {
        for (int i = 0; i < g.music.count; ++i) all.push_back(i);
        cat = &all;
    }
    if (cat->empty()) return;
    int i = 0;
    for (size_t k = 0; k < cat->size(); ++k)
        if ((*cat)[k] == g.music.track) i = int(k);
    const int n = int(cat->size());
    playMusic(g, (*cat)[((i + step) % n + n) % n]);
}

// F6 : memorise le morceau courant pour la zone dans data/world.txt.
// La ligne est remplacee si elle existe deja : appuyer plusieurs fois ne fait pas grossir le fichier.
void saveZoneMusic(Game& g) {
    if (g.music.track < 0) return;
    const std::string zone = zoneName(g);
    g.zoneMusic[zone] = g.music.track;
    const auto path = g.data / "world.txt";
    std::ifstream in(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) lines.push_back(line);
    const std::string prefix = "music " + zone + " ";
    bool found = false;
    for (auto& l : lines)
        if (l.rfind(prefix, 0) == 0) { l = prefix + std::to_string(g.music.track); found = true; }
    if (!found) lines.push_back(prefix + std::to_string(g.music.track));
    std::ofstream out(path, std::ios::trunc);
    for (auto& l : lines) out << l << '\n';
    floatText(g, g.hero.x, g.hero.y - 40, "Musique enregistree", SDL_Color{210, 232, 225, 255});
    g.music.shownTicks = 0;
}



// Musique de boss : tant qu'un ogre vivant est proche du heros, on joue la categorie "boss" ;
// on revient a la musique de la zone quand il est vaincu ou distance.
void updateBossMusic(Game& g) {
    if (g.music.jingle) { // pas de changement pendant un jingle ; a la fin, retour a la zone
        bool ended = false;
        if (g.music.stream) {
            SDL_LockAudioStream(g.music.stream);
            ended = g.music.ended && !g.music.pendingReady;
            SDL_UnlockAudioStream(g.music.stream);
        }
        if (!ended) return;
        g.music.jingle = false;
        g.music.track = -1;
        playZoneMusic(g);
        g.music.shownTicks = 0;
        return;
    }
    bool near = false;
    for (const Enemy& e : g.enemies)
        if (e.kind == EnemyKind::Ogre && e.hp > 0 && std::hypot(e.x - g.hero.x, e.y - g.hero.y) < 140.f) near = true;
    auto cat = g.musicCats.find("boss");
    if (cat == g.musicCats.end() || cat->second.empty()) return;
    if (near && !g.music.boss) {
        g.music.boss = true;
        playMusic(g, cat->second.front());
        g.music.shownTicks = 0;
    } else if (!near && g.music.boss) {
        g.music.boss = false;
        playZoneMusic(g);
        g.music.shownTicks = 0;
    }
}

// --- Bruitages ---------------------------------------------------------------------------
// assets/sfx/NN.wav (extraits par tools/a2_sfx.py de la banque d'effets d'Alundra 2) ;
// data/sfx.txt associe chaque evenement du jeu a un numero : "<evenement> <numero> [hauteur] [volume]".
void loadSfx(Game& g) {
    for (int n = 0; n < 200; ++n) { // numero = identifiant du jeu original (banque systeme < 200)
        char name[16];
        std::snprintf(name, sizeof name, "%02d.wav", n);
        const auto path = (g.assets / "sfx" / name).string();
        if (!std::filesystem::exists(path)) {
            g.music.sfx.emplace_back();
            continue;
        }
        SDL_AudioSpec spec;
        Uint8* data = nullptr;
        Uint32 len = 0;
        std::vector<int16_t> s;
        if (SDL_LoadWAV(path.c_str(), &spec, &data, &len)) {
            const int16_t* p = reinterpret_cast<int16_t*>(data);
            const size_t ch = size_t(std::max(1, spec.channels));
            for (size_t i = 0; i + ch <= len / 2; i += ch) s.push_back(p[i]);
            SDL_free(data);
        }
        g.music.sfx.push_back(std::move(s));
    }
    std::ifstream in(g.data / "sfx.txt");
    std::string line;
    while (std::getline(in, line)) {
        if (auto c = line.find('#'); c != std::string::npos) line.resize(c);
        std::istringstream ss(line);
        std::string ev;
        SfxDef d;
        if (!(ss >> ev >> d.id)) continue;
        ss >> d.pitch >> d.vol;
        if (!ss) ss.clear();
        g.sfxMap[ev] = d;
    }
}

void playSfxId(Game& g, int id, float pitch, float vol, float pan) {
    Music& m = g.music;
    if (!m.stream || id < 0 || id >= int(m.sfx.size()) || m.sfx[size_t(id)].size() < 2) return;
    SDL_LockAudioStream(m.stream);
    if (m.voices.size() < 16) m.voices.push_back({id, 0.f, pitch, vol, std::clamp(pan, -0.8f, 0.8f)});
    SDL_UnlockAudioStream(m.stream);
}

// Joue le bruitage d'un evenement, avec une petite variation de hauteur (evite la repetition
// mecanique) et un panoramique selon la position a l'ecran.
void playSfx(Game& g, const char* name, float x) {
    auto it = g.sfxMap.find(name);
    if (it == g.sfxMap.end()) return;
    const SfxDef& d = it->second;
    const float var = 1.f + (float(std::rand() % 1000) / 1000.f - 0.5f) * 0.08f;
    const float pan = x < 0.f ? 0.f : (x - g.hero.x) / 200.f;
    playSfxId(g, d.id, d.pitch * var, d.vol, pan);
}
