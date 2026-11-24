// GENERE par tools/ps1_export_icons.py -- ne pas editer a la main.
// Source : a1_icons.a1s + banks.txt + item_icons.txt (A1 non publie).
#pragma once

#include <stdint.h>

struct PSIIconEntry {
	char code[24];
	uint8_t frame;
};

static const PSIIconEntry kPsiIcons[] = {
	{ "long_sword", 8 },
	{ "bronze_sword", 7 },
	{ "broad_sword", 25 },
	{ "rune_sword", 19 },
	{ "shining_sword", 26 },
	{ "rising_sword", 36 },
	{ "epee_supreme", 36 },
	{ "herb", 68 },
	{ "herbe", 68 },
	{ "tonic", 16 },
	{ "elixir", 47 },
	{ "panacea", 46 },
	{ "lorien_nuts", 48 },
	{ "grapes_of_eru", 49 },
	{ "pixie_ring", 13 },
	{ "siren_ring", 14 },
	{ "dryad_ring", 32 },
	{ "newt_ring", 30 },
	{ "compass", 34 },
	{ "elemental_charm", 55 },
	{ "elemental_ring", 76 },
	{ "wonder_orb", 37 },
	{ "elf_ring", 58 },
	{ "sp_elf_ring", 78 },
	{ "guard_ring", 77 },
	{ "sp_guard_ring", 74 },
	{ "amulet", 54 },
	{ "gauntlet", 29 },
	{ "gantelet", 29 },
	{ "star_prism", 73 },
	{ "prisme", 73 },
	{ "star_crystal", 35 },
	{ "cristal", 35 },
	{ "lamp_of_lumiere", 17 },
	{ "lampe", 17 },
	{ "strange_key", 15 },
	{ "cle", 15 },
	{ "hand_carved_cow", 11 },
	{ "hammer", 38 },
	{ "red_statue", 10 },
	{ "idole", 10 },
	{ "pirate_earring", 72 },
	{ "cow_princess_autograph", 3 },
	{ "vanilla_wafer", 5 },
	{ "goo_goo_bird", 18 },
	{ "crimson_cape", 57 },
	{ "barons_orders", 63 },
	{ "ordre", 63 },
	{ "lettre", 65 },
	{ "coeur", 61 },
	{ "puzzle_piece", 66 },
};

static const int kPsiIconCount = 51;
