"""Decompresseur "EZ" d'Alundra 1 (reimplementation de la routine 0x80080E14 d'ALUN_CD.EXE).

Flux : un octet != 0xAD est un litteral.
    AD 00       -> litteral 0xAD
    AD d 00     -> fin du flux
    AD d L      -> copie L octets depuis (sortie - d)
En-tete : 6 octets, "EZ" suivi d'un nom sur 4 octets (ex. ".tx\\0").
"""


def decompress(src, start=None):
    if start is None:
        if src[:2] != b"EZ":
            raise ValueError("pas un flux EZ")
        start = 6  # "EZ" + nom sur 4 octets (".tx\0" ou "\0\0\0\0")
    out = bytearray()
    p, n = start, len(src)
    while p < n:
        c = src[p]
        p += 1
        if c != 0xAD:
            out.append(c)
            continue
        d = src[p]
        p += 1
        if d == 0:
            out.append(0xAD)
            continue
        ln = src[p]
        p += 1
        if ln == 0:
            return bytes(out), p
        if d > len(out):
            raise ValueError(f"reference invalide a {p}")
        for _ in range(ln):
            out.append(out[-d])
    return bytes(out), p
