#pragma once

#include <cstdint>

/*
 * Logique pure du diagnostic de zone PS1 : POD partagé entre le module MIPS
 * et le test hôte. Aucune STL, aucun appel système.
 *
 * Les 16 mots sont lus directement par le harnais Lua ; l'ordre des champs
 * est donc un contrat (ne pas réordonner sans mettre à jour capture_zone_heap.lua).
 */
struct PS1ZoneHeapDiag {
	uint32_t magic;
	uint32_t cmd;
	uint32_t done;
	uint32_t step;
	uint32_t zone;
	uint32_t loadCount;
	uint32_t releaseCount;
	uint32_t errors;
	uint32_t baselineAlloc;
	uint32_t allocBefore;
	uint32_t allocAfterFree;
	uint32_t allocAfterLoad;
	uint32_t peakAfterLoad;
	uint32_t bytes;
	uint32_t finalAlloc;
	uint32_t staticRam;
};

constexpr uint32_t kZoneHeapMagic = 0xA25A5A5Au;
constexpr uint32_t kZoneHeapCmd = 0x5A5A5A5Au;
constexpr uint32_t kZoneHeapStepCount = 4u;  // 3 chargements + libération finale
constexpr uint32_t kZoneHeapSectorBytes = 2048u;

// Un CdRead PS1 travaille par secteur de 2048 octets ; le tampon malloc est
// arrondi au secteur superieur (aucune valeur de taille figee dans le MIPS).
inline uint32_t zoneHeapSectorsForBytes(uint32_t bytes) {
	return (bytes + kZoneHeapSectorBytes - 1u) / kZoneHeapSectorBytes;
}

inline uint32_t zoneHeapBufferBytes(uint32_t bytes) {
	return zoneHeapSectorsForBytes(bytes) * kZoneHeapSectorBytes;
}

inline void zoneHeapDiagInit(PS1ZoneHeapDiag& s, uint32_t staticRam) {
	s = PS1ZoneHeapDiag{kZoneHeapMagic, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
	                    0u,            0u, 0u, 0u, 0u, 0u, 0u, staticRam};
}

inline void zoneHeapDiagBegin(PS1ZoneHeapDiag& s, uint32_t zone,
                              uint32_t allocBefore) {
	if (s.step == 0u) s.baselineAlloc = allocBefore;
	s.cmd = 0u;
	s.done = 0u;
	s.zone = zone;
	s.allocBefore = allocBefore;
	++s.step;
	++s.loadCount;
}

inline void zoneHeapDiagRecordFree(PS1ZoneHeapDiag& s, uint32_t allocAfterFree) {
	s.allocAfterFree = allocAfterFree;
	++s.releaseCount;
}

inline void zoneHeapDiagRecordLoad(PS1ZoneHeapDiag& s, uint32_t allocAfterLoad,
                                   uint32_t peakAfterLoad, uint32_t bytes) {
	s.allocAfterLoad = allocAfterLoad;
	s.peakAfterLoad = peakAfterLoad;
	s.bytes = bytes;
	if (peakAfterLoad < allocAfterLoad) ++s.errors;
}

// Le Lua rearme cmd/done pour chaque commande ; le MIPS signale la fin d'une
// etape (chargement ou erreur) sans terminer la sequence de fuite.
inline void zoneHeapDiagCompleteStep(PS1ZoneHeapDiag& s) {
	s.cmd = 0u;
	s.done = 1u;
}

inline void zoneHeapDiagFinish(PS1ZoneHeapDiag& s, uint32_t finalAlloc) {
	s.finalAlloc = finalAlloc;
	s.cmd = 0u;
	s.done = 1u;
	++s.step;  // la liberation finale est la 4e etape annoncee au harnais
}

inline int zoneHeapDiagValidate(const PS1ZoneHeapDiag& s) {
	return s.magic == kZoneHeapMagic && s.done != 0u && s.errors == 0u &&
	       s.baselineAlloc != 0u && s.loadCount == kZoneHeapStepCount - 1u &&
	       s.releaseCount == kZoneHeapStepCount - 1u &&
	       s.finalAlloc == s.baselineAlloc ? 1 : 0;
}
