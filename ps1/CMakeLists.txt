# Cible PlayStation 1 native — SEPAREE du build PC.
# Ne pas modifier le CMakeLists.txt racine ni build.ps1.
cmake_minimum_required(VERSION 3.21)

project(
	Alundra2PS1
	LANGUAGES    C CXX ASM
	VERSION      1.0.0
	DESCRIPTION  "Alundra 2 remake - native PS1 build"
)

set(PS1_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../src")

psn00bsdk_add_executable(alundra2 GPREL
	${PS1_SRC}/ps1/pf_ps1.cpp
	${PS1_SRC}/ps1/audio_ps1.cpp
	${PS1_SRC}/ps1/cd_stream.cpp
	${PS1_SRC}/ps1/diag_ps1.cpp
	${PS1_SRC}/ps1/zone_heap_ps1.cpp
	${PS1_SRC}/ps1/vram_diag_ps1.cpp
	${PS1_SRC}/ps1/spu_diag_ps1.cpp
	${PS1_SRC}/ps1/zone011.cpp
	${PS1_SRC}/ps1/hero_ps1.cpp
	${PS1_SRC}/ps1/hud_ps1.cpp
	${PS1_SRC}/ps1/interaction_ps1.cpp
	${PS1_SRC}/ps1/dialogue_ps1.cpp
	${PS1_SRC}/ps1/menu_ps1.cpp
	${PS1_SRC}/ps1/menu_inventory_ps1.cpp
	${PS1_SRC}/ps1/save_ps1.cpp
	${PS1_SRC}/ps1/intro_ps1.cpp
	${PS1_SRC}/ps1/boot.cpp
)

target_include_directories(alundra2 PRIVATE ${PS1_SRC})

# Blobs generes par tools/ps1_export_zone.py (non commits, voir build_ps1.ps1).
# La banque VAG doit etre alignee 64 octets pour le transfert DMA SPU.
psn00bsdk_target_incbin_a(alundra2 PRIVATE audio_vag audio_vag_size
	${CMAKE_CURRENT_SOURCE_DIR}/assets/audio.vag .data 64)
psn00bsdk_target_incbin(alundra2 PRIVATE hero_dat ${CMAKE_CURRENT_SOURCE_DIR}/assets/hero.dat)
psn00bsdk_target_incbin(alundra2 PRIVATE npc_dat ${CMAKE_CURRENT_SOURCE_DIR}/assets/npc.dat)
psn00bsdk_target_incbin(alundra2 PRIVATE actor_dat ${CMAKE_CURRENT_SOURCE_DIR}/assets/actor.dat)
psn00bsdk_target_incbin(alundra2 PRIVATE enemy_anim_dat ${CMAKE_CURRENT_SOURCE_DIR}/assets/enemy_anim.dat)
psn00bsdk_target_incbin(alundra2 PRIVATE hud_dat ${CMAKE_CURRENT_SOURCE_DIR}/assets/hud.dat)
psn00bsdk_target_incbin(alundra2 PRIVATE hud2_dat ${CMAKE_CURRENT_SOURCE_DIR}/assets/hud2.dat)
psn00bsdk_target_incbin(alundra2 PRIVATE portrait_dat ${CMAKE_CURRENT_SOURCE_DIR}/assets/portrait.dat)
psn00bsdk_target_incbin(alundra2 PRIVATE icons_dat ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons.dat)

# Fichiers VAGi du streaming musical : sans DEPENDS, une VAG modifiee ne
# regrettere pas l'ISO (psn00bsdk_add_cd_image configure juste le XML).
set(MUSIC_VAG_DEPENDS "")
foreach(theme RANGE 0 39)
	if(theme LESS 10)
		set(vag_name "0${theme}.VAG")
	else()
		set(vag_name "${theme}.VAG")
	endif()
	list(APPEND MUSIC_VAG_DEPENDS
		"${CMAKE_CURRENT_SOURCE_DIR}/assets/music/${vag_name}")
endforeach()

psn00bsdk_add_cd_image(
	alundra2_image  # Target CMake
	alundra2_ps1    # resultant .bin / .cue
	iso.xml
	DEPENDS alundra2 system.cnf ${MUSIC_VAG_DEPENDS}
		${CMAKE_CURRENT_SOURCE_DIR}/assets/zone011.dat
		${CMAKE_CURRENT_SOURCE_DIR}/assets/zone013.dat
		${CMAKE_CURRENT_SOURCE_DIR}/assets/zone026.dat
)
