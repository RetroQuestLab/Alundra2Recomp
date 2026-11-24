#pragma once

#include <cstdint>

// Logique pure du diagnostic PS1, partagée entre le build MIPS et le test hôte.
// Aucune STL, aucun appel système : uniquement des POD et des fonctions inline.

struct DiagHeapSnapshot {
	uint32_t total;
	uint32_t reserved;
	uint32_t stackReserve;
	uint32_t alloc;
	uint32_t allocMax;
};

struct PS1DiagState {
	uint32_t magic;
	uint32_t heapTotal;
	uint32_t heapReserved;
	uint32_t heapStackReserve;
	uint32_t heapAlloc;
	uint32_t heapPeak;
	uint32_t stackTop;
	uint32_t stackPaintLo;
	uint32_t stackFloor;
	uint32_t stackDepthMax;
	uint32_t sampleCount;
};

constexpr uint32_t kDiagMagic = 0xA2503144u;
constexpr uint32_t kDiagPaintPattern = 0xA5A5A5A5u;

// stackFloor starts at stackTop: no stack word touched means zero depth.
inline void diagInitState(PS1DiagState& s, uint32_t stackTop, uint32_t paintLo) {
	s = PS1DiagState{kDiagMagic, 0, 0, 0, 0, 0, stackTop, paintLo, stackTop, 0, 0};
}

inline void diagMergeHeap(PS1DiagState& s, const DiagHeapSnapshot& snap) {
	s.heapTotal = snap.total;
	s.heapReserved = snap.reserved;
	s.heapStackReserve = snap.stackReserve;
	s.heapAlloc = snap.alloc;
	const uint32_t peak = snap.allocMax > snap.alloc ? snap.allocMax : snap.alloc;
	if (peak > s.heapPeak) s.heapPeak = peak;
	++s.sampleCount;
}

// Returns the lowest modified word index at or after ignoreWords, or count
// when the painted pattern is intact in the scanned window.
inline uint32_t diagScanStack(const uint32_t* words, uint32_t count,
                              uint32_t pattern, uint32_t ignoreWords) {
	if (words == nullptr || count == 0 || ignoreWords >= count) return count;
	for (uint32_t i = ignoreWords; i < count; ++i) {
		if (words[i] != pattern) return i;
	}
	return count;
}

inline void diagMergeStack(PS1DiagState& s, uint32_t floorAddr) {
	if (floorAddr >= s.stackFloor) return;
	s.stackFloor = floorAddr;
	const uint32_t depth = s.stackTop > floorAddr ? s.stackTop - floorAddr : 0u;
	if (depth > s.stackDepthMax) s.stackDepthMax = depth;
}
