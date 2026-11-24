#pragma once

#include <cstdint>

/*
 * Logique pure de la table d'occupation VRAM PS1, partagee entre le build MIPS
 * et le test hote. Aucune STL, aucun appel systeme : POD et fonctions inline.
 *
 * Un bit represente un demi-mot VRAM (2 octets). Les uploads 4bpp/16bpp ont
 * des coordonnees RECT en demi-mots ; les framebuffers sont reserves une fois.
 * L'occupation est une union : reecrire un rectangle ne double pas son cout.
 *
 * Les 16 mots du POD sont lus directement par le harnais Lua : l'ordre des
 * champs est un contrat (ne pas changer sans mettre a jour capture_vram.lua).
 */
struct PS1VramDiag {
	uint32_t magic;
	uint32_t ready;
	uint32_t frame;
	uint32_t uploads;
	uint32_t overlapHalfwords;
	uint32_t currentBytes;
	uint32_t peakBytes;
	uint32_t peakFrame;
	uint32_t framebufferBytes;
	uint32_t texturePeakBytes;
	uint32_t rejections;
	uint32_t firstFrame;
	uint32_t lastFrame;
	uint32_t vramBytes;
	uint32_t halfwords;
	uint32_t integrity;
};

constexpr uint32_t kVramDiagMagic = 0xA2565241u;
constexpr uint32_t kVramDiagHalfwordsPerRow = 1024u;
constexpr uint32_t kVramDiagRows = 512u;
constexpr uint32_t kVramDiagBitsetBytes =
    (kVramDiagHalfwordsPerRow * kVramDiagRows) / 8u;

inline uint32_t vramDiagIntegrity(const PS1VramDiag& s) {
	return s.magic ^ s.uploads ^ s.currentBytes ^ s.peakBytes ^
	       s.overlapHalfwords;
}

inline void vramDiagInit(PS1VramDiag& s, uint32_t framebufferBytes,
                         uint32_t vramBytes) {
	s = PS1VramDiag{kVramDiagMagic, 1u, 0u, 0u, 0u, 0u, 0u, 0u,
	                framebufferBytes, 0u, 0u, 0u, 0u, vramBytes,
	                kVramDiagHalfwordsPerRow * kVramDiagRows, 0u};
	s.integrity = vramDiagIntegrity(s);
}

inline void vramDiagStampFrame(PS1VramDiag& s, uint32_t frame) {
	s.frame = frame;
	s.lastFrame = frame;
	if (s.firstFrame == 0u && frame != 0u) s.firstFrame = frame;
}

// Retourne 1 si le rectangle est accepte, 0 si ses coordonnees sortent de la
// VRAM ou si sa taille est nulle. Le bitmap doit contenir kVramDiagBitsetBytes
// octets mis a zero avant le premier appel.
inline uint32_t vramDiagTrackRect(PS1VramDiag& s, uint8_t* bits, int x, int y,
                                  int w, int h, uint32_t frame) {
	if (bits == nullptr || x < 0 || y < 0 || w <= 0 || h <= 0) {
		++s.rejections;
		return 0u;
	}
	const uint32_t ux = static_cast<uint32_t>(x);
	const uint32_t uy = static_cast<uint32_t>(y);
	const uint32_t uw = static_cast<uint32_t>(w);
	const uint32_t uh = static_cast<uint32_t>(h);
	if (ux >= kVramDiagHalfwordsPerRow || uy >= kVramDiagRows ||
	    uw > kVramDiagHalfwordsPerRow - ux || uh > kVramDiagRows - uy) {
		++s.rejections;
		return 0u;
	}

	uint32_t newBits = 0u;
	uint32_t overlap = 0u;
	for (uint32_t row = uy; row < uy + uh; ++row) {
		uint32_t index = row * kVramDiagHalfwordsPerRow + ux;
		for (uint32_t col = 0u; col < uw; ++col, ++index) {
			const uint32_t mask = static_cast<uint32_t>(1u) << (index & 7u);
			uint8_t& byte = bits[index >> 3u];
			if ((byte & mask) != 0u) {
				++overlap;
			} else {
				byte = static_cast<uint8_t>(byte | mask);
				++newBits;
			}
		}
	}

	s.currentBytes += newBits * 2u;
	s.overlapHalfwords += overlap;
	++s.uploads;
	if (s.currentBytes > s.peakBytes) {
		s.peakBytes = s.currentBytes;
		s.peakFrame = frame;
	}
	const uint32_t textureBytes = s.currentBytes > s.framebufferBytes
	                                  ? s.currentBytes - s.framebufferBytes
	                                  : 0u;
	if (textureBytes > s.texturePeakBytes) s.texturePeakBytes = textureBytes;
	s.frame = frame;
	s.lastFrame = frame;
	if (s.firstFrame == 0u && frame != 0u) s.firstFrame = frame;
	s.integrity = vramDiagIntegrity(s);
	return 1u;
}

inline uint32_t vramDiagValidate(const PS1VramDiag& s) {
	return s.magic == kVramDiagMagic && s.ready != 0u && s.uploads != 0u &&
	       s.rejections == 0u && s.vramBytes == 1048576u &&
	       s.halfwords == 524288u && s.integrity == vramDiagIntegrity(s) &&
	       s.peakBytes >= s.framebufferBytes && s.peakBytes <= s.vramBytes &&
	       s.texturePeakBytes <= s.peakBytes - s.framebufferBytes &&
	       s.peakFrame != 0u ? 1u : 0u;
}
