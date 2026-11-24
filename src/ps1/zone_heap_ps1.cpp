/*
 * Harnais capture-only de mesure du pic heap PS1 par zone.
 *
 * Sans ecriture Lua dans g_ps1ZoneHeapDiag.cmd, aucune commande n'est traitee :
 * le jeu normal garde ses blobs .incbin et son chemin d'execution identique.
 * Sur commande, un blob P1ZM est lu depuis le CD dans l'arene malloc, puis
 * libere avant toute allocation suivante. GetHeapUsage fournit les valeurs
 * brutes (alloc, alloc_max) : aucune estimation, aucun piege de taille figee.
 */
#include "zone_heap_ps1.h"

#include <psxcd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "audio_ps1.h"
#include "zone011.h"

extern "C" char _end[];

namespace {

constexpr uint32_t kRamBase = 0x80000000u;
// Les tables VRAM (64 Kio) et SPU (8 Kio) decalent _end de 9 156 octets.
// 0x38000 laisse 4 088 octets sous stackPaintLo mesure sur cette ISO
// (qa/ps1_logs/harness_v0822/diag_probe/diag_capture.txt) et couvre encore le
// pic de zone capture-only de 225 280 octets avec 4 096 octets de marge.
constexpr uint32_t kHeapArenaBytes = 0x38000u;      // 229 376 octets
constexpr uint32_t kMaxBufferBytes = kHeapArenaBytes - 0x1000u;

uint8_t* s_zoneBuffer = 0;

const char* zonePath(uint32_t zone) {
	switch (zone) {
	case 11u: return "\\ZONES\\ZONE011.DAT";
	case 13u: return "\\ZONES\\ZONE013.DAT";
	case 26u: return "\\ZONES\\ZONE026.DAT";
	case 42u: return "\\ZONES\\ZONE042.DAT";
	default:  return 0;
	}
}

int signalError(uint32_t code) {
	(void)code;  // code retenu dans errors pour l'audit du harnais
	++g_ps1ZoneHeapDiag.errors;
	zoneHeapDiagCompleteStep(g_ps1ZoneHeapDiag);
	return 0;
}

}  // namespace

PS1ZoneHeapDiag g_ps1ZoneHeapDiag;

static int zoneCdFail(uint32_t zone, int code) {
	printf("PS1_ZONE_CD FAIL zone=%u err=%d\n", static_cast<unsigned>(zone), code);
	return 0;
}

int zoneLoadFromCdPS1(uint32_t zone) {
	const char* path = zonePath(zone);
	if (path == 0) return zoneCdFail(zone, 1);
	audioStreamStopPS1();
	CdlFILE file{};
	if (CdSearchFile(&file, path) == 0) return zoneCdFail(zone, 2);
	const uint32_t bytes = static_cast<uint32_t>(file.size);
	const uint32_t bufferBytes = zoneHeapBufferBytes(bytes);
	if (bufferBytes == 0u || bufferBytes > kMaxBufferBytes) return zoneCdFail(zone, 3);
	// Un seul blob CD vit a la fois : la zone precedente n'est plus dessinee
	// (les zones embarquees ne pointent jamais dans ce tampon).
	if (s_zoneBuffer != 0) {
		free(s_zoneBuffer);
		s_zoneBuffer = 0;
	}
	uint8_t* buffer = static_cast<uint8_t*>(malloc(bufferBytes));
	if (buffer == 0) return zoneCdFail(zone, 4);
	CdControl(CdlSetloc, &file.pos, 0);
	CdReadCallback(0);
	CdRead(static_cast<int>(zoneHeapSectorsForBytes(bytes)),
	       reinterpret_cast<uint32_t*>(buffer), CdlModeSpeed);
	if (CdReadSync(0, 0) != 0) {
		free(buffer);
		return zoneCdFail(zone, 5);
	}
	if (buffer[0] != 'P' || buffer[1] != '1' || buffer[2] != 'Z' || buffer[3] != 'M') {
		free(buffer);
		return zoneCdFail(zone, 6);
	}
	loadZone(buffer);
	printf("PS1_ZONE_CD OK zone=%u bytes=%u\n", static_cast<unsigned>(zone),
	       static_cast<unsigned>(bytes));
	s_zoneBuffer = buffer;
	return 1;
}

