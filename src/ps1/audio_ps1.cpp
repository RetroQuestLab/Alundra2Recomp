/*
 * Pilote SPU natif pour le-portage PS1.
 *
 * La banque VAG est generee par tools/ps1_export_audio.py et incluse dans
 * l'EXE via psn00bsdk_target_incbin. Le transfert DMA est fait une seule fois
 * au demarrage ; ensuite les voix sont pilotees par les registres materiels
 * (24 canaux, ADPCM materiel, pas de melangeur logiciel).
 */
#include "audio_ps1.h"

#include <psxspu.h>
#include <psxcd.h>
#include <stdint.h>

#include "../platform/pf.h"
#include "audio_data_ps1.h"
#include "cd_stream.h"
#include "mixer_ps1_logic.h"
#include "music_data_ps1.h"
#include "spu_diag_ps1.h"

extern const uint8_t audio_vag[];
extern const uint32_t audio_vag_size;

PS1AudioDiag g_ps1AudioDiag = {0u, 0u, 0u, 0u, 0u, 0u};
PS1StreamDiag g_ps1StreamDiag = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
PS1MixDiag g_ps1MixDiag = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

/* Demarre effectivement un flux CD (prefill synchrone). Defini plus bas,
 * appele par audioPumpPS1 quand un fondu atteint zero. */
void startStreamNow(int theme);

namespace {

PS1MixState s_mix;
uint32_t s_lastFadeIns = 0u;

// Les premiers 4 Ko de la RAM SPU sont reserves aux buffers de capture, et
// SpuInit televerse un echantillon muet a 0x1000 (voir vagsample/MAIN.C du SDK).
constexpr uint32_t kBankSpuAddr = 0x1010u;
constexpr int kVoiceCount = 24;
constexpr int kMusicVoice = kVoiceCount - 1;
constexpr uint16_t kPitch22050 = getSPUSampleRate(22050);
constexpr uint16_t kVolume = 0x3fffu;

int s_ready = 0;
int s_playCount = 0;

void appendUInt(char* line, int& pos, uint32_t value) {
	char digits[12];
	int count = 0;
	if (value == 0u) {
		digits[count++] = '0';
	}
	while (value != 0u) {
		digits[count++] = static_cast<char>('0' + (value % 10u));
		value /= 10u;
	}
	while (count > 0) line[pos++] = digits[--count];
}

void logEvent(const char* name, uint32_t a, uint32_t b) {
	char line[72];
	int pos = 0;
	while (*name != '\0' && pos < 58) line[pos++] = *name++;
	line[pos++] = ' ';
	line[pos++] = 'a';
	line[pos++] = '=';
	appendUInt(line, pos, a);
	line[pos++] = ' ';
	line[pos++] = 'b';
	line[pos++] = '=';
	appendUInt(line, pos, b);
	line[pos] = '\0';
	pfLog(line);
}

void playClip(uint32_t clipAddr, uint32_t clipSize, bool loop, int voice,
              uint32_t logId) {
	(void)clipSize;
	SpuSetKey(0, 1u << voice);
	SPU_CH_FREQ(voice) = kPitch22050;
	SPU_CH_ADDR(voice) = getSPUAddr(kBankSpuAddr + clipAddr);
	// Le bloc VAG 0 est un silence d'initialisation : la boucle part au bloc 1.
	SPU_CH_LOOP_ADDR(voice) =
	    getSPUAddr(kBankSpuAddr + clipAddr + (loop ? 16u : 0u));
	// ADSR desactive : volume constant, identique a l'exemple vagsample du SDK.
	SPU_CH_VOL_L(voice) = kVolume;
	SPU_CH_VOL_R(voice) = kVolume;
	SPU_CH_ADSR1(voice) = 0x00ffu;
	SPU_CH_ADSR2(voice) = 0x0000u;
	SpuSetKey(1, 1u << voice);
	++s_playCount;
	g_ps1AudioDiag.playCount = static_cast<uint32_t>(s_playCount);
	g_ps1AudioDiag.lastId = logId;
	g_ps1AudioDiag.lastVoice = static_cast<uint32_t>(voice);
	logEvent("PS1_SPU_PLAY", logId, static_cast<uint32_t>(voice));
}

}  // namespace

