// GENERE par tools/ps1_export_intro.py -- ne pas editer a la main.
// Source : data/dialogues.txt [prologue], narration A2,
// pagination A1 : 254 px, 3 lignes par panneau, police FONT3.TIM.
#pragma once

static const char kIntroLines[][3][64] = {
	{ "Royaume de Varuna. Le roi a disparu", "sans laisser de trace. Sur son tr\364ne,", "un pantin ob\351it au baron Diaz." },
	{ "Sur les mers, les pirates de Lady", "Milena font la loi. On murmure", "qu'ils ont aid\351 le baron \340 prendre le" },
	{ "pouvoir.", "Cette nuit-l\340, un de leurs navires", "volants fend les nuages. \300 son bord," },
	{ "un clandestin : Flint, chasseur de", "pirates.", "Jouer les nourrices pour une" },
	{ "cargaison de bo\356tes de conserve !", "Milena me le paiera.", "Du calme, papa. Mais ces cr\351atures" },
	{ "qui pilotent... \" Gi Gi \", c'est tout ce", "qu'elles savent dire ?", "Chut ! Vous avez entendu ? Il y a" },
	{ "quelqu'un sur le pont !", "Un chasseur de pirates ! Il est \340 moi,", "les enfants. Voyons s'il encaisse mon \"" },
	{ "\351crase-cr\342ne \" !", "Le coup l'envoie par-dessus bord.", "Flint tombe. Longtemps. Puis la mer" },
	{ "se referme sur lui.", "Trois jours plus tard, la mar\351e le", "d\351pose sur une plage, tout pr\350s du" },
	{ "Village Paco.", "", "" },
};

static const int kIntroPanelCount = 10;

#include "intro_ps1_logic.h"  // INTRO_SCENE_*

// Cartes du prologue PC (src/title.cpp) : une par replique.
struct IntroCardData {
	int scene, nlines, zone, len, shipFirst, shipLast;
	const char* name;
	const char* lines[4];
};

static const IntroCardData kIntroCards[] = {
	{ INTRO_SCENE_ZONE, 3, 42, 104, 0, 0, "", { "Royaume de Varuna. Le roi a disparu", "sans laisser de trace. Sur son tr\364ne,", "un pantin ob\351it au baron Diaz.", "" } },
	{ INTRO_SCENE_ZONE, 4, 18, 111, 1, 1, "", { "Sur les mers, les pirates de Lady", "Milena font la loi. On murmure qu'ils", "ont aid\351 le baron \340 prendre le", "pouvoir." } },
	{ INTRO_SCENE_SHIP, 4, 0, 115, 2, 6, "", { "Cette nuit-l\340, un de leurs navires", "volants fend les nuages. \300 son bord,", "un clandestin : Flint, chasseur de", "pirates." } },
	{ INTRO_SCENE_SHIP, 3, 0, 83, 2, 6, "- Zeppo -", { "Jouer les nourrices pour une", "cargaison de bo\356tes de conserve !", "Milena me le paiera.", "" } },
	{ INTRO_SCENE_SHIP, 3, 0, 98, 2, 6, "- Ruby -", { "Du calme, papa. Mais ces cr\351atures", "qui pilotent... \" Gi Gi \", c'est tout", "ce qu'elles savent dire ?", "" } },
	{ INTRO_SCENE_SHIP, 2, 0, 57, 2, 6, "- Albert -", { "Chut ! Vous avez entendu ? Il y a", "quelqu'un sur le pont !", "", "" } },
	{ INTRO_SCENE_SHIP, 3, 0, 95, 2, 6, "- Zeppo -", { "Un chasseur de pirates ! Il est \340", "moi, les enfants. Voyons s'il", "encaisse mon \" \351crase-cr\342ne \" !", "" } },
	{ INTRO_SCENE_FALL, 3, 0, 89, 7, 7, "", { "Le coup l'envoie par-dessus bord.", "Flint tombe. Longtemps. Puis la mer", "se referme sur lui.", "" } },
	{ INTRO_SCENE_ZONE, 3, 26, 83, 8, 8, "", { "Trois jours plus tard, la mar\351e le", "d\351pose sur une plage, tout pr\350s du", "Village Paco.", "" } },
};

static const int kIntroCardCount = 9;
