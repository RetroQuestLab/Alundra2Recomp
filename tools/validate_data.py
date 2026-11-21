# -*- coding: utf-8 -*-
"""Validation complete des donnees du remake (aucune ecriture).

Verifie :
  - presence des cartes, sprites, dialogues, pistes audio et bruitages ;
  - coherence des objets, evenements et passages (besoin optionnel) ;
  - accessibilite des points d'interet depuis l'entree de chaque zone.

Usage : py -3.12 tools/validate_data.py
"""
import pathlib
import re
import struct
import sys
from collections import deque

ROOT = pathlib.Path(__file__).resolve().parents[1]
FMT = '<BBBBHH'
CELL = struct.calcsize(FMT)
HEADER = struct.calcsize('<HHH') + 4
BLOCK = 0x01 | 0x10  # ATTR_SOLID | ATTR_EDGE : voir game.h

errors = []
warnings = []
map_for = {}         # zone du remake -> carte A1 utilisee (commande "map" de world.txt)


def bad(message):
    errors.append(message)


def warn(message):
    warnings.append(message)


def read_lines(name):
    return (ROOT / 'data' / name).read_text(encoding='utf-8').splitlines()


def clean(line):
    return line.split('#', 1)[0].strip()


def load_map(zone):
    name = map_for.get(zone, zone)
    path = ROOT / 'assets' / 'maps' / f'{name}.a1m'
    if not path.exists():
        bad(f'carte absente : {path.name} (zone {zone})')
        return None
    data = path.read_bytes()
    w, h, n = struct.unpack_from('<HHH', data, 4)
    off = HEADER + n * 24 * 16 * 4
    cells = []
    for i in range(w * h):
        height, _, _, flags, top, _ = struct.unpack_from(FMT, data, off + i * CELL)
        cells.append((height, flags, top))
    return w, h, cells


def walkable(cells, w, h, x, y):
    if not (0 <= x < w and 0 <= y < h):
        return False
    height, flags, top = cells[y * w + x]
    return top != 0xFFFF and not (flags & BLOCK)


def reachable(w, h, cells, seeds):
    """Marche <= 1 niveau, saut <= 2 niveaux (JUMP_CLIMB)."""
    seeds = {(x, y) for x, y in seeds if walkable(cells, w, h, x, y)}
    seen = set(seeds)
    queue = deque(seeds)
    while queue:
        x, y = queue.popleft()
        cur = cells[y * w + x][0]
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if (nx, ny) in seen or not walkable(cells, w, h, nx, ny):
                continue
            if cells[ny * w + nx][0] <= cur + 2:  # saut : +2 niveaux max, chute libre sinon
                seen.add((nx, ny))
                queue.append((nx, ny))
    return seen


dialogues = set(re.findall(r'^\[([^\]]+)\]', (ROOT / 'data' / 'dialogues.txt').read_text(encoding='utf-8'), re.M))
# Doit rester synchronise avec la liste de main.cpp (tryInteract).
VARIANT_EVENTS = {'objet_gantelet', 'objet_lettre', 'objet_ordre', 'objet_idole', 'objet_lampe',
                  'objet_prisme', 'objet_cristal'}
for did in dialogues:
    if '_objet_' in did:
        event = 'objet_' + did.rsplit('_objet_', 1)[1]
        if event not in VARIANT_EVENTS:
            warn(f'variante de dialogue jamais declenchee : {did} ({event})')
sprites = {p.stem for p in (ROOT / 'assets' / 'sprites').glob('*.a1s')}
music = {}
for line in read_lines('music.txt'):
    parts = clean(line).split()
    if len(parts) > 1:
        music[parts[0]] = set(parts[1:])

maps = {}
points = {}          # zone -> {(x, y, etiquette)}
entries = {}         # zone -> [(x, y)]
world = []           # commandes du monde, pour la verification de la route complete
warps_all = []       # passages, pour la verification de la route complete
npc_positions = set()  # (zone, x, y) : un passage ne doit pas deposer le heros sur un PNJ
known = set()        # objets et evenements utilisables comme condition
known |= {'epee_supreme', 'objet_gantelet', 'objet_ordre'}  # octroyes par Jeehan


def point(zone, x, y, label):
    points.setdefault(zone, set()).add((x, y, label))


def entry(zone, x, y):
    entries.setdefault(zone, set()).add((x, y))