void audioInitPS1(void) {
	if (s_ready) return;

	ps1MixInit(&s_mix);
	SpuInit();
	SpuSetCommonMasterVolume(0x5000u, 0x5000u);
	for (int voice = 0; voice < kVoiceCount; ++voice) {
		SpuSetKey(0, 1u << voice);
		SPU_CH_VOL_L(voice) = 0u;
		SPU_CH_VOL_R(voice) = 0u;
	}

	uint32_t bytes = audio_vag_size;
	bytes = (bytes + 63u) & ~63u;
	SpuSetTransferMode(SPU_TRANSFER_BY_DMA);
	SpuSetTransferStartAddr(kBankSpuAddr);
	SpuWrite(reinterpret_cast<const uint32_t*>(audio_vag), bytes);
	spuDiagTrackRangePS1(kBankSpuAddr, bytes);  // apres SpuWrite, order preserved
	SpuIsTransferCompleted(SPU_TRANSFER_WAIT);

	// Reliture reelle de la RAM SPU : prouve que le transfert DMA a abouti,
	// pas seulement que le codage MIPS a ete appele.
	uint32_t readback[16] = {0u};
	SpuSetTransferMode(SPU_TRANSFER_BY_DMA);
	SpuSetTransferStartAddr(kBankSpuAddr);
	SpuRead(readback, sizeof readback);
	SpuIsTransferCompleted(SPU_TRANSFER_WAIT);
	const uint32_t* source = reinterpret_cast<const uint32_t*>(audio_vag);
	uint32_t verify = 1u;
	for (int i = 0; i < 16; ++i) {
		if (readback[i] != source[i]) verify = 0u;
	}

	s_ready = 1;
	g_ps1AudioDiag.ready = 1u;
	g_ps1AudioDiag.uploadBytes = bytes;
	g_ps1AudioDiag.verify = verify;
	logEvent("PS1_SPU_INIT", static_cast<uint32_t>(kVoiceCount), 22050u);
	logEvent("PS1_SPU_UPLOAD", bytes, kBankSpuAddr);
	logEvent("PS1_SPU_VERIFY", verify, 0u);
}

void audioPumpPS1(void) {
	PS1MixAction mix;
	ps1MixStep(&s_mix, &mix);

	// Un clip termine est remis sur le bloc muet avec la cle SPU active :
	// c'est l'etat "voix neutre" exige par le pilote de streaming pour eviter
	// les IRQ SPU parasites. La priorite SFX est ainsi rendue sans coupure.
	if (mix.expired != 0u) {
		for (int voice = 0; voice < PS1_MIX_SFX_VOICES; ++voice) {
			if ((mix.expired & (1u << voice)) == 0u) continue;
			SpuSetKey(0, 1u << voice);
			SPU_CH_VOL_L(voice) = 0u;
			SPU_CH_VOL_R(voice) = 0u;
			SPU_CH_ADDR(voice) = getSPUAddr(0x1000u);
			SpuSetKey(1, 1u << voice);
		}
	}

	// Volume de la voix musique : fondu PC (0,5 s) x ducking sous les SFX.
	SPU_CH_VOL_L(kMusicVoice) = mix.volume;
	SPU_CH_VOL_R(kMusicVoice) = mix.volume;

	if (mix.action == PS1_MIX_ACT_START) {
		logEvent("PS1_MIX_SWAP", static_cast<uint32_t>(mix.theme),
		         s_mix.fades);
		startStreamNow(mix.theme);
	}

	// La musique residante boucle en materiel ; le flux CD est alimente ici.
	audioStreamPumpPS1();

	// Diagnostics POD : aucun formatage, aucune allocation dans la boucle.
	int activeSfx = 0;
	for (int voice = 0; voice < PS1_MIX_SFX_VOICES; ++voice)
		if (s_mix.sfxFrames[voice] != 0) ++activeSfx;
	g_ps1MixDiag.fadeState = static_cast<uint32_t>(s_mix.fade);
	g_ps1MixDiag.fadeLevel = static_cast<uint32_t>(s_mix.level);
	g_ps1MixDiag.activeTheme = s_mix.activeTheme < 0
	                               ? 0xffffffffu
	                               : static_cast<uint32_t>(s_mix.activeTheme);
	g_ps1MixDiag.pendingTheme = s_mix.pendingTheme < 0
	                                ? 0xffffffffu
	                                : static_cast<uint32_t>(s_mix.pendingTheme);
	g_ps1MixDiag.fades = s_mix.fades;
	g_ps1MixDiag.fadesIn = s_mix.fadesIn;
	g_ps1MixDiag.duck = static_cast<uint32_t>(s_mix.duck);
	g_ps1MixDiag.hold = static_cast<uint32_t>(s_mix.hold);
	g_ps1MixDiag.sfxActive = static_cast<uint32_t>(activeSfx);
	g_ps1MixDiag.sfxStarts = s_mix.sfxStarts;
	g_ps1MixDiag.sfxSteals = s_mix.sfxSteals;
	g_ps1MixDiag.sfxPreserved = s_mix.sfxPreserved;
	g_ps1MixDiag.musicVolume = mix.volume;

	if (s_mix.fadesIn != s_lastFadeIns) {
		s_lastFadeIns = s_mix.fadesIn;
		logEvent("PS1_MIX_FADEIN", s_lastFadeIns, s_mix.level);
	}
}

