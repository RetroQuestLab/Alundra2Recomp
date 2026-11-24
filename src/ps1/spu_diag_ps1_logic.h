#pragma once

#include <cstdint>

/*
 * Logique pure de la table d'occupation SPU PS1, partagee entre le build MIPS
 * et le test hote. Aucune STL, aucun appel systeme : POD et fonctions inline.
 *
 * Un bit represente un bloc de 8 octets de RAM SPU (524288 / 8 / 8 = 8192
 * octets de bitmap). Les deux seuls chemins d'ecriture du port sont la banque
 * VAG (SpuWrite dans audioInitPS1) et le double buffer musical (SpuWrite dans
 * le handler IRQ de cd_stream.cpp). L'occupation est une union : reecrire une
 * plage ne double pas son cout. Les 4 Kio reserves par le SDK ne sont pas
 * comptes : ils ne sont pas ecrits par le jeu.
 *
 * Les 16 mots du POD sont lus directement par le harnais Lua : l'ordre des
 * champs est un contrat (ne pas changer sans mettre a jour capture_spu.lua).
 */
struct PS1SpuDiag {
	uint32_t magic;
	uint32_t ready;
	uint32_t frame;
	uint32_t transfers;
	uint32_t currentBytes;
	uint32_t peakBytes;
	uint32_t peakFrame;
	uint32_t overlapBytes;
	uint32_t rejections;
	uint32_t spuBytes;
	uint32_t reservedBytes;
	uint32_t bankBytes;
	uint32_t streamBytes;
	uint32_t chunks;
	uint32_t underruns;
	uint32_t integrity;
};

constexpr uint32_t kSpuDiagMagic = 0xA2535044u;
constexpr uint32_t kSpuDiagSpuBytes = 524288u;
constexpr uint32_t kSpuDiagReservedBytes = 4096u;
constexpr uint32_t kSpuDiagBlockShift = 3u;
constexpr uint32_t kSpuDiagBitsetBytes =
    kSpuDiagSpuBytes >> (kSpuDiagBlockShift + 3u);
constexpr uint32_t kSpuDiagStreamSplit = 0x5D600u;

inline uint32_t spuDiagIntegrity(const PS1SpuDiag& s) {
	return s.magic ^ s.transfers ^ s.currentBytes ^ s.peakBytes ^
	       s.overlapBytes;
}

inline void spuDiagInit(PS1SpuDiag& s) {
	s = PS1SpuDiag{kSpuDiagMagic, 1u,       0u, 0u, 0u, 0u, 0u, 0u,
	               0u,           kSpuDiagSpuBytes, kSpuDiagReservedBytes,
	               0u,           0u,       0u, 0u, 0u};
	s.integrity = spuDiagIntegrity(s);
}

inline void spuDiagStampFrame(PS1SpuDiag& s, uint32_t frame) {
	s.frame = frame;
}

// Retourne 1 si la plage est acceptee, 0 si elle est hors RAM SPU, non
// alignee sur 8 octets, vide ou a cheval sur la frontiere banque/stream.
// Le bitmap doit contenir kSpuDiagBitsetBytes octets mis a zero avant le
// premier appel.
inline uint32_t spuDiagTrackRange(PS1SpuDiag& s, uint8_t* bits, uint32_t addr,
                                  uint32_t bytes, uint32_t frame) {
	if (bits == nullptr || bytes == 0u || (addr & 7u) != 0u ||
	    (bytes & 7u) != 0u || addr >= kSpuDiagSpuBytes ||
	    bytes > kSpuDiagSpuBytes - addr) {
		++s.rejections;
		return 0u;
	}
	const uint32_t end = addr + bytes;
	const bool inBank = end <= kSpuDiagStreamSplit;
	const bool inStream = addr >= kSpuDiagStreamSplit;
	if (!inBank && !inStream) {
		++s.rejections;
		return 0u;
	}

	uint32_t first = addr >> kSpuDiagBlockShift;
	const uint32_t count = bytes >> kSpuDiagBlockShift;
	uint32_t newBytes = 0u;
	uint32_t overlappedBytes = 0u;
	for (uint32_t i = 0u; i < count; ++i, ++first) {
		const uint32_t mask = 1u << (first & 7u);
		uint8_t& byte = bits[first >> 3u];
		if ((byte & mask) != 0u) {
			overlappedBytes += 8u;
		} else {
			byte = static_cast<uint8_t>(byte | mask);
			newBytes += 8u;
		}
	}

	s.currentBytes += newBytes;
	s.overlapBytes += overlappedBytes;
	++s.transfers;
	if (newBytes != 0u) {
		if (inBank) {
			s.bankBytes += newBytes;
		} else {
			s.streamBytes += newBytes;
		}
	}
	if (s.currentBytes > s.peakBytes) {
		s.peakBytes = s.currentBytes;
		s.peakFrame = frame;
	}
	s.frame = frame;
	s.integrity = spuDiagIntegrity(s);
	return 1u;
}

inline uint32_t spuDiagValidate(const PS1SpuDiag& s) {
	return s.magic == kSpuDiagMagic && s.ready != 0u && s.transfers != 0u &&
	       s.rejections == 0u && s.spuBytes == kSpuDiagSpuBytes &&
	       s.reservedBytes == kSpuDiagReservedBytes &&
	       s.integrity == spuDiagIntegrity(s) &&
	       s.bankBytes + s.streamBytes == s.currentBytes &&
	       s.currentBytes <= s.peakBytes && s.peakBytes <= s.spuBytes &&
	       s.peakFrame != 0u ? 1u : 0u;
}