# --- Monde ---------------------------------------------------------------------
for raw in read_lines('world.txt'):
    line = clean(raw)
    if not line:
        continue
    p = line.split()
    cmd = p[0]
    world.append(p)
    if cmd == 'start':
        entry(p[1], int(p[2]), int(p[3]))
    elif cmd in ('spawn', 'npc', 'chest', 'name', 'event', 'zone'):
        if len(p) < 2:
            bad(f'ligne incomplete : {line}')
            continue
        zone = p[1]
        if cmd == 'spawn' and len(p) >= 5:
            kind = p[4]
            if (ROOT / 'assets' / 'sprites' / f'{kind}.a1s').exists() is False:
                bad(f'sprite du monstre absent : {kind}')
            point(zone, int(p[2]), int(p[3]), f'monstre {kind}')
        elif cmd == 'npc' and len(p) >= 7:
            sprite, dialogue = p[4], p[6]
            if sprite not in sprites:
                bad(f'sprite du PNJ absent : {sprite}')
            if p[5] not in ('bas', 'haut', 'gauche', 'droite'):
                bad(f'direction inconnue : {p[5]}')
            if dialogue not in dialogues and not any(k.startswith(dialogue + '_') for k in dialogues):
                warn(f'dialogue du PNJ absent : {dialogue}')
            point(zone, int(p[2]), int(p[3]), f'PNJ {dialogue}')
            npc_positions.add((zone, int(p[2]), int(p[3])))
        elif cmd == 'chest' and len(p) >= 5:
            item, _, qty = p[4].partition(':')  # « objet:n » : plusieurs unites (materiaux du remake)
            if qty and (not qty.isdigit() or int(qty) < 1):
                bad(f'quantite de coffre invalide : {p[4]}')
            known.add(item)
            if f'objet_{item}' not in dialogues:
                bad(f'dialogue de l objet absent : objet_{item}')
            if len(p) >= 6:
                known.add(p[5])
            point(zone, int(p[2]), int(p[3]), f'coffre {item}')
        elif cmd == 'event' and len(p) >= 4:
            if p[3] not in dialogues:
                bad(f'dialogue d evenement absent : {p[3]}')
            known.add(p[2])
        elif cmd == 'zone' and len(p) >= 3:
            if p[2] not in music:
                bad(f'categorie musicale inconnue : {p[2]}')
            else:
                for track in music[p[2]]:
                    if not (ROOT / 'assets' / 'music' / f'{int(track):02d}.wav').exists():
                        bad(f'piste absente : {track} ({p[1]})')
    elif cmd == 'map' and len(p) >= 3:
        if not (ROOT / 'assets' / 'maps' / f'{p[2]}.a1m').exists():
            bad(f'carte de remplacement absente : {p[2]}.a1m (zone {p[1]})')
        map_for[p[1]] = p[2]
    elif cmd == 'music' and len(p) >= 3:
        if not (ROOT / 'assets' / 'music' / f'{int(p[2]):02d}.wav').exists():
            bad(f'piste absente : {p[2]} ({p[1]})')

# --- Passages ------------------------------------------------------------------
for raw in read_lines('warps.txt'):
    line = clean(raw)
    if not line:
        continue
    left, right = line.split('->')
    a, b = left.split(), right.split()
    if len(a) != 5 or len(b) not in (4, 5):
        bad(f'passage mal forme : {line}')
        continue
    zs, xs, ys = a[0], int(a[1]), int(a[2])
    zd, xd, yd = b[0], int(b[1]), int(b[2])
    warps_all.append({'zone': zs, 'x': int(a[1]), 'y': int(a[2]), 'w': int(a[3]), 'h': int(a[4]),
                      'dest': zd, 'dx': xd, 'dy': yd,
                      'need': b[4] if len(b) == 5 else ''})
    if len(b) == 5:
        if b[4] not in known:
            bad(f'condition de passage inconnue : {b[4]}')
        known.add(b[4])
    point(zs, xs, ys, 'passage sortant')
    point(zd, xd, yd, 'passage entrant')
    if (zd, xd, yd) in npc_positions:
        bad(f'passage {zs}({xs},{ys}) depose le heros sur un PNJ en {zd}({xd},{yd})')
    entry(zd, xd, yd)