void audioPlaySfxIdPS1(int id) {
	if (!s_ready) return;
	for (int i = 0; i < PS1_AUDIO_SFX_COUNT; ++i) {
		if (g_ps1AudioSfx[i].id != static_cast<uint16_t>(id)) continue;
		const int frames = ps1MixSfxFrames(g_ps1AudioSfx[i].size);
		const int voice = ps1MixAllocSfx(&s_mix, frames);
		if (voice < 0) return;
		playClip(g_ps1AudioSfx[i].addr, g_ps1AudioSfx[i].size, false, voice,
		         static_cast<uint32_t>(id));
		return;
	}
}

void audioPlayMusicPS1(int theme) {
	if (!s_ready) return;
	playClip(PS1_AUDIO_MUSIC_ADDR, PS1_AUDIO_MUSIC_SIZE, true, kMusicVoice,
	         static_cast<uint32_t>(theme));
}

void audioStopPS1(void) {
	for (int voice = 0; voice < kVoiceCount; ++voice) {
		SpuSetKey(0, 1u << voice);
	}
}

int audioReadyPS1(void) { return s_ready; }

int audioPlayedCountPS1(void) { return s_playCount; }

/* ------------------------------------------------------------------------- */
/* Streaming musical : fichiers VAGi masterises sous \MUSIC\ sur le CD.       */
/* La logique IRQ/DMA/FIFO vient de cd_stream.{h,cpp} (PSn00bSDK, MPL-2.0).    */
/* ------------------------------------------------------------------------- */

