/*
 * Diagnostic memoire PS1 : pic heap reel et profondeur de pile MIPS.
 *
 * Le SDK maintient deja HeapUsage.alloc_max dans TrackHeapUsage() (appele par
 * malloc/free) : aucun wrapper malloc ni linker flag n'est necessaire.
 * La pile est mesuree en peignant une fenetre bornee de la reserve de pile,
 * puis en cherchant le premier mot modifie. Le balayage est periodicique pour
 * ne pas alourdir chaque image d'une lecture de 256 Kio.
 */
#include "diag_ps1.h"

#include <stdlib.h>

#include <cstdint>

extern "C" char _end[];  // symbole de script de linkage (fin du BSS)

namespace {

constexpr uint32_t kProbeBytes = 256u * 1024u;
constexpr uint32_t kGuardBytes = 512u;
constexpr uint32_t kScanPeriod = 60u;
constexpr uint32_t kWordMask = ~3u;

uint32_t s_heapStart = 0;
uint32_t s_scanHi = 0;

inline uint32_t alignDown(uint32_t value) { return value & kWordMask; }
inline uint32_t alignUp(uint32_t value) { return (value + 3u) & kWordMask; }

}  // namespace

PS1DiagState g_ps1Diag;

void diagInitPS1(void) {
	HeapUsage usage{};
	GetHeapUsage(&usage);

	uint32_t sp = 0;
	asm volatile("move %0, $sp" : "=r"(sp));

	const uint32_t heapStart =
	    static_cast<uint32_t>(reinterpret_cast<uintptr_t>(_end)) + 4u;
	// L'arene capture-only reduit `usage.total` : `heapStart + total` n'est plus
	// le sommet de la pile. Le releve doit partir du pointeur de pile reel et
	// rester au-dessus de l'arene, sinon la peinture ecraserait les blobs.
	const uint32_t arenaTop = heapStart + static_cast<uint32_t>(usage.total);
	const uint32_t stackTop = alignDown(sp);

	uint32_t probeLo = stackTop > kProbeBytes ? stackTop - kProbeBytes : arenaTop + 8u;
	if (probeLo < arenaTop + 8u) probeLo = arenaTop + 8u;
	uint32_t probeHi = stackTop > kGuardBytes ? stackTop - kGuardBytes : probeLo;
	if (probeHi < probeLo) probeHi = probeLo;
	probeLo = alignDown(probeLo);
	probeHi = alignDown(probeHi);

	for (uint32_t addr = probeLo; addr < probeHi; addr += 4u) {
		*reinterpret_cast<volatile uint32_t*>(addr) = kDiagPaintPattern;
	}

	s_heapStart = heapStart;
	s_scanHi = probeHi;
	diagInitState(g_ps1Diag, stackTop, probeLo);
}

void diagSamplePS1(void) {
	HeapUsage usage{};
	GetHeapUsage(&usage);

	const DiagHeapSnapshot snap{
	    static_cast<uint32_t>(usage.total),
	    static_cast<uint32_t>(usage.heap),
	    static_cast<uint32_t>(usage.stack),
	    static_cast<uint32_t>(usage.alloc),
	    static_cast<uint32_t>(usage.alloc_max),
	};
	diagMergeHeap(g_ps1Diag, snap);

	if ((g_ps1Diag.sampleCount % kScanPeriod) != 0u) return;
	if (s_scanHi == 0u) return;

	uint32_t lo = g_ps1Diag.stackPaintLo;
	const uint32_t heapFloor = alignUp(s_heapStart + snap.reserved) + 4u;
	if (heapFloor > lo) lo = heapFloor;
	if (lo >= s_scanHi) return;

	const uint32_t words = (s_scanHi - lo) / 4u;
	const uint32_t idx = diagScanStack(reinterpret_cast<const uint32_t*>(lo),
	                                    words, kDiagPaintPattern, 0u);
	if (idx < words) diagMergeStack(g_ps1Diag, lo + idx * 4u);
}
