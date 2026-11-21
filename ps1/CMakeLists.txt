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
	${PS1_SRC}/ps1/zone011.cpp
	${PS1_SRC}/ps1/hero_ps1.cpp
	${PS1_SRC}/ps1/boot.cpp
)

target_include_directories(alundra2 PRIVATE ${PS1_SRC})

# Blobs generes par tools/ps1_export_zone.py (non commits, voir build_ps1.ps1).
psn00bsdk_target_incbin(alundra2 PRIVATE zone011_bin ${CMAKE_CURRENT_SOURCE_DIR}/assets/zone011.dat)
psn00bsdk_target_incbin(alundra2 PRIVATE hero_dat ${CMAKE_CURRENT_SOURCE_DIR}/assets/hero.dat)

psn00bsdk_add_cd_image(
	alundra2_image  # Target CMake
	alundra2_ps1    # resultant .bin / .cue
	iso.xml
	DEPENDS alundra2 system.cnf
)