namespace {

constexpr uint32_t kStreamSpuAddr = 0x5D600u;  // apres la banque residante
constexpr uint32_t kInterleave = 8192u;        // 512 blocs PS-ADPCM / chunk
constexpr size_t kRingBytes = 0x18000u;
constexpr int kRefillSectors = 24;
constexpr uint16_t kSectorBytes = 2048u;

struct VagFileHeader {
	uint32_t magic;
	uint32_t version;
	uint32_t interleave;
	uint32_t size_be;
	uint32_t rate_be;
	uint16_t reserved[5];
	uint16_t channels;
	char name[16];
};

uint32_t swap32(uint32_t v) {
	return ((v & 0xffu) << 24) | ((v & 0xff00u) << 8) |
	       ((v >> 8) & 0xff00u) | ((v >> 24) & 0xffu);
}

struct ReadState {
	int startLba;
	int streamLength;
	int nextSector;
	size_t refillLength;
};

Stream_Context s_stream;
ReadState s_read = {0, 0, 0, 0u};
bool s_streamReady = false;
bool s_cdReady = false;
bool s_voicesNeutralized = false;

// Le SDK cdstream pointe les voix inactives vers l'echantillon muet televerse
// par SpuInit a 0x1000 : sinon une voix arretee peut lire de la RAM SPU non
// initialisee et declencher une IRQ SPU parasite qui desynchronise le flux.
void neutralizeInactiveVoices() {
	if (s_voicesNeutralized) return;

	// Priorite SFX : les voix qui jouent encore un bruitage ne sont pas
	// touchees (ni key off, ni changement d'adresse) lors du demarrage du flux.
	const uint32_t mask = ps1MixNeutralizeMask(&s_mix);
	SpuSetKey(0, mask);
	for (int ch = 0; ch < PS1_MIX_SFX_VOICES; ++ch) {
		if ((mask & (1u << ch)) == 0u) continue;
		SPU_CH_ADDR(ch) = getSPUAddr(0x1000u);
		SPU_CH_FREQ(ch) = 0x1000u;
	}
	SpuSetKey(1, mask);
	s_voicesNeutralized = true;
}

void cdReadDone(CdlIntrResult event, uint8_t* /*payload*/) {
	if (event == CdlDiskError) {
		g_ps1StreamDiag.lastError = 3u;
		return;
	}
	Stream_Feed(&s_stream, s_read.refillLength * kSectorBytes);
}

bool feedStream() {
	if (CdReadSync(1, 0) > 0) return true;
	if (Stream_GetRefillLength(&s_stream) <
	    static_cast<size_t>(kRefillSectors) * kSectorBytes)
		return false;

	uint8_t* ptr = 0;
	size_t refill = Stream_GetFeedPtr(&s_stream, &ptr) / kSectorBytes;
	if (refill == 0u) return false;

	int next = s_read.nextSector;
	int remaining = s_read.streamLength - next;
	while (remaining <= 0) {
		next -= s_read.streamLength;
		remaining += s_read.streamLength;
	}
	if (refill > static_cast<size_t>(remaining))
		refill = static_cast<size_t>(remaining);

	CdlLOC pos;
	CdIntToPos(s_read.startLba + next, &pos);
	CdControl(CdlSetloc, &pos, 0);
	CdReadCallback(&cdReadDone);
	CdRead(static_cast<int>(refill), reinterpret_cast<uint32_t*>(ptr),
	       CdlModeSpeed);

	s_read.nextSector = next + static_cast<int>(refill);
	s_read.refillLength = refill;
	return true;
}

/* Prefill synchrone. Sous emulateur, CdReadSync(1) ne signale pas toujours
 * l'occupation du lecteur des que CdRead() vient d'etre lance : une boucle de
 * lectures asynchrones peut alors ecraser la lecture precedente avant que son
 * callback ne soit appele, et le ring reste vide au Stream_Start (underruns).
 * On lit donc une tranche a la fois en attente bloquante, puis on alimente le
 * ring explicitement. Le regime continu reste asynchrone via feedStream(). */
void prefillStream() {
	while (Stream_GetRefillLength(&s_stream) >=
	       static_cast<size_t>(kRefillSectors) * kSectorBytes) {
		uint8_t* ptr = 0;
		size_t refill = Stream_GetFeedPtr(&s_stream, &ptr) / kSectorBytes;
		if (refill == 0u) break;

		int next = s_read.nextSector;
		int remaining = s_read.streamLength - next;
		while (remaining <= 0) {
			next -= s_read.streamLength;
			remaining += s_read.streamLength;
		}
		if (refill > static_cast<size_t>(remaining))
			refill = static_cast<size_t>(remaining);
		if (refill == 0u) break;

		CdlLOC pos;
		CdIntToPos(s_read.startLba + next, &pos);
		CdControl(CdlSetloc, &pos, 0);
		CdReadCallback(0);
		CdRead(static_cast<int>(refill), reinterpret_cast<uint32_t*>(ptr),
		       CdlModeSpeed);
		CdReadSync(0, 0);
		Stream_Feed(&s_stream, refill * kSectorBytes);
		s_read.nextSector = next + static_cast<int>(refill);
	}
}

}  // namespace

void audioStreamMusicPS1(int theme) {
	if (!s_ready || theme < 0 || theme >= PS1_MUSIC_COUNT) {
		g_ps1StreamDiag.lastError = 1u;
		return;
	}
	// Fondu PC : le flux reel ne change qu'a volume zero. Le premier theme
	// demarre tout de suite (aucun flux a interrompre) avec montee depuis 0.
	if (ps1MixRequestTheme(&s_mix, theme) == PS1_MIX_ACT_START)
		startStreamNow(theme);
}

