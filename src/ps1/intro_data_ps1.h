// GENERE par tools/ps1_export_intro.py -- ne pas editer a la main.
// Source : data/dialogues.txt [prologue], narration A2,
// pagination A1 : 254 px, 3 lignes par panneau, police FONT3.TIM.
#pragma once

static const char kIntroLines[][3][64] = {
	{ "Royaume de Varuna. Le roi a disparu", "sans laisser de trace. Sur son tr\364ne,", "un pantin ob\351it au baron Diaz." },
	{ "Pour prendre le pouvoir, Diaz s'est", "alli\351 aux pirates et \340 un sorcier venu", "de nulle part : M\351phisto." },
	{ "Sur les mers, les pirates de Lady", "Milena font la loi. On murmure", "qu'ils ont aid\351 le baron \340 prendre le" },
	{ "pouvoir.", "Cette nuit-l\340, un de leurs navires", "volants fend les nuages. \300 son bord," },
	{ "un clandestin : Flint, chasseur de", "pirates.", "Les soldats de l'\351quipage ne parlent" },
	{ "pas et ne dorment pas. Une grosse cl\351", "tourne lentement dans leur dos.", "Jouer les nourrices pour une" },
	{ "cargaison de bo\356tes de conserve !", "Milena me le paiera.", "Du calme, papa. Mais ces cr\351atures" },
	{ "qui pilotent... \" Gi Gi \", c'est tout ce", "qu'elles savent dire ?", "C'est l'oeuvre de M\351phisto. Il change" },
	{ "les hommes en machines, et personne", "ne sait comment.", "Le baron non plus n'a pas confiance." },
	{ "Il nous a demand\351 de garder un oeil", "sur ce sorcier.", "Dans la cale, Flint trouve une cl\351" },
	{ "\351trange, cisel\351e comme celles des", "soldats. Il l'empoche sans bruit.", "Chut ! Vous avez entendu ? Il y a" },
	{ "quelqu'un sur le pont !", "Un chasseur de pirates ! Il est \340 moi,", "les enfants. Voyons s'il encaisse mon \"" },
	{ "\351crase-cr\342ne \" !", "Zeppo bondit, frappe lourdement... et", "s'effondre. Mais un soldat m\351canique" },
	{ "bien plus grand se dresse.", "Le combat fracasse la coque. Le pont", "c\350de, et le navire volant se brise" },
	{ "au-dessus de la mer.", "Flint tombe. Longtemps. Sous les", "vagues, une baleine immense, bard\351e" },
	{ "de m\351tal, ouvre un oeil.", "Trois jours plus tard, la mar\351e le", "d\351pose sur une plage, tout pr\350s du" },
	{ "Village Paco.", "", "" },
};

static const int kIntroPanelCount = 17;

#include "intro_ps1_logic.h"  // INTRO_SCENE_*

// Cartes du prologue PC (src/title.cpp) : une par replique.
struct IntroCardData {
	int scene, nlines, zone, len, shipFirst, shipLast;
	const char* name;
	const char* lines[4];
};

static const IntroCardData kIntroCards[] = {
	{ INTRO_SCENE_ZONE, 3, 42, 104, 0, 0, "", { "Royaume de Varuna. Le roi a disparu", "sans laisser de trace. Sur son tr\364ne,", "un pantin ob\351it au baron Diaz.", "" } },
	{ INTRO_SCENE_ZONE, 3, 42, 100, 1, 1, "", { "Pour prendre le pouvoir, Diaz s'est", "alli\351 aux pirates et \340 un sorcier", "venu de nulle part : M\351phisto.", "" } },
	{ INTRO_SCENE_ZONE, 4, 18, 111, 2, 2, "", { "Sur les mers, les pirates de Lady", "Milena font la loi. On murmure qu'ils", "ont aid\351 le baron \340 prendre le", "pouvoir." } },
	{ INTRO_SCENE_SHIP, 4, 0, 115, 3, 13, "", { "Cette nuit-l\340, un de leurs navires", "volants fend les nuages. \300 son bord,", "un clandestin : Flint, chasseur de", "pirates." } },
	{ INTRO_SCENE_SHIP, 3, 0, 106, 3, 13, "", { "Les soldats de l'\351quipage ne parlent", "pas et ne dorment pas. Une grosse cl\351", "tourne lentement dans leur dos.", "" } },
	{ INTRO_SCENE_SHIP, 3, 0, 83, 3, 13, "- Zeppo -", { "Jouer les nourrices pour une", "cargaison de bo\356tes de conserve !", "Milena me le paiera.", "" } },
	{ INTRO_SCENE_SHIP, 3, 0, 98, 3, 13, "- Ruby -", { "Du calme, papa. Mais ces cr\351atures", "qui pilotent... \" Gi Gi \", c'est tout", "ce qu'elles savent dire ?", "" } },
	{ INTRO_SCENE_SHIP, 3, 0, 90, 3, 13, "- Zeppo -", { "C'est l'oeuvre de M\351phisto. Il change", "les hommes en machines, et personne", "ne sait comment.", "" } },
	{ INTRO_SCENE_SHIP, 3, 0, 88, 3, 13, "- Albert -", { "Le baron non plus n'a pas confiance.", "Il nous a demand\351 de garder un oeil", "sur ce sorcier.", "" } },
	{ INTRO_SCENE_SHIP, 3, 0, 102, 3, 13, "", { "Dans la cale, Flint trouve une cl\351", "\351trange, cisel\351e comme celles des", "soldats. Il l'empoche sans bruit.", "" } },
	{ INTRO_SCENE_SHIP, 2, 0, 57, 3, 13, "- Albert -", { "Chut ! Vous avez entendu ? Il y a", "quelqu'un sur le pont !", "", "" } },
	{ INTRO_SCENE_SHIP, 3, 0, 95, 3, 13, "- Zeppo -", { "Un chasseur de pirates ! Il est \340", "moi, les enfants. Voyons s'il", "encaisse mon \" \351crase-cr\342ne \" !", "" } },
	{ INTRO_SCENE_SHIP, 3, 0, 101, 3, 13, "", { "Zeppo bondit, frappe lourdement... et", "s'effondre. Mais un soldat m\351canique", "bien plus grand se dresse.", "" } },
	{ INTRO_SCENE_SHIP, 3, 0, 92, 3, 13, "", { "Le combat fracasse la coque. Le pont", "c\350de, et le navire volant se brise", "au-dessus de la mer.", "" } },
	{ INTRO_SCENE_FALL, 3, 0, 93, 14, 14, "", { "Flint tombe. Longtemps. Sous les", "vagues, une baleine immense, bard\351e", "de m\351tal, ouvre un oeil.", "" } },
	{ INTRO_SCENE_ZONE, 3, 26, 83, 15, 15, "", { "Trois jours plus tard, la mar\351e le", "d\351pose sur une plage, tout pr\350s du", "Village Paco.", "" } },
};

static const int kIntroCardCount = 16;
