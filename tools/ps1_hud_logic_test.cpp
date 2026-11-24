/*
 * Test hote du mapping des gemmes du HUD PS1 (TDD).
 * Compilation : clang++ -std=c++20 -o ps1\build\ps1_hud_logic_test.exe tools\ps1_hud_logic_test.cpp
 *
 * Les valeurs attendues sont celles de src/main.cpp::drawHud, figees a la main
 * pour eviter un test qui recalcule l'implementation sous test.
 */
#include "../src/ps1/hud_ps1_logic.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {
int g_fail = 0;
int g_checks = 0;
#define CHECK(cond)                                                     \
	do {                                                                  \
		++g_checks;                                                         \
		if (!(cond)) {                                                      \
			std::printf("FAIL ligne %d : %s\n", __LINE__, #cond);            \
			++g_fail;                                                       \
		}                                                                   \
	} while (0)
}  // namespace

void hud_gem_mapping_matches_pc_reference() {
	CHECK(hudBigGemCount(10) == 0);
	CHECK(hudBigGemCount(20) == 1);
	CHECK(hudBigGemCount(30) == 2);
	// Un maxHp extreme ne doit pas deborder le calcul (max 99, comme /MM).
	CHECK(hudBigGemCount(-5) == 0);
	CHECK(hudBigGemCount(100) == 9);

	CHECK(hudSmallFull(0, 10, 0) == 0);
	CHECK(hudSmallFull(1, 10, 0) == 1);
	CHECK(hudSmallFull(1, 10, 1) == 0);
	CHECK(hudSmallFull(8, 10, 7) == 1);
	CHECK(hudSmallFull(8, 10, 8) == 0);
	CHECK(hudSmallFull(10, 10, 9) == 1);

	CHECK(hudBigGemFull(0, 20, 0) == 0);
	CHECK(hudBigGemFull(10, 20, 0) == 0);
	CHECK(hudBigGemFull(11, 20, 0) == 1);
	CHECK(hudSmallFull(11, 20, 0) == 1);
	CHECK(hudSmallFull(20, 20, 9) == 1);
}

void hud_layout_uses_small_gems_first_row_second_row() {
	CHECK(hudSmallGemX(0) == 120);
	CHECK(hudSmallGemX(4) == 152);
	CHECK(hudSmallGemX(5) == 120);
	CHECK(hudSmallGemX(9) == 152);
	CHECK(hudSmallGemY(0) == 16);
	CHECK(hudSmallGemY(4) == 16);
	CHECK(hudSmallGemY(5) == 24);
	CHECK(hudSmallGemY(9) == 24);
}

void hud_max_digits_match_pc_reference() {
	// src/main.cpp::drawHud : "/"+ deux chiffres, zero initial, borne 99.
	CHECK(hudMaxDigit(10, 0) == 1);
	CHECK(hudMaxDigit(10, 1) == 0);
	CHECK(hudMaxDigit(9, 0) == 0);
	CHECK(hudMaxDigit(9, 1) == 9);
	CHECK(hudMaxDigit(99, 0) == 9);
	CHECK(hudMaxDigit(99, 1) == 9);
	CHECK(hudMaxDigit(100, 0) == 9);
	CHECK(hudMaxDigit(100, 1) == 9);
	CHECK(hudMaxDigit(-1, 0) == 0);
	CHECK(hudMaxDigit(-1, 1) == 0);
	CHECK(hudMaxDigit(0, 0) == 0);
	CHECK(hudMaxDigit(0, 1) == 0);
	CHECK(hudMaxDigit(10, 2) == 0);
	CHECK(hudMaxDigit(10, -1) == 0);
}