void startStreamNow(int theme) {
	if (!s_cdReady) {
		CdInit();
		s_cdReady = true;
	}

	CdlFILE file;
	if (!CdSearchFile(&file, g_ps1MusicPaths[theme])) {
		g_ps1StreamDiag.lastError = 2u;
		return;
	}

	uint32_t sector[512];
	CdControl(CdlSetloc, &file.pos, 0);
	CdReadCallback(0);
	CdRead(1, sector, CdlModeSpeed);
	CdReadSync(0, 0);

	const VagFileHeader* vag = reinterpret_cast<const VagFileHeader*>(sector);
	if (vag->magic != 0x69474156u || vag->interleave != kInterleave ||
	    vag->channels != 1u) {
		g_ps1StreamDiag.lastError = 4u;
		return;
	}
	const int payload = static_cast<int>(swap32(vag->size_be));
	const int rate = static_cast<int>(swap32(vag->rate_be));

	if (s_streamReady) {
		Stream_Stop();
		Stream_Destroy(&s_stream);
		s_streamReady = false;
	}

	neutralizeInactiveVoices();

	Stream_Config config;
	__builtin_memset(&config, 0, sizeof config);
	config.spu_address = kStreamSpuAddr;
	config.channel_mask = 1u << kMusicVoice;
	config.interleave = vag->interleave;
	config.buffer_size = kRingBytes;
	config.refill_threshold =
	    static_cast<size_t>(kRefillSectors) * kSectorBytes;
	config.sample_rate = rate;
	SPU_CH_VOL_L(kMusicVoice) = kVolume;
	SPU_CH_VOL_R(kMusicVoice) = kVolume;

	Stream_Init(&s_stream, &config);
	s_streamReady = true;

	s_read.startLba = CdPosToInt(&file.pos) + 1;
	s_read.streamLength = (payload + static_cast<int>(kSectorBytes) - 1) /
	                      static_cast<int>(kSectorBytes);
	s_read.nextSector = 0;
	s_read.refillLength = 0u;

	prefillStream();
	g_cdStreamPreStartLen = static_cast<int>(s_stream.buffer.length);
	Stream_Start(&s_stream, false);

	g_ps1StreamDiag.active = 1u;
	g_ps1StreamDiag.theme = static_cast<uint32_t>(theme);
	g_ps1StreamDiag.fileFound = 1u;
	g_ps1StreamDiag.fileSectors = static_cast<uint32_t>(s_read.streamLength);
	g_ps1StreamDiag.lastError = 0u;
	logEvent("PS1_CD_STREAM", static_cast<uint32_t>(theme),
	         static_cast<uint32_t>(s_read.streamLength));
}

void audioStreamPumpPS1(void) {
	if (!s_streamReady) return;
	feedStream();
	g_ps1StreamDiag.feedBytes = g_cdStreamBytesFed;
	g_ps1StreamDiag.chunks = g_cdStreamChunks;
	g_ps1StreamDiag.underruns = g_cdStreamUnderruns;
	g_ps1StreamDiag.irqLen0 = static_cast<uint32_t>(g_cdStreamIrqLens[0]);
	g_ps1StreamDiag.irqLen1 = static_cast<uint32_t>(g_cdStreamIrqLens[1]);
	g_ps1StreamDiag.irqLen2 = static_cast<uint32_t>(g_cdStreamIrqLens[2]);
	g_ps1StreamDiag.irqLen3 = static_cast<uint32_t>(g_cdStreamIrqLens[3]);
	g_ps1StreamDiag.preStartLen = static_cast<uint32_t>(g_cdStreamPreStartLen);
	g_ps1StreamDiag.feedCalls = g_cdStreamFeedCalls;
}

void audioStreamStopPS1(void) {
	ps1MixCancel(&s_mix);  // aucun fondu ne doit redemarrer le flux apres l'arret
	if (!s_streamReady) return;
	// Une lecture asynchrone de feedStream() peut etre en vol : la laisser se
	// terminer (son callback alimente encore un flux valide), puis retirer le
	// callback avant de rendre le lecteur (lecture de zone 026, v0.8.26).
	// Precaution : aucun blocage n'a ete attribue a cette course.
	CdReadSync(0, 0);
	CdReadCallback(0);
	Stream_Stop();
	Stream_Destroy(&s_stream);
	s_streamReady = false;
	g_ps1StreamDiag.active = 0u;
}
