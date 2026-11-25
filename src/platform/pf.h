#pragma once
/*
 * Interface plateforme minimale partageable PC/PS1.
 *
 * STEM : aucun conteneur STL, aucune allocation dynamique, aucun destructeur
 * global. Types POD uniquement, afin que ce header soit compilable par le
 * toolchain MIPS de PSn00bSDK 0.24 (ni libstdc++ ni <vector> n'y existent).
 *
 * Les six fonctions ci-dessous suffisent au démarrage ; les services de jeu
 * (audio, sauvegarde, temps determine) seront ajoutes dans des headers
 * distincts au fur et a mesure du portage.
 */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PF_W 320
#define PF_H 240

/* VRAM PS1 : 1024x512 pixels 16 bits. Deux framebuffers cote a cote en haut. */
#define PF_VRAM_W 1024
#define PF_VRAM_H 512
#define PF_FB0_X 0
#define PF_FB0_Y 0
#define PF_FB1_X 320
#define PF_FB1_Y 0
/* Scratch de la frame heros courante (le reste des frames reste en RAM).
 * IMPORTANT : sous les framebuffers (y >= 240). Le placer dans x=320..639,
 * y<240 le faisait écraser par le clear du DRAWENV à chaque pfPresent(). */
#define PF_HERO_SCRATCH_X 512
#define PF_HERO_SCRATCH_Y 256
#define PF_HERO_SCRATCH_W 128
#define PF_HERO_SCRATCH_H 64
/* CLUT du heros : 64 banques de 16 mots, independantes du decor (P1SP identique
 * pour toutes les zones). Synchronise avec HERO_CLUTS de ps1_export_zone.py. */
#define PF_HERO_CLUT_X 512
#define PF_HERO_CLUT_Y 320
/* Atlas de police A1 (GENERE par tools/ps1_export_font.py, non commite) :
 * 256x256 pixels 4bpp = 64x256 demi-mots (4 pixels par demi-mot), juste sous
 * les framebuffers. Les 7 CLUT (or / blanc / cyan / gris / orange / bleu pale /
 * bleu aide) sont en x=128, y=256..262 : la 5e sert au "- remake -" du titre
 * (v0.8.15), la 6e aux lignes normales du panneau d'options (v0.8.16), la 7e
 * aux deux aides de pied de page du menu (v0.8.17). */
#define PF_FONT_X 0
#define PF_FONT_Y 256
#define PF_FONT_W 64
#define PF_FONT_H 256
#define PF_FONT_CLUT_X 128
#define PF_FONT_CLUT_Y 256
#define PF_FONT_BANK_GOLD 0
#define PF_FONT_BANK_IVORY 1
#define PF_FONT_BANK_CYAN 2
#define PF_FONT_BANK_GREY 3
#define PF_FONT_BANK_ORANGE 4
#define PF_FONT_BANK_PALE 5
#define PF_FONT_BANK_HINT 6
#define PF_FONT_BANK_SELECT 7
#define PF_FONT_BANK_SUBTITLE 8

/* Atlas WIND.TX de la fenetre de dialogue A1 : 256x256 4bpp = 64x256 demi-mots.
 * Ses 16 banques CLUT occupent 16 lignes en x=320, y=256. Ces deux rectangles
 * sont reserves dans tools/ps1_export_zone.py pour ne pas ecraser les tuiles. */
#define PF_WIND_X 256
#define PF_WIND_Y 256
#define PF_WIND_W 64
#define PF_WIND_H 256
#define PF_WIND_CLUT_X 320
#define PF_WIND_CLUT_Y 256

/* PNJ de la premiere interaction (Paco) : deux frames A1 idle rangees sous les
 * framebuffers, a cote du scratch heros. CLUT sur deux lignes consecutive. */
#define PF_NPC0_TEX_X 640
#define PF_NPC0_TEX_Y 256
#define PF_NPC1_TEX_X 656
#define PF_NPC1_TEX_Y 256
#define PF_NPC2_TEX_X 672
#define PF_NPC2_TEX_Y 256
#define PF_NPC_CLUT_X 640
#define PF_NPC_CLUT_Y 304

/* Acteurs du jalon boss (ogre 40x49, coffres 25x33) et de la salle du trone :
 * six slots plus larges que les PNJ, reserves dans ps1_export_zone.py et remplis
 * par actor.dat. CLUTs sur six lignes consecutives a partir de PF_ACTOR_CLUT_Y. */
#define PF_ACTOR0_TEX_X 704
#define PF_ACTOR1_TEX_X 736
#define PF_ACTOR2_TEX_X 768
/* Salle du trone : Mephisto (40x49), Alexia (24x43) et Roi (25x47). */
#define PF_ACTOR3_TEX_X 800
#define PF_ACTOR4_TEX_X 832
#define PF_ACTOR5_TEX_X 864
#define PF_ACTOR_TEX_Y 256
#define PF_ACTOR_CLUT_X 704
#define PF_ACTOR_CLUT_Y 320

/* HUD A1 : 4 gemmes (frames 1/4/5/8), 10 chiffres (9..18) et slash (19) dans
 * une seule page 4bpp, une CLUT par frame. Suffixe "/MM" identique a la cible
 * PC. Synchronise avec HUD_ATLAS / HUD_CLUTS de tools/ps1_export_zone.py. */
