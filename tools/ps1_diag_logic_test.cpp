/*
 * Test hote de la logique de diagnostic PS1 (TDD).
 * Compilation : clang++ -std=c++20 -o qa\ps1_logs\harness_v0819\diag_logic_test.exe tools\ps1_diag_logic_test.cpp
 */
#include "../src/ps1/diag_ps1_logic.h"

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

constexpr uint32_t kTestStackTop = 0x801ffff8u;
constexpr uint32_t kTestPaintLo = 0x801f0000u;

void testInitState() {
	PS1DiagState s{};
	diagInitState(s, kTestStackTop, kTestPaintLo);
	CHECK(s.magic == kDiagMagic);
	CHECK(s.heapTotal == 0);
	CHECK(s.heapReserved == 0);
	CHECK(s.heapStackReserve == 0);
	CHECK(s.heapAlloc == 0);
	CHECK(s.heapPeak == 0);
	CHECK(s.stackTop == kTestStackTop);
	CHECK(s.stackPaintLo == kTestPaintLo);
	CHECK(s.stackFloor == kTestStackTop);
	CHECK(s.stackDepthMax == 0);
	CHECK(s.sampleCount == 0);
}

void testHeapPeakIsMonotonic() {
	PS1DiagState s{};
	diagInitState(s, kTestStackTop, kTestPaintLo);

	DiagHeapSnapshot snap{1000, 200, 800, 100, 100};
	diagMergeHeap(s, snap);
	CHECK(s.heapTotal == 1000);
	CHECK(s.heapReserved == 200);
	CHECK(s.heapStackReserve == 800);
	CHECK(s.heapAlloc == 100);
	CHECK(s.heapPeak == 100);
	CHECK(s.sampleCount == 1);

	snap.alloc = 40;       // allocation liberee
	snap.allocMax = 100;   // le pic SDK reste 100
	diagMergeHeap(s, snap);
	CHECK(s.heapAlloc == 40);
	CHECK(s.heapPeak == 100);
	CHECK(s.sampleCount == 2);

	snap.alloc = 220;
	snap.allocMax = 220;
	diagMergeHeap(s, snap);
	CHECK(s.heapAlloc == 220);
	CHECK(s.heapPeak == 220);
	CHECK(s.sampleCount == 3);
}

void testStackScanIgnoresGuardAndFindsLowestTouch() {
	uint32_t words[16];
	for (uint32_t& w : words) w = kDiagPaintPattern;
	words[1] = 0xDEADBEEFu;  // dans la garde (index 0..1) : ignore
	words[5] = 0x12345678u;  // plus bas mot modifie hors garde
	words[9] = 0x87654321u;
	const uint32_t found = diagScanStack(words, 16, kDiagPaintPattern, 2);
	CHECK(found == 5);
}

void testStackScanReturnsSentinelWhenUntouched() {
	uint32_t words[16];
	for (uint32_t& w : words) w = kDiagPaintPattern;
	words[0] = 0xDEADBEEFu;  // sous la garde : ignore
	words[1] = 0xCAFEBABEu;
	const uint32_t found = diagScanStack(words, 16, kDiagPaintPattern, 2);
	CHECK(found == 16);
}

void testStackMergeOnlyLowersFloor() {
	PS1DiagState s{};
	diagInitState(s, kTestStackTop, kTestPaintLo);

	diagMergeStack(s, 0x801ff000u);
	CHECK(s.stackFloor == 0x801ff000u);
	CHECK(s.stackDepthMax == 0xff8u);

	diagMergeStack(s, 0x801fe000u);
	CHECK(s.stackFloor == 0x801fe000u);
	CHECK(s.stackDepthMax == 0x1ff8u);

	// Un balayage plus haut ne doit pas effacer le pic.
	diagMergeStack(s, 0x801fff00u);
	CHECK(s.stackFloor == 0x801fe000u);
	CHECK(s.stackDepthMax == 0x1ff8u);
}
}  // namespace

int main() {
	testInitState();
	testHeapPeakIsMonotonic();
	testStackScanIgnoresGuardAndFindsLowestTouch();
	testStackScanReturnsSentinelWhenUntouched();
	testStackMergeOnlyLowersFloor();

	std::printf("PS1_DIAG_LOGIC: %d/%d checks PASS\n", gChecks - gFail, gChecks);
	return gFail == 0 ? 0 : 1;
}
