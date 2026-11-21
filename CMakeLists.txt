cmake_minimum_required(VERSION 3.20)
project(Alundra2Remake CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SDL3 telecharge dans external/ (paquet de developpement MinGW)
set(SDL3_DIR "${CMAKE_SOURCE_DIR}/external/SDL3-3.4.20/x86_64-w64-mingw32/lib/cmake/SDL3")
find_package(SDL3 REQUIRED CONFIG)

# src/menu.cpp est cree par la tache 11 (narration-ui). CONFIGURE_DEPENDS rattache
# le fichier a la cible des son apparition, sans casser la build avant sa creation.
file(GLOB MENU_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/menu.cpp")

add_executable(alundra2 WIN32
    src/main.cpp
    src/map.cpp
    src/sprite.cpp
    src/warps.cpp
    src/world.cpp
    src/entities.cpp
    src/dialogue.cpp
    src/fx.cpp
    src/audio.cpp
    src/title.cpp
    src/items.cpp
    ${MENU_SOURCES}
)
target_link_libraries(alundra2 PRIVATE SDL3::SDL3)
target_compile_options(alundra2 PRIVATE -Wall -Wextra)
# Bibliotheque C++ integree a l'exe (pas de libc++.dll a fournir)
target_link_options(alundra2 PRIVATE -static)

# Copie SDL3.dll a cote de l'executable
add_custom_command(TARGET alundra2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3-shared> $<TARGET_FILE_DIR:alundra2>)

# --- Tests deterministes (ctest --test-dir build --output-on-failure) -------------
enable_testing()
find_program(PYTHON_EXECUTABLE NAMES python3 py python REQUIRED)
add_test(NAME validate_data COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/validate_data.py)
set_tests_properties(validate_data PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
# Verrouille le decodage A1 : ordre des nibble, tuiles 16x16 et plan 4bpp de WIND.TX.
add_test(NAME validate_a1_textures COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/check_a1_textures.py)
set_tests_properties(validate_a1_textures PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
# Verrouille le cadrage 48x56 du portrait de dialogue observe dans la reference A1.
# Pagination A1 (3 lignes max, aucun texte perdu) sur toutes les pages.
add_test(NAME validate_dialogue_pagination COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/check_dialogue_pagination.py)
# Sauvegarde reelle atomique (--savefile) : fichier complet, pas de .tmp residuel, secours .bak.
add_test(NAME validate_save_file COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/check_save_file.py $<TARGET_FILE:alundra2>)
add_test(NAME validate_dialogue_layout COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/check_dialogue_layout.py $<TARGET_FILE:alundra2> ${CMAKE_BINARY_DIR}/dialogue_layout.bmp)
set_tests_properties(validate_dialogue_layout PROPERTIES TIMEOUT 30 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

function(add_auto_test name script)
    add_test(NAME ${name} COMMAND $<TARGET_FILE:alundra2> --auto ${script} ${ARGN})
    set_tests_properties(${name} PROPERTIES TIMEOUT 30 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endfunction()

# Test de porte : on part sur le passage, on ferme le dialogue d'arrivee, on sort et on revient.
# ${ARGN} contient les options d'etat (--allitems, --supreme...) ; sans elles la porte doit rester fermee.
function(add_gate_test name dialogs zone x y expect)
    set(script "${dialogs}U24 D24 W10")
    add_auto_test(${name} "${script}" ${ARGN} --zone ${zone} --at ${x} ${y} --expect zone=${expect})
endfunction()

add_auto_test(auto_paco "T1 W60 T1 W3 R20 T1 W20" --zone 011)
# Tranche 1 : verrouillage deterministe du deplacement, saut, attaque et interaction dans le premier village.
add_auto_test(auto_zone011_move "R20 L20 D10 U10 W10" --zone 011 --at 26 47 --expect zone=011)
add_auto_test(auto_zone011_jump "J1 W40 T1 W10" --zone 011 --at 26 47 --expect zone=011)
add_auto_test(auto_zone011_attack "A8 W20 T1 W10" --zone 011 --at 26 47 --expect zone=011)
add_auto_test(auto_kindra "T1 W60 T1 W3 R20 T1 W20" --zone 013)
add_auto_test(auto_epave "T1 W60 T1 W3 R20 T1 W20" --zone 019)
add_auto_test(auto_toroledo "T1 W60 T1 W3 R20 T1 W20" --zone 119 --at 22 12)
add_auto_test(auto_gantelet "Q1 W2 A20 W30" --zone 019 --at 11 15)
add_auto_test(auto_supreme "Q1 W2 A20 W30" --supreme --zone 119 --at 24 12)
add_auto_test(auto_vallee "T1 W60 T1 W3 W20" --allitems --zone 026 --at 25 22)
add_auto_test(auto_eden "T1 W60 T1 W3 W20" --allitems --zone 042 --at 20 51)
add_auto_test(auto_puerto "T1 W60 T1 W3 W20" --allitems --zone 050 --at 8 27)
add_auto_test(auto_manoir "T1 W60 T1 W3 W20" --allitems --zone 086 --at 7 30)
add_auto_test(auto_trone "T1 W60 T1 W3 W20" --allitems --zone 120 --at 13 52)
# Victoire reelle : le heros bat Meghisto a coups d'epee supreme. --godmode evite seulement
# le hasard des degats ; --expect ending exige l'evenement mephisto_vaincu, pas un --ending force.
# Le W2 entre deux T est indispensable : sans tick de relachement, un appui ne compte qu'une fois.
# Le dialogue d'arrivee est ferme par X (nombre de panneaux variable, revue QA du 2026-11-21).
set(BOSS_VICTORY_SCRIPT "X600 L2 W2 ")
foreach(i RANGE 1 60)
    string(APPEND BOSS_VICTORY_SCRIPT "A1 W18 ")
endforeach()
add_auto_test(auto_boss_victory "${BOSS_VICTORY_SCRIPT}" --godmode --allitems --zone 120 --at 21 26 --expect ending)
# Portes conditionnees de la route A -> Z. Le dialogue d'arrivee est ferme par X (valider tant
# qu'il est ouvert) : depuis la pagination A1 a deux lignes par panneau (2026-11-21), le nombre
# d'appuis depend de la longueur du texte, plus du nombre de pages des donnees.
set(DLG_CLOSE "X600 ")
set(DLG_TORO "${DLG_CLOSE}")
set(DLG_VALLEE "${DLG_CLOSE}")
set(DLG_EDEN "${DLG_CLOSE}")
set(DLG_MEDUSA "${DLG_CLOSE}")
set(DLG_MANOIR "${DLG_CLOSE}")

add_gate_test(auto_gate_epee_blocked "${DLG_TORO}" 119 26 44 119)
add_gate_test(auto_gate_epee_open "${DLG_TORO}" 119 26 44 026 --supreme)
# Devant la lampe, le terrain au nord est un mur : on sort vers le sud puis on revient.
add_auto_test(auto_gate_lampe_blocked "${DLG_VALLEE}D12 U12 W10" --zone 026 --at 26 22 --expect zone=026)
add_auto_test(auto_gate_lampe_open "${DLG_VALLEE}D12 U12 W10" --allitems --zone 026 --at 26 22 --expect zone=042)
# Sur la carte 028, le passage d'Eden est bordé au nord et par une marche de 2 au sud :
# on descend d'une case puis on remonte pour franchir la porte (U24/D24 sortiraient de la case).
set(SCRIPT_PRISME "${DLG_EDEN}D14 U14 W10")
add_auto_test(auto_gate_prisme_blocked "${SCRIPT_PRISME}" --zone 042 --at 26 13 --expect zone=042)
add_auto_test(auto_gate_prisme_open "${SCRIPT_PRISME}" --allitems --zone 042 --at 26 13 --expect zone=050)
# Le quai de Puerto Medusa est borde au sud : la aussi on reste sur une courte oscillation.
add_auto_test(auto_gate_medusa_blocked "${DLG_MEDUSA}U8 D8 W10" --zone 050 --at 44 28 --expect zone=050)
add_auto_test(auto_gate_medusa_open "${DLG_MEDUSA}U8 D8 W10" --allitems --zone 050 --at 44 28 --expect zone=086)
add_gate_test(auto_gate_cristal_blocked "${DLG_MANOIR}" 086 27 8 086)
add_gate_test(auto_gate_cristal_open "${DLG_MANOIR}" 086 27 8 120 --allitems)
add_auto_test(auto_ending "W100" --ending --zone 120 --at 13 52)
add_auto_test(auto_savetest "W1" --savetest --allitems --zone 026 --at 25 22)
add_auto_test(auto_save_ending "W1" --savetest --ending --zone 120 --at 13 52)
# Dialogues de la trame A2 : on ferme le bandeau d'arrivee, on se tourne vers le PNJ, on lui parle.
# --expect dialogue= compare l'identifiant reellement ouvert, pas une simple presence de boite.
function(add_dialogue_test name zone x y dialogue)
    add_auto_test(${name} "${DLG_MEDUSA}R1 W2 T2 W2" ${ARGN}
                  --zone ${zone} --at ${x} ${y} --expect dialogue=${dialogue})
endfunction()
add_dialogue_test(auto_zeppo 050 46 26 zeppo_puerto)
add_dialogue_test(auto_zeppo_cristal 050 46 26 zeppo_puerto_objet_cristal --allitems)
add_dialogue_test(auto_georges 050 14 27 georges_puerto)
add_dialogue_test(auto_berger 119 24 34 berger_toroledo)
add_dialogue_test(auto_berger_ordre 119 24 34 berger_toroledo_objet_ordre --allitems)
# Zone 011 : pas de dialogue d'arrivee a fermer (contrairement aux zones de boss), donc pas de DLG_MEDUSA.
# Le heros part de (30,47) face au PNJ prunewell en (31,47) : on avance d'un pas puis on parle.
add_auto_test(auto_prunewell "R1 W2 T2 W2" --zone 011 --at 30 47 --expect dialogue=prunewell)

# Tranche 2 : menu A1 / contenu A2. Le menu s'ouvre avec M, se ferme avec M,
# A valide la selection sans jamais declencher d'attaque.
add_auto_test(auto_menu_equip "M1 R1 A1 M1 W5" --give bronze_sword:1 --zone 011 --at 26 47
              --expect menu=closed,weapon=bronze_sword,swordPower=2,noattack=1)
add_auto_test(auto_menu_empty "M1 D1 W5" --zone 011 --at 26 47 --expect menu=open,page=items)
add_auto_test(auto_menu_consumable "M1 D1 A1 W5" --give lorien_nuts:1 --zone 011 --at 26 47
              --expect menu=open,page=items,ep=200,qty:lorien_nuts=0,noattack=1)
add_auto_test(auto_menu_key "M1 D1 A1 W5" --give strange_key:1 --zone 011 --at 26 47
              --expect menu=open,page=items,qty:strange_key=1,noattack=1)
add_auto_test(auto_menu_start "M1 A1 M1 W5" --zone 011 --at 26 47
              --expect weapon=long_sword,swordPower=1,qty:long_sword=1,noattack=1)
add_auto_test(auto_menu_savetest "W1" --savetest --give bronze_sword:1 --zone 011 --at 26 47
              --expect qty:bronze_sword=1,qty:long_sword=1)
# Ecran 2 (equipement A1) : anneaux dans les emplacements en losange (7, cases vides comprises),
# puis objets de poche ; la validation equipe sans consommer et sans attaquer.
add_auto_test(auto_menu_ring "M1 U1 W3 A1 W3" --give pixie_ring:1,compass:1 --zone 011 --at 26 47
              --expect menu=open,page=equipment,ring=pixie_ring,pocket=none,qty:pixie_ring=1,noattack=1)
add_auto_test(auto_menu_pocket "M1 U1 W3 R1 W2 R1 W2 R1 W2 R1 W2 R1 W2 R1 W2 R1 W2 A1 W3" --give pixie_ring:1,compass:1
              --zone 011 --at 26 47 --expect page=equipment,pocket=compass,ring=none,qty:compass=1,noattack=1)

# Tranche 3 : armes d'exploration, forge et enigme (ajouts du remake, absents d'Alundra 2).
# Graine aleatoire fixe en mode --auto : les monstres bougent de facon reproductible.
set(RM_EQUIP_FIRST "M1 R1 A1 M1 W3 ")          # 2e arme de la liste (apres l'Epee longue)
set(RM_EQUIP_SECOND "M1 R1 W2 R1 A1 M1 W3 ")   # 3e arme de la liste
set(RM_HITS "U1 A2 W16 A2 W16 A2 W16 A2 W16 A2 W16 A2 W16")
add_auto_test(auto_rm_data "W1" --give hammer:1,rm_rift_hammer:1,rm_ember_shard:2 --zone 011 --at 26 47
              --expect qty:hammer=1,qty:rm_rift_hammer=1,qty:rm_ember_shard=2,rmweapon=rm_ember_blade,rmrecipe=6)
add_auto_test(auto_rm_equip_fire "${RM_EQUIP_FIRST}${RM_HITS}" --give rm_ember_blade:1 --zone 013 --at 6 53
              --expect weapon=rm_ember_blade,effect=fire,damage=3)
add_auto_test(auto_rm_equip_ice "${RM_EQUIP_FIRST}U1 A2 W14 A2 W14 A2 W14 A2 W14 A2 W14 A2 W14" --give rm_frost_lance:1
              --zone 013 --at 6 53 --expect weapon=rm_frost_lance,effect=ice,damage=2)
add_auto_test(auto_rm_equip_heavy "${RM_EQUIP_SECOND}U1 A2 W22 A2 W22 A2 W22 A2 W22 A2 W22"
              --give rm_ember_blade:1,rm_rift_hammer:1 --zone 013 --at 6 53 --expect weapon=rm_rift_hammer,effect=heavy,damage=5)
# Forge : deux appuis (preparer puis forger), transaction atomique, une branche par arme.
set(RM_FORGE "G1 W2 A1 W3 A1 W3")
add_auto_test(auto_rm_forge_ok "${RM_FORGE}" --give rm_ember_blade:1,rm_ember_shard:2,rm_ironwood:1 --zone 011 --at 26 47
              --expect forge=rm_ember_blade_flame_1,qty:rm_ember_shard=0,qty:rm_ironwood=0,noattack=1,page=forge)
add_auto_test(auto_rm_forge_missing "${RM_FORGE}" --give rm_ember_blade:1,rm_ember_shard:1 --zone 011 --at 26 47
              --expect forge=none,qty:rm_ember_shard=1,qty:rm_ember_blade=1)
add_auto_test(auto_rm_forge_cancel "G1 W2 A1 W3 B1 W3 G1 W2 A1 W3" --give rm_ember_blade:1,rm_ember_shard:2,rm_ironwood:1
              --zone 011 --at 26 47 --expect forge=none,qty:rm_ember_shard=2,qty:rm_ironwood=1)
add_auto_test(auto_rm_forge_double "G1 W2 A1 W3 A1 W3 A1 W3 A1 W3" --give rm_ember_blade:1,rm_ember_shard:2,rm_ironwood:1
              --zone 011 --at 26 47 --expect forge=rm_ember_blade_flame_1,qty:rm_ember_shard=0,upgrades=1)
add_auto_test(auto_rm_forge_branch "G1 W2 A1 W3 A1 W3 D1 W3 A1 W3 A1 W3"
              --give rm_ember_blade:1,rm_ember_shard:3,rm_ironwood:1,rm_rune_dust:1 --zone 011 --at 26 47
              --expect forge=rm_ember_blade_flame_1,upgrades=1,qty:rm_ember_shard=1,qty:rm_rune_dust=1)
add_auto_test(auto_rm_forge_noweapon "${RM_FORGE}" --give rm_ember_shard:2,rm_ironwood:1 --zone 011 --at 26 47
              --expect forge=none,qty:rm_ember_shard=2,qty:rm_ironwood=1)
# Orvan : premiere rencontre = Marteau de faille ; ensuite la forge s'ouvre apres sa replique.
add_auto_test(auto_rm_orvan "D1 W2 T2 W2 X600 W2 T2 W2 X600 W5" --zone 011 --at 36 46
              --expect menu=open,page=forge,qty:rm_rift_hammer=1,event:rm_forgeron=1,noattack=1)
# Enigme a solutions multiples (2 objets sur 3 ouvrent la cache), mauvais effet sans effet.
set(RM_TWO "${RM_EQUIP_SECOND}D1 W2 T2 W2 X600 R60 W2 D1 W2 T2 W2 X600")
# dialogue= en tete : prouve que l'interaction a bien eu lieu (revue QA : le test du mauvais
# effet passait aussi loin du pilier) ; une seule entree ne doit pas ouvrir la cache.
add_auto_test(auto_rm_puzzle_fire "${RM_EQUIP_FIRST}D1 W2 T2 W2" --give rm_ember_blade:1 --zone 011 --at 23 49
              --expect dialogue=rm_brasero_allume,interaction:rm_demo_brazier=1,puzzle=1,effect=fire,event:rm_demo_gate=0)
add_auto_test(auto_rm_puzzle_wrong "${RM_EQUIP_FIRST}D1 W2 T2 W2" --give rm_ember_blade:1 --zone 011 --at 27 49
              --expect dialogue=rm_interaction_sans_effet,interaction:rm_demo_pillar=0,puzzle=0)
add_auto_test(auto_rm_puzzle_ice "${RM_EQUIP_FIRST}D1 W2 T2 W2" --give rm_frost_lance:1 --zone 011 --at 25 49
              --expect dialogue=rm_chemin_gele,interaction:rm_demo_ice_path=1,puzzle=1,event:rm_demo_gate=0)
add_auto_test(auto_rm_puzzle_heavy_two "${RM_TWO}" --give rm_ember_blade:1,rm_rift_hammer:1 --zone 011 --at 23 49
              --expect interaction:rm_demo_brazier=1,interaction:rm_demo_pillar=1,puzzle=2,event:rm_demo_gate=1)
add_auto_test(auto_rm_puzzle_alt_two "${RM_EQUIP_SECOND}D1 W2 T2 W2 X600 R40 W2 D1 W2 T2 W2 X600"
              --give rm_ember_blade:1,rm_rift_hammer:1 --zone 011 --at 23 49
              --expect interaction:rm_demo_brazier=1,interaction:rm_demo_ice_path=1,puzzle=2,event:rm_demo_gate=1)
add_auto_test(auto_rm_puzzle_reset "D1 W2 T2 W2 X600" --loadsave ${CMAKE_SOURCE_DIR}/tests/fixtures/v4_puzzle.txt --zone 011
              --expect puzzle=0,event:rm_demo_gate=1,interaction:rm_demo_brazier=0,zone=011)
add_auto_test(auto_rm_cache "D1 W2 T2 W2 X600" --loadsave ${CMAKE_SOURCE_DIR}/tests/fixtures/v4_cache.txt --zone 011
              --expect qty:rm_ember_blade=1,zone=011)
add_auto_test(auto_rm_no_gate_change "${RM_TWO}" --give rm_ember_blade:1,rm_rift_hammer:1 --zone 011 --at 23 49
              --expect puzzle=2,event:epee_supreme=0,event:boss_medusa=0,qty:lampe=0,qty:prisme=0,qty:cristal=0,qty:epee_supreme=0,zone=011)
# Sauvegarde V4 : V3 migree sans perte, V4 relue et reecrite a l'identique, version future refusee.
add_auto_test(auto_rm_save_v3 "W1" --loadsave ${CMAKE_SOURCE_DIR}/tests/fixtures/v3_save.txt --zone 011
              --expect forge=none,qty:long_sword=1,interaction:rm_demo_brazier=0,weapon=long_sword)
add_auto_test(auto_rm_save_v4 "W1" --loadsave ${CMAKE_SOURCE_DIR}/tests/fixtures/v4_puzzle.txt --savetest --zone 011
              --expect forge=rm_rift_hammer_quake_1,interaction:rm_demo_pillar=1,event:rm_demo_gate=1,weapon=rm_rift_hammer,swordPower=6)
# Refus prouve par le message de version (un fichier absent ou un plantage ne suffit plus).
add_auto_test(auto_rm_save_future "W1" --loadsave ${CMAKE_SOURCE_DIR}/tests/fixtures/v5_future.txt --zone 011)
set_tests_properties(auto_rm_save_future PROPERTIES PASS_REGULAR_EXPRESSION "version 5 refusee")
# Revue QA : annulation par changement de recette, coup charge elementaire (EP depense en zone),
# degats forges au-dela de 6 (le chemin des epees A2 plafonne a 6 : 7 prouve le chemin du remake).
add_auto_test(auto_rm_forge_cancel_move "G1 W2 A1 W3 D1 W3 U1 W3 A1 W3"
              --give rm_ember_blade:1,rm_ember_shard:2,rm_ironwood:1 --zone 011 --at 26 47
              --expect forge=none,qty:rm_ember_shard=2,qty:rm_ironwood=1)
# Coup charge en zone sans monstre (Paco) : une gelee qui touche le heros interrompt la charge.
add_auto_test(auto_rm_charged "M1 D1 A1 W3 U1 W3 R1 W2 A1 M1 W3 U1 K70 W30" --give lorien_nuts:1,rm_ember_blade:1
              --zone 011 --at 26 47 --expect weapon=rm_ember_blade,ep=194,effect=fire)
add_auto_test(auto_rm_forged_quake "U1 A2 W24 A2 W24 A2 W24 A2 W24 A2 W24 A2 W24"
              --loadsave ${CMAKE_SOURCE_DIR}/tests/fixtures/v4_kindra.txt --zone 013
              --expect weapon=rm_rift_hammer,effect=heavy,damage=7,forge=rm_rift_hammer_quake_1)

# B02 : choix de dialogue (presentation A1 : parchemin de reponses + triangle de WIND.TX) et actions
# atomiques (prend/donne/evenement/sinon/si). Premier contenu : marchand de Kindra (A2, bloc 33).
set(KINDRA_QUESTION "D1 W2 T2 W100 T2 W2 W100 ")
add_auto_test(auto_choice_accept "D1 W2 T2 W2 X600" --give herbe:1 --zone 013 --at 10 56
              --expect dialogue=marchand_kindra_oui,qty:herbe=0,qty:compass=1,event:marchand_kindra_echange=1)
add_auto_test(auto_choice_refuse "${KINDRA_QUESTION}R1 W2 T2 W2 X600" --give herbe:1 --zone 013 --at 10 56
              --expect dialogue=marchand_kindra_non,choice=Refuser,qty:herbe=1,qty:compass=0)
add_auto_test(auto_choice_cancel "${KINDRA_QUESTION}B1 W2 X600" --give herbe:1 --zone 013 --at 10 56
              --expect dialogue=marchand_kindra_non,choice=Refuser,qty:herbe=1)
add_auto_test(auto_choice_missing "D1 W2 T2 W2 X600" --zone 013 --at 10 56
              --expect dialogue=marchand_kindra_sans_herbe,qty:compass=0,event:marchand_kindra_echange=0)
add_auto_test(auto_choice_after "D1 W2 T2 W2 X600 W5 T2 W2 X600" --give herbe:1 --zone 013 --at 10 56
              --expect dialogue=marchand_kindra_merci,qty:compass=1,qty:herbe=0)
# B06 : cle etrange de l'epave (debut de la chaine d'echange A2) et variante d'Alexia (« ! si »).
add_auto_test(auto_epave_cle "X600 D1 W2 T2 W200 X600" --zone 019 --at 7 15
              --expect qty:strange_key=1,event:objet_strange_key=1)
add_auto_test(auto_epave_cle_alexia "W2 T2 W2" --loadsave ${CMAKE_SOURCE_DIR}/tests/fixtures/v4_epave.txt --zone 019
              --expect dialogue=alexia_epave_cle)
add_auto_test(auto_epave_alexia_sans_cle "X600 W2 U1 W2 T2 W2" --zone 019 --at 10 12 --expect dialogue=alexia_epave)
# B05 : piece de puzzle dans la foret Kindra (A2, bloc 11), prepare les enigmes de Jeehan (B10).
add_auto_test(auto_kindra_puzzle "D1 W2 T2 W200 X600" --zone 013 --at 13 47 --expect qty:puzzle_piece=1,event:objet_puzzle_piece=1)