void zoneHeapInitPS1(void) {
	const uintptr_t endAddr = reinterpret_cast<uintptr_t>(_end);
	InitHeap(reinterpret_cast<void*>(endAddr + 4u), kHeapArenaBytes);
	zoneHeapDiagInit(g_ps1ZoneHeapDiag,
	                 static_cast<uint32_t>(endAddr - kRamBase));
}

void zoneHeapSamplePS1(void) {
	PS1ZoneHeapDiag& d = g_ps1ZoneHeapDiag;
	if (d.cmd != kZoneHeapCmd || d.done != 0u) return;

	// Le flux VAGi utilise le meme lecteur CD : on l'arrete avant CHAQUE
	// commande capture-only. Le jeu peut redemarrer une musique entre deux
	// commandes ; ses recherches CD transitoires font sinon varier `alloc`
	// de 2048 octets entre le baseline et la mesure finale.
	audioStreamStopPS1();

	// Le pilote ISO du SDK ne garde qu'un enregistrement de repertoire en
	// cache : passer de \MUSIC a \ZONES libere 4096 octets et en alloue 2048.
	// Sans cet echauffement, baselineAlloc inclut la cache MUSIC alors que la
	// mesure finale inclut la cache ZONES : le test de fuite comparerait deux
	// etats de cache differents et signalerait une fuite inexistante.
	if (d.step == 0u) {
		CdlFILE warmup{};
		CdSearchFile(&warmup, "\\ZONES\\ZONE011.DAT");
	}

	const uint32_t zone = d.zone;
	if (zone == 0u) {
		if (s_zoneBuffer != 0) {
			free(s_zoneBuffer);
			s_zoneBuffer = 0;
		}
		zoneInit();  // restaure les pointeurs statiques et la VRAM de 011
		HeapUsage usage{};
		GetHeapUsage(&usage);
		zoneHeapDiagFinish(d, static_cast<uint32_t>(usage.alloc));
		return;
	}

	HeapUsage before{};
	GetHeapUsage(&before);
	if (s_zoneBuffer != 0) {
		free(s_zoneBuffer);  // liberation explicite AVANT l'allocation suivante
		s_zoneBuffer = 0;
	}
	HeapUsage afterFree{};
	GetHeapUsage(&afterFree);
	zoneHeapDiagBegin(d, zone, static_cast<uint32_t>(before.alloc));
	zoneHeapDiagRecordFree(d, static_cast<uint32_t>(afterFree.alloc));

	const char* path = zonePath(zone);
	if (path == 0) { signalError(1u); return; }

	CdlFILE file{};
	if (CdSearchFile(&file, path) == 0) { signalError(2u); return; }

	const uint32_t bytes = static_cast<uint32_t>(file.size);
	const uint32_t bufferBytes = zoneHeapBufferBytes(bytes);
	if (bufferBytes == 0u || bufferBytes > kMaxBufferBytes) {
		signalError(3u);
		return;
	}

	uint8_t* buffer = static_cast<uint8_t*>(malloc(bufferBytes));
	if (buffer == 0) { signalError(4u); return; }

	CdControl(CdlSetloc, &file.pos, 0);
	CdReadCallback(0);
	CdRead(static_cast<int>(zoneHeapSectorsForBytes(bytes)),
	       reinterpret_cast<uint32_t*>(buffer), CdlModeSpeed);
	if (CdReadSync(0, 0) != 0) {
		free(buffer);
		signalError(5u);
		return;
	}
	if (buffer[0] != 'P' || buffer[1] != '1' ||
	    buffer[2] != 'Z' || buffer[3] != 'M') {
		free(buffer);
		signalError(6u);
		return;
	}

	loadZone(buffer);  // meme parseur que le jeu, upload VRAM inclus
	s_zoneBuffer = buffer;

	HeapUsage after{};
	GetHeapUsage(&after);
	zoneHeapDiagRecordLoad(d, static_cast<uint32_t>(after.alloc),
	                       static_cast<uint32_t>(after.alloc_max), bytes);
	zoneHeapDiagCompleteStep(d);
}
