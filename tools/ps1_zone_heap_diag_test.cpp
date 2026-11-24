/*
 * Test hote de la logique du diagnostic de zones PS1 (TDD).
 * Compile les vrais en-tetes partages avec le build MIPS.
 * Compilation : clang++ -std=c++20 -o build\ps1_zone_heap_diag_test.exe tools\ps1_zone_heap_diag_test.cpp
 */
#include "../src/ps1/zone_heap_ps1_logic.h"

#include <cstdint>
#include <cstdio>

namespace {
int gFail = 0;
int gChecks = 0;
#define CHECK(cond)                                                     \
	do {                                                                  \
		++gChecks;                                                         \
		if (!(cond)) {                                                      \
			std::printf("FAIL ligne %d : %s\n", __LINE__, #cond);            \
			++gFail;                                                       \
		}                                                                   \
	} while (0)

void testInit() {
	PS1ZoneHeapDiag s{};
	zoneHeapDiagInit(s, 0x00123456u);
	CHECK(s.magic == kZoneHeapMagic);
	CHECK(s.staticRam == 0x00123456u);
	CHECK(s.cmd == 0u);
	CHECK(s.done == 0u);
	CHECK(s.step == 0u);
	CHECK(s.zone == 0u);
	CHECK(s.loadCount == 0u);
	CHECK(s.releaseCount == 0u);
	CHECK(s.errors == 0u);
	CHECK(zoneHeapDiagValidate(s) == 0);  // sequence pas terminee
}

void testCompleteSequenceReturnsToBaseline() {
	PS1ZoneHeapDiag s{};
	zoneHeapDiagInit(s, 0x00123456u);

	zoneHeapDiagBegin(s, 11, 6144);
	CHECK(s.baselineAlloc == 6144u);
	CHECK(s.step == 1u);
	CHECK(s.zone == 11u);
	CHECK(s.loadCount == 1u);
	CHECK(s.allocBefore == 6144u);
	zoneHeapDiagRecordFree(s, 6144);
	CHECK(s.releaseCount == 1u);
	CHECK(s.allocAfterFree == 6144u);
	zoneHeapDiagRecordLoad(s, 6144, 227328, 220006);
	CHECK(s.allocAfterLoad == 6144u);
	CHECK(s.peakAfterLoad == 227328u);
	CHECK(s.bytes == 220006u);

	zoneHeapDiagBegin(s, 13, 9000);
	CHECK(s.baselineAlloc == 6144u);  // premier appel seulement
	CHECK(s.step == 2u);
	zoneHeapDiagRecordFree(s, 6144);
	zoneHeapDiagRecordLoad(s, 161792, 227328, 154636);
	CHECK(s.allocAfterLoad == 161792u);
	CHECK(s.peakAfterLoad == 227328u);  // alloc_max SDK est monotene

	zoneHeapDiagBegin(s, 26, 9000);
	CHECK(s.step == 3u);
	zoneHeapDiagRecordFree(s, 6144);
	zoneHeapDiagRecordLoad(s, 178176, 227328, 168448);
	CHECK(s.bytes == 168448u);

	zoneHeapDiagFinish(s, 6144);
	CHECK(s.done == 1u);
	CHECK(s.step == kZoneHeapStepCount);  // 3 chargements + etape finale
	CHECK(s.finalAlloc == 6144u);
	CHECK(s.loadCount == 3u);
	CHECK(s.releaseCount == 3u);
	CHECK(s.errors == 0u);
	CHECK(zoneHeapDiagValidate(s) == 1);
}

void testSectorRounding() {
	CHECK(zoneHeapSectorsForBytes(0u) == 0u);
	CHECK(zoneHeapSectorsForBytes(1u) == 1u);
	CHECK(zoneHeapSectorsForBytes(2048u) == 1u);
	CHECK(zoneHeapSectorsForBytes(2049u) == 2u);
	CHECK(zoneHeapSectorsForBytes(220006u) == 108u);   // zone011.dat reel
	CHECK(zoneHeapSectorsForBytes(154636u) == 76u);    // zone013.dat reel
	CHECK(zoneHeapBufferBytes(220006u) == 221184u);
	CHECK(zoneHeapBufferBytes(154636u) == 155648u);
}

void testCompleteStepSignalsHarness() {
	PS1ZoneHeapDiag s{};
	zoneHeapDiagInit(s, 0u);
	zoneHeapDiagBegin(s, 11, 6144);
	zoneHeapDiagRecordFree(s, 6144);
	zoneHeapDiagRecordLoad(s, 227328, 227328, 220006);
	CHECK(s.done == 0u);
	zoneHeapDiagCompleteStep(s);
	CHECK(s.done == 1u);
	CHECK(s.cmd == 0u);
	CHECK(zoneHeapDiagValidate(s) == 0);  // une étape n'est pas la fin
}

void testFinishRejectsLeak() {
	PS1ZoneHeapDiag s{};
	zoneHeapDiagInit(s, 0u);
	zoneHeapDiagBegin(s, 11, 6144);
	zoneHeapDiagRecordFree(s, 6144);
	zoneHeapDiagRecordLoad(s, 227328, 227328, 220006);
	zoneHeapDiagBegin(s, 13, 9000);
	zoneHeapDiagRecordFree(s, 6144);
	zoneHeapDiagRecordLoad(s, 161792, 227328, 154636);
	zoneHeapDiagBegin(s, 26, 9000);
	zoneHeapDiagRecordFree(s, 6144);
	zoneHeapDiagRecordLoad(s, 178176, 227328, 168448);
	// Tout est coherent sauf le retour a la hauteur d'eau initiale : c'est
	// precisement la condition que ce test doit proteger.
	zoneHeapDiagFinish(s, 7000);
	CHECK(s.loadCount == 3u);
	CHECK(s.releaseCount == 3u);
	CHECK(zoneHeapDiagValidate(s) == 0);
}

void testRecordLoadRejectsPeakBelowAlloc() {
	PS1ZoneHeapDiag s{};
	zoneHeapDiagInit(s, 0u);
	zoneHeapDiagBegin(s, 11, 6144);
	zoneHeapDiagRecordFree(s, 6144);
	zoneHeapDiagRecordLoad(s, 9000, 8000, 100);
	CHECK(s.errors == 1u);
	zoneHeapDiagFinish(s, 6144);
	CHECK(zoneHeapDiagValidate(s) == 0);
}
}  // namespace

int main() {
	testInit();
	testSectorRounding();
	testCompleteStepSignalsHarness();
	testCompleteSequenceReturnsToBaseline();
	testFinishRejectsLeak();
	testRecordLoadRejectsPeakBelowAlloc();

	std::printf("PS1_ZONE_HEAP_LOGIC: %d/%d checks PASS\n", gChecks - gFail, gChecks);
	return gFail == 0 ? 0 : 1;
}