# --- Ajouts du remake (tranche 3) : armes, materiaux, forge, interactions ---------
# Plan de la tranche 3, tâche 1.
EFFECTS = {'none', 'fire', 'ice', 'heavy'}
RM_WEAPONS = {'rm_ember_blade', 'rm_frost_lance', 'rm_rift_hammer'}
RM_MATERIALS = {'rm_ember_shard', 'rm_frost_core', 'rm_rift_fragment', 'rm_ironwood', 'rm_rune_dust'}
INTERACTION_TYPES = {'flammable', 'ice_surface', 'fragile', 'mechanism', 'lever'}


def data_rows(name, fields):
    path = ROOT / 'data' / name
    if not path.exists():
        bad(f'{name} absent')
        return []
    rows = []
    for number, raw in enumerate(path.read_text(encoding='utf-8-sig').splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith('#'):
            continue
        parts = [p.strip() for p in line.split('|')]
        if len(parts) != fields:
            bad(f'{name}:{number} : {len(parts)} champs, attendu {fields}')
            continue
        rows.append((number, parts))
    return rows


def as_int(name, number, value, label):
    try:
        v = int(value)
    except ValueError:
        bad(f'{name}:{number} : {label} non entier ({value!r})')
        return None
    if v < 0:
        bad(f'{name}:{number} : {label} negatif ({v})')
    return v


item_kinds = {}
item_power = {}
for number, parts in data_rows('items.txt', 7):
    iid = parts[0]
    if iid in item_kinds:
        bad(f'items.txt:{number} : identifiant duplique {iid}')
    item_kinds[iid] = parts[3]
    item_power[iid] = parts[6]

rm_weapons = {}
for number, p in data_rows('rm_weapons.txt', 9):
    wid = p[0]
    if wid in rm_weapons:
        bad(f'rm_weapons.txt:{number} : arme dupliquee {wid}')
    stats = [as_int('rm_weapons.txt', number, v, label) for v, label in
             zip(p[3:8], ('degats', 'vitesse', 'portee', 'recul', 'coutEp'))]
    if p[8] not in EFFECTS:
        bad(f'rm_weapons.txt:{number} : effet inconnu {p[8]}')
    if not wid.startswith('rm_'):
        bad(f'rm_weapons.txt:{number} : {wid} doit porter le prefixe rm_ (ajout du remake)')
    if item_kinds.get(wid) != 'Weapon':
        bad(f'rm_weapons.txt:{number} : {wid} absent de items.txt ou pas de type Weapon')
    elif stats[0] is not None and item_power.get(wid) != str(stats[0]):
        bad(f'items.txt : puissance de {wid} = {item_power.get(wid)}, attendu les degats {stats[0]}')
    if f'objet_{wid}' not in dialogues:
        bad(f'dialogue absent : objet_{wid}')
    rm_weapons[wid] = p
if set(rm_weapons) != RM_WEAPONS:
    bad(f'armes du remake attendues {sorted(RM_WEAPONS)}, trouvees {sorted(rm_weapons)}')

rm_materials = set()
for number, p in data_rows('rm_materials.txt', 4):
    mid = p[0]
    if mid in rm_materials:
        bad(f'rm_materials.txt:{number} : materiau duplique {mid}')
    rm_materials.add(mid)
    if item_kinds.get(mid) != 'Key':
        bad(f'rm_materials.txt:{number} : {mid} absent de items.txt ou pas de type Key (jamais consomme par H/menu)')
    if f'objet_{mid}' not in dialogues:
        bad(f'dialogue absent : objet_{mid}')
if rm_materials != RM_MATERIALS:
    bad(f'materiaux attendus {sorted(RM_MATERIALS)}, trouves {sorted(rm_materials)}')

recipes = set()
for number, p in data_rows('rm_forge.txt', 13):
    weapon, branch, rank = p[0], p[1], p[2]
    rid = f'{weapon}_{branch}_{rank}'
    if rid in recipes:
        bad(f'rm_forge.txt:{number} : recette dupliquee {rid}')
    recipes.add(rid)
    if weapon not in rm_weapons:
        bad(f'rm_forge.txt:{number} : arme inconnue {weapon}')
    if rank != '1':
        bad(f'rm_forge.txt:{number} : rang {rank} (premiere tranche : rang 1 seulement)')
    cost = {}
    for chunk in p[5].split(','):
        mid, _, qty = chunk.partition(':')
        if mid not in rm_materials:
            bad(f'rm_forge.txt:{number} : materiau inconnu {mid!r}')
        q = as_int('rm_forge.txt', number, qty, f'quantite de {mid}')
        if q is not None and q < 1:
            bad(f'rm_forge.txt:{number} : quantite nulle pour {mid}')
        cost[mid] = q
    if not cost:
        bad(f'rm_forge.txt:{number} : aucun materiau')
    for v, label in zip(p[6:11], ('degats', 'vitesse', 'portee', 'recul', 'coutEp')):
        as_int('rm_forge.txt', number, v, label)
    if p[11] not in EFFECTS:
        bad(f'rm_forge.txt:{number} : effet inconnu {p[11]}')
    if not p[12]:
        bad(f'rm_forge.txt:{number} : contrepartie manquante')
if len(recipes) != 6:
    bad(f'rm_forge.txt : {len(recipes)} recettes, attendu 6 (2 branches x 3 armes)')
for wid in rm_weapons:
    if sum(1 for r in recipes if r.startswith(wid + '_')) != 2:
        bad(f'rm_forge.txt : {wid} doit avoir exactement 2 branches')

interactions = {}
for number, p in data_rows('rm_interactions.txt', 8):
    zone, iid, kind = p[0], p[3], p[4]
    x = as_int('rm_interactions.txt', number, p[1], 'x')
    y = as_int('rm_interactions.txt', number, p[2], 'y')
    if iid in interactions:
        bad(f'rm_interactions.txt:{number} : interaction dupliquee {iid}')
    if not iid.startswith('rm_'):
        bad(f'rm_interactions.txt:{number} : {iid} doit porter le prefixe rm_')
    if kind not in INTERACTION_TYPES:
        bad(f'rm_interactions.txt:{number} : type inconnu {kind}')
    sols = p[5].split(',')
    if kind == 'mechanism':
        if not re.fullmatch(r'count\d+', p[5]):
            bad(f'rm_interactions.txt:{number} : mecanisme {iid} sans countN')
    elif kind == 'lever':
        if p[5] != 'reset':
            bad(f'rm_interactions.txt:{number} : levier {iid} : solution attendue reset')
    elif not sols or any(s not in EFFECTS - {'none'} for s in sols):
        bad(f'rm_interactions.txt:{number} : solutions inconnues {p[5]!r}')
    if p[6] not in ('puzzle', 'none'):
        bad(f'rm_interactions.txt:{number} : reset {p[6]!r} (puzzle ou none)')
    if not (ROOT / 'assets' / 'maps' / f"{map_for.get(zone, zone)}.a1m").exists():
        bad(f'rm_interactions.txt:{number} : zone inexistante {zone}')
    if (zone, x, y) in npc_positions:
        bad(f'rm_interactions.txt:{number} : {iid} sur un PNJ en {zone}({x},{y})')
    if any(c[0] == 'chest' and c[1] == zone and int(c[2]) == x and int(c[3]) == y for c in world):
        bad(f'rm_interactions.txt:{number} : {iid} sur un coffre en {zone}({x},{y})')
    if x is not None and y is not None:
        point(zone, x, y, f'interaction {iid}')
    interactions[iid] = (zone, kind, p[5], p[6], p[7])
for iid, (zone, kind, sol, reset, result) in interactions.items():
    if kind == 'mechanism':
        inputs = [k for k, v in interactions.items() if v[4] == iid and v[1] not in ('mechanism', 'lever')]
        need = int(sol[5:]) if sol.startswith('count') and sol[5:].isdigit() else 0
        if need < 1 or need > len(inputs):
            bad(f'rm_interactions.txt : mecanisme {iid} demande {need} entrees, {len(inputs)} disponibles')
        if len({tuple(interactions[k][2].split(',')) for k in inputs}) < 2:
            bad(f'rm_interactions.txt : {iid} doit offrir plusieurs solutions distinctes')
    elif result not in interactions or interactions[result][1] != 'mechanism':
        bad(f'rm_interactions.txt : {iid} vise un mecanisme inconnu {result}')
    if result in ('epee_supreme', 'lampe', 'prisme', 'boss_medusa', 'cristal'):
        bad(f'rm_interactions.txt : {iid} ne doit pas toucher au gate historique {result}')
for zone in {v[0] for v in interactions.values()}:
    if sum(1 for v in interactions.values() if v[0] == zone) > 5:
        bad(f'rm_interactions.txt : plus de 5 objets interactifs en zone {zone}')
# Dialogues utilises par le code du remake (forgeron, forge, enigme) : revue QA tranche 3.
for did in ('rm_forgeron', 'rm_forgeron_forge', 'rm_forge_ok', 'rm_forge_manque', 'rm_forge_deja',
            'rm_forge_sans_arme', 'rm_mecanisme_ouvert', 'rm_mecanisme_reinit', 'rm_brasero_allume',
            'rm_chemin_gele', 'rm_pilier_brise', 'rm_interaction_sans_effet'):
    if did not in dialogues:
        bad(f'dialogue du remake absent : {did}')
# Les coffres du monde doivent permettre de forger, pour chaque arme, sa branche la plus chere
# (les deux branches d'une arme sont exclusives).
need = {}
per_weapon = {}
for number, p in data_rows('rm_forge.txt', 13):
    for chunk in p[5].split(','):
        mid, _, qty = chunk.partition(':')
        if qty.isdigit():
            w = per_weapon.setdefault(p[0], {})
            w[mid] = max(w.get(mid, 0), int(qty))
for w in per_weapon.values():
    for mid, q in w.items():
        need[mid] = need.get(mid, 0) + q
supply = {}
for c in world:
    if c[0] == 'chest' and len(c) >= 5:
        mid, _, qty = c[4].partition(':')
        supply[mid] = supply.get(mid, 0) + (int(qty) if qty.isdigit() else 1)
for mid, q in sorted(need.items()):
    if supply.get(mid, 0) < q:
        bad(f'materiau {mid} : {supply.get(mid, 0)} dans les coffres, {q} necessaires pour forger chaque arme')
for wid in rm_weapons:
    if supply.get(wid, 0) < 1 and wid != 'rm_rift_hammer':  # le marteau est donne par Orvan
        bad(f'arme du remake {wid} introuvable dans les coffres du monde')
collide = [i for i in (set(rm_weapons) | rm_materials | set(interactions))
           if i in item_kinds and not i.startswith('rm_')]
if collide or 'hammer' in rm_weapons:
    bad(f'collision avec des identifiants A2 : {collide or ["hammer"]}')

# --- Choix et actions de dialogue (B02) -----------------------------------------
#   ? Oui > cible | Non > cible      (au moins deux reponses ; cible = dialogue existant ou vide)
#   ! prend|donne <objet> [n]   ! evenement <id>   ! sinon <dialogue>   ! si <evenement> <dialogue>
current = None
for number, raw in enumerate((ROOT / 'data' / 'dialogues.txt').read_text(encoding='utf-8-sig').splitlines(), 1):
    line = raw.strip()
    if not line or line.startswith('#'):
        continue
    if line.startswith('[') and line.endswith(']'):
        current = line[1:-1]
        continue
    if line.startswith('? '):
        options = [o.strip() for o in line[2:].split('|')]
        if len(options) < 2:
            bad(f'dialogues.txt:{number} : une question demande au moins deux reponses')
        for option in options:
            label, _, target = option.partition('>')
            if not label.strip():
                bad(f'dialogues.txt:{number} : reponse sans libelle')
            if target.strip() and target.strip() not in dialogues:
                bad(f'dialogues.txt:{number} : reponse vers un dialogue inconnu {target.strip()}')
    elif line.startswith('! '):
        parts = line[2:].split()
        verb = parts[0] if parts else ''
        if verb in ('prend', 'donne'):
            if len(parts) < 2 or parts[1] not in item_kinds:
                bad(f'dialogues.txt:{number} : action {verb} sur un objet inconnu')
            if len(parts) >= 3 and not parts[2].isdigit():
                bad(f'dialogues.txt:{number} : quantite invalide')
        elif verb == 'evenement':
            if len(parts) < 2:
                bad(f'dialogues.txt:{number} : evenement sans identifiant')
            else:
                known.add(parts[1])
        elif verb == 'sinon':
            if len(parts) < 2 or parts[1] not in dialogues:
                bad(f'dialogues.txt:{number} : branche sinon vers un dialogue inconnu')
        elif verb == 'si':
            if len(parts) < 3 or parts[2] not in dialogues:
                bad(f'dialogues.txt:{number} : variante si vers un dialogue inconnu')
        else:
            bad(f'dialogues.txt:{number} : action inconnue {verb!r}')

# --- Cartes et accessibilite ---------------------------------------------------
for zone, marks in points.items():
    m = maps.get(zone) or load_map(zone)
    maps[zone] = m
    if m is None:
        continue
    w, h, cells = m
    for x, y, label in marks:
        if not walkable(cells, w, h, x, y):
            bad(f'{zone} ({x},{y}) non marchable : {label}')
    seeds = entries.get(zone, set())
    if not seeds:
        bad(f'{zone} : aucune entree (start ou passage entrant)')
        continue
    seen = reachable(w, h, cells, seeds)
    for x, y, label in marks:
        if walkable(cells, w, h, x, y) and (x, y) not in seen:
            bad(f'{zone} ({x},{y}) inaccessible : {label}')

# --- Route complete depuis le debut ---------------------------------------------
# Verifie que l'enchainement des passages et des reliques permet d'atteindre le
# boss final. Les evenements sont ajoutes au fur et a mesure : un coffre garde par
# un boss ne compte que si ce boss est accessible, etc.
route_entries = {}
for p in world:
    if p[0] == 'start' and len(p) >= 4:
        route_entries.setdefault(p[1], set()).add((int(p[2]), int(p[3])))

have = set()
for _ in range(40):
    reach = {z: reachable(*maps[z], seeds) for z, seeds in route_entries.items() if z in maps}
    changed = False
    for w in warps_all:
        source = [(w['x'] + i, w['y'] + j) for i in range(w['w']) for j in range(w['h'])]
        if not any(cell in reach.get(w['zone'], set()) for cell in source):
            continue
        if w['need'] and w['need'] not in have:
            continue
        dest = (w['dx'], w['dy'])
        if dest not in route_entries.setdefault(w['dest'], set()):
            route_entries[w['dest']].add(dest)
            changed = True
    for p in world:
        if len(p) < 4:
            continue
        cmd, zone = p[0], p[1]
        if cmd == 'chest' and len(p) >= 5:
            need = p[5] if len(p) >= 6 else ''
            if need == 'ogre':
                ok = 'bossDown' in have
            else:
                ok = not need or need in have
            if ok and (int(p[2]), int(p[3])) in reach.get(zone, set()):
                for value in (p[4].split(':')[0], 'objet_' + p[4].split(':')[0]):
                    if value not in have:
                        have.add(value)
                        changed = True
        elif cmd == 'spawn' and (int(p[2]), int(p[3])) in reach.get(zone, set()):
            gained = {'ogre': 'bossDown', 'golem': 'boss_' + zone, 'capitaine': 'boss_medusa'}.get(p[4])
            if gained and gained not in have:
                have.add(gained)
                changed = True
        elif cmd == 'npc' and len(p) >= 7 and p[6] == 'jeehan' and 'ordre' in have:
            if (int(p[2]), int(p[3])) in reach.get(zone, set()) and 'epee_supreme' not in have:
                have.add('epee_supreme')
                changed = True
    if not changed:
        break

reach = {z: reachable(*maps[z], seeds) for z, seeds in route_entries.items() if z in maps}
for w in warps_all:
    source = [(w['x'] + i, w['y'] + j) for i in range(w['w']) for j in range(w['h'])]
    if w['need'] and any(cell in reach.get(w['zone'], set()) for cell in source) and w['need'] not in have:
        bad(f"route : passage {w['zone']}({w['x']},{w['y']}) bloque par {w['need']}")
if '120' not in route_entries:
    bad('route : salle du trone (120) inaccessible')
else:
    final_boss = next((p for p in world if p[0] == 'spawn' and p[1] == '120' and p[4] == 'mephisto'), None)
    if not final_boss or (int(final_boss[2]), int(final_boss[3])) not in reach.get('120', set()):
        bad('route : Méphisto inaccessible dans la salle du trone')
route_zones = len(route_entries)
route_items = len([v for v in have if not v.startswith('objet_')])

# --- Bruitages -----------------------------------------------------------------
for raw in read_lines('sfx.txt'):
    p = clean(raw).split()
    if len(p) >= 2 and p[1].isdigit():
        if not (ROOT / 'assets' / 'sfx' / f'{int(p[1]):02d}.wav').exists():
            bad(f'bruitage absent : {p[0]} -> {p[1]}')

print(f' dialogues {len(dialogues)} | sprites {len(sprites)} | zones {len(points)} | passages {sum(len(v) for v in entries.values())}')
print(f' route : {route_zones} zones | {route_items} objets/evenements | boss final accessible')
print(f' ERR: {len(errors)} | WARN: {len(warnings)}')
for message in warnings:
    print('  WARN', message)
for message in errors:
    print('  ERR ', message)
sys.exit(1 if errors else 0)