namespace {

void put16(std::vector<unsigned char>& blob, size_t at, int value) {
	blob[at] = (unsigned char)(value & 0xff);
	blob[at + 1] = (unsigned char)((value >> 8) & 0xff);
}

std::vector<unsigned char> makeHudBlob(int count = 15) {
	// Chaque frame : 16 octets d'en-tete + 32 de CLUT + 8*2*16 de pixels.
	std::vector<unsigned char> blob(8 + (size_t)count * (16 + 32 + 256), 0);
	blob[0] = 'P'; blob[1] = '1'; blob[2] = 'H'; blob[3] = 'U';
	put16(blob, 4, count);
	for (int i = 0; i < count; ++i) {
		const size_t p = 8 + (size_t)i * (16 + 32 + 256);
		put16(blob, p, 8);       // w
		put16(blob, p + 2, 16);  // h
		put16(blob, p + 8, 8);   // stride
		put16(blob, p + 10, 16); // uploadH
		put16(blob, p + 12, i * 16); // atlasY
		put16(blob, p + 14, i);      // clutY
	}
	return blob;
}

}  // namespace

void hud_blob_validation_rejects_corrupt_input() {
	const std::vector<unsigned char> valid = makeHudBlob();
	CHECK(valid.size() == 4568);
	CHECK(hudBlobValidate(valid.data(), (unsigned)valid.size(), 16, 224) == 1);
	CHECK(hudBlobValidate(nullptr, (unsigned)valid.size(), 16, 224) == 0);
	CHECK(hudBlobValidate(valid.data(), 7, 16, 224) == 0);

	std::vector<unsigned char> bad = valid;
	bad[0] = 'X';
	CHECK(hudBlobValidate(bad.data(), (unsigned)bad.size(), 16, 224) == 0);

	bad = valid; put16(bad, 4, 14);
	CHECK(hudBlobValidate(bad.data(), (unsigned)bad.size(), 16, 224) == 0);
	bad = valid; put16(bad, 4, 16);
	CHECK(hudBlobValidate(bad.data(), (unsigned)bad.size(), 16, 224) == 0);

	CHECK(hudBlobValidate(valid.data(), (unsigned)valid.size() - 1, 16, 224) == 0);
	CHECK(hudBlobValidate(valid.data(), (unsigned)valid.size() + 1, 16, 224) == 0);

	// stride est en demi-mots VRAM (4 pixels 4bpp/unite, voir pad_rows).
	// w=16/stride=8 est accepte ; w=32/stride=8 est refuse par le contrat
	// conservateur w <= 2*stride, bien que la capacite reelle soit 4*stride.
	bad = valid; put16(bad, 8 + 0, 16);
	CHECK(hudBlobValidate(bad.data(), (unsigned)bad.size(), 16, 224) == 1);
	bad = valid; put16(bad, 8 + 0, 32);
	CHECK(hudBlobValidate(bad.data(), (unsigned)bad.size(), 16, 224) == 0);

	bad = valid; put16(bad, 8 + 8, 0);  // stride de la frame 0
	CHECK(hudBlobValidate(bad.data(), (unsigned)bad.size(), 16, 224) == 0);
	bad = valid; put16(bad, 8 + 10, 8); // uploadH < h
	CHECK(hudBlobValidate(bad.data(), (unsigned)bad.size(), 16, 224) == 0);
	bad = valid; put16(bad, 8 + 8, 32); // stride > maxStride
	CHECK(hudBlobValidate(bad.data(), (unsigned)bad.size(), 16, 224) == 0);
	CHECK(hudBlobValidate(valid.data(), (unsigned)valid.size(), 7, 224) == 0);
	CHECK(hudBlobValidate(valid.data(), (unsigned)valid.size(), 16, 8) == 0);
}

int main() {
	hud_gem_mapping_matches_pc_reference();
	hud_layout_uses_small_gems_first_row_second_row();
	hud_max_digits_match_pc_reference();
	hud_blob_validation_rejects_corrupt_input();

	if (g_fail != 0) {
		std::printf("PS1_HUD_LOGIC: %d/%d checks FAIL\n", g_checks - g_fail, g_checks);
		return 1;
	}
	std::printf("PS1_HUD_LOGIC: %d/%d checks PASS\n", g_checks, g_checks);
	return 0;
}
