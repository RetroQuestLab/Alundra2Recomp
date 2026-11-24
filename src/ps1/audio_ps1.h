#pragma once

#include <stdint.h>

/* Pilote audio natif PS1 : banque VAG residante jouee par le SPU materiel.
 * Aucune allocation, aucune STL ; toutes les entrees sont des identifiants A2. */

#ifdef __cplusplus
extern "C" {
#endif

/* Etat observable par la sonde Lua (adresse lue dans le .map / nm). */
typedef struct PS1AudioDiag {
	uint32_t ready;
	uint32_t playCount;
	uint32_t uploadBytes;
	uint32_t verify;
	uint32_t lastId;
	uint32_t lastVoice;
} PS1AudioDiag;

extern PS1AudioDiag g_ps1AudioDiag;

/* Etat du flux CD observable par la sonde Lua (POD, aucune allocation). */
typedef struct PS1StreamDiag {
	uint32_t active;
	uint32_t theme;
	uint32_t fileFound;
	uint32_t fileSectors;
	uint32_t feedBytes;
	uint32_t chunks;
	uint32_t underruns;
	uint32_t lastError;
	uint32_t irqLen0;
	uint32_t irqLen1;
	uint32_t irqLen2;
	uint32_t irqLen3;
	uint32_t preStartLen;
	uint32_t feedCalls;
} PS1StreamDiag;

extern PS1StreamDiag g_ps1StreamDiag;

/* Etat du mixage (fondus et priorite SFX) observable par la sonde Lua.
 * activeTheme/pendingTheme valent 0xffffffff quand ils sont absents. */
typedef struct PS1MixDiag {
	uint32_t fadeState;
	uint32_t fadeLevel;
	uint32_t activeTheme;
	uint32_t pendingTheme;
	uint32_t fades;
	uint32_t fadesIn;
	uint32_t duck;
	uint32_t hold;
	uint32_t sfxActive;
	uint32_t sfxStarts;
	uint32_t sfxSteals;
	uint32_t sfxPreserved;
	uint32_t musicVolume;
	uint32_t reserved;
} PS1MixDiag;

extern PS1MixDiag g_ps1MixDiag;

/* Une seule fois au demarrage : SpuInit + televersement de la banque. */
void audioInitPS1(void);

/* Appel chaque image (reserve aux futures court-circuits de streaming). */
void audioPumpPS1(void);

/* id = identifiant A2 present dans data/sfx.txt. */
void audioPlaySfxIdPS1(int id);

/* theme = numero de piste A2 (une seule piste residante pour l'instant). */
void audioPlayMusicPS1(int theme);

/* Coupe toutes les voix (retour titre, fin de partie). */
void audioStopPS1(void);

int audioReadyPS1(void);
int audioPlayedCountPS1(void);

/* Streaming musical depuis les fichiers VAGi masterises sur le CD. */
void audioStreamMusicPS1(int theme);
void audioStreamPumpPS1(void);
void audioStreamStopPS1(void);

/* CdInit() une seule fois, partage avec le chargeur de zones CD. */
void audioCdInitOncePS1(void);

#ifdef __cplusplus
}
#endif