#define PF_HUD_TEX_X 64
#define PF_HUD_TEX_Y 256
#define PF_HUD_TEX_W 16
#define PF_HUD_TEX_H 224
#define PF_HUD_CLUT_X 64
#define PF_HUD_CLUT_Y 480

/* Scratch de l'animation de l'ogre de Kindra (v0.8.8) : les six vraies frames
 * A1 restent en RAM dans enemy_anim.dat, seule la frame courante est televerse
 * ici (et sa CLUT sur une ligne), comme pour le heros. Synchronise avec
 * ENEMY_TEX / ENEMY_CLUT de tools/ps1_export_zone.py. */
#define PF_HUD2_TEX_X 112
#define PF_HUD2_TEX_Y 256
#define PF_HUD2_TEX_H 128
#define PF_HUD2_CLUT_X 112
#define PF_HUD2_CLUT_Y 384
#define PF_ENEMY_TEX_X 80
#define PF_ENEMY_TEX_Y 256
#define PF_ENEMY_TEX_W 32
#define PF_ENEMY_TEX_H 64
#define PF_ENEMY_CLUT_X 80
#define PF_ENEMY_CLUT_Y 320

/* Atlas d'icones d'inventaire A1 (a1_icons.a1s, 84 frames 4bpp) : grille 12x7
 * de cellules 24x32 px = 6 demi-mots x 32 lignes, sous les slots acteurs.
 * Une seule CLUT 16 couleurs partagee juste apres la grille. Synchronise avec
 * ps1_export_zone.py (ICON_ATLAS / ICON_CLUT) et tools/ps1_export_icons.py. */
#define PF_ICON_TEX_X 896
#define PF_ICON_TEX_Y 256
#define PF_ICON_TEX_W 72
#define PF_ICON_TEX_H 224
#define PF_ICON_CLUT_X 896
#define PF_ICON_CLUT_Y 480
#define PF_ICON_CELL_W 6
#define PF_ICON_CELL_H 32

/* Boutons manette, valeurs actives en haut (alignees sur PAD_* du SDK). */
#define PF_BTN_SELECT  (1u << 0)
#define PF_BTN_START   (1u << 3)
#define PF_BTN_UP      (1u << 4)
#define PF_BTN_RIGHT   (1u << 5)
#define PF_BTN_DOWN    (1u << 6)
#define PF_BTN_LEFT    (1u << 7)
#define PF_BTN_L1      (1u << 10)
#define PF_BTN_R1      (1u << 11)
#define PF_BTN_TRIANGLE (1u << 12)
#define PF_BTN_CIRCLE  (1u << 13)
#define PF_BTN_CROSS   (1u << 14)
#define PF_BTN_SQUARE  (1u << 15)

/* Ordre reel des directions dans les animations A1SP (`hero.a1s`) :
 * dir0=dos/haut, dir1=profil GAUCHE, dir2=profil DROITE, dir3=face/bas.
 * Verifie le 2026-11-21 par inspection visuelle des frames agrandies et par le
 * retour matériel : avec l'ancien
 * ordre 1=droite/2=gauche, le heros marchait a reculons sur les deux profils.
 * Le commentaire de src/sprite.h (0=bas,1=haut,2=gauche,3=droite) ne
 * correspond pas aux donnees exportees ; le backend PS1 suit les frames.
 * Ne pas melanger avec l'ordre des bits PAD (PF_BTN_*). */
#define PF_DIR_UP     0
#define PF_DIR_LEFT   1
#define PF_DIR_RIGHT  2
#define PF_DIR_DOWN   3

/* Initialise GPU, double buffer, manette. A appeler une seule fois. */
void pfInit(void);

/* Termine la frame : presentation, inversion des buffers, compteur. */
void pfPresent(void);

/* Televerse en VRAM. x,y,w sont en demi-mots 16 bits : une image 4bpp de 24
 * pixels occupe w=6 demi-mots (4 pixels par demi-mot). h est en lignes. */
void pfUpload4bpp(int x, int y, int w, int h, const uint8_t* data);
void pfUpload16(int x, int y, int w, int h, const uint16_t* data);

/* Sprite 4bpp : (u,v) = coin haut-gauche de la texture en VRAM (demi-mots),
 * (w,h) = taille a l'ecran en pixels, (clutX,clutY) = position de la CLUT.
 * Le mot DR_TPAGE est emis automatiquement. */
void pfDrawSprite16(int x, int y, int u, int v, int w, int h, int clutX, int clutY, int z);

/* Rectangle opaque, coordonnees ecran 320x240, couleur BGR555 (15 bits).
 * z = priorite de dessin dans l'ordering table : 0 = premier plan,
 * 63 = arriere-plan (le SDK dessine les z decroissants). */
void pfDrawRect(int x, int y, int w, int h, uint32_t bgr555, int z);

/* Meme rectangle, mais emis avec le bit semi-transparent du GPU PS1.
 * Le melange materiel est 50/50 ; superposer 2 ou 3 passes donne 75/87,5 %.
 * Sert au voile du titre PS1 (aucune texture ni CLUT supplementaire). */
void pfDrawRectSemi(int x, int y, int w, int h, uint32_t bgr555, int z);

/* Masque PF_BTN_* actif en haut ; 0 si aucune manette. */
uint32_t pfPad(void);


/* Nombre de frames presentees depuis pfInit(). */
uint32_t pfFrame(void);

/* Ligne de diagnostic (sortie TTY/stdout selon la cible). */
void pfLog(const char* line);

#ifdef __cplusplus
}
#endif
