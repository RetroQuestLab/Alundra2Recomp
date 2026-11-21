"""Flint (Alundra 2) dessine a la main, pixel par pixel, au format des sprites d'Alundra 1.

Poses fixes (face, dos, gauche, droite) a valider avant d'animer.
    python tools/flint_poses.py apercu.png
"""
import sys

from PIL import Image

PAL = {
    "K": (26, 12, 22),                                                           # contour
    "r": (120, 24, 28), "R": (200, 56, 40), "H": (246, 112, 66), "h": (255, 178, 128),  # cheveux
    "s": (196, 120, 84), "S": (240, 178, 130), "L": (255, 218, 176),             # peau
    "E": (60, 20, 34), "w": (255, 255, 255),                                     # yeux
    "b": (30, 44, 124), "B": (52, 90, 200), "C": (116, 164, 244),                # echarpe
    "o": (176, 72, 20), "O": (238, 126, 34), "Y": (255, 192, 98),                # haut orange
    "p": (96, 70, 170), "P": (150, 122, 232), "Q": (212, 198, 255),              # epauliere
    "d": (70, 40, 26), "D": (114, 68, 42), "T": (164, 108, 66),                  # cuir
    "G": (232, 188, 72),                                                         # boucle
    "n": (40, 48, 136), "N": (70, 88, 202), "M": (124, 144, 238),                # pantalon
    "v": (110, 84, 210), "V": (172, 148, 255),                                   # semelles
}

FRONT = [
    "..........K...K.........",
    "......K..KRK.KRK..K.....",
    ".......KKRRRKRHRKKK.....",
    "......KRRRHRRRHHRRRK....",
    ".....KRRHHHRRHHHHRRRK...",
    "....KRRHHhhHHHhHHRRRrK..",
    "....KRHHHHHHHHHHHRRRrK..",
    "...KRRRHHHRRRRHHRRRrrK..",
    "...KRRRRRRRRRRRRRRRrrrK.",
    "...KrRRKRRRRRRKRRRRRrrK.",
    "...KrRKSRRrRRKSRRrRrrK..",
    "...KrKSSLRSSRSSSRSSrrK..",
    "....KrSLSSSSSSSSSSSsK...",
    "....KSKKKSSSSSSKKKSsK...",
    "....KSKEwSSSSSSKEwSsK...",
    "....KsSEESSSSSSSEESsK...",
    ".....KsSSSSSsSSSSSsK....",
    ".....KssSSSSDDSSSssK....",
    "......KKsSSSSSSSsKK.....",
    ".......KbKsssssKbK......",
    "......KbBBBBBBBBBBbKKK..",
    ".....KBCCBBBBBBBBBBKPPK.",
    "....KsKbBBBBBBBBBbKPQQPK",
    "...KSSKKOYOOODOOOKPPQPPK",
    "...KSLKOYYOOODDOOKpPPPpK",
    "...KSSKOYOOOOODOOKKppppK",
    "...KsSKOOOOOOOODOKSKKKK.",
    "...KsSKoOOOOOOOODKSSK...",
    "...KDDKKdDDDGGDDDKsSK...",
    "...KDTDKnNNNNNNNNKDDDK..",
    "...KdDdKNMNNNNNNNKDTDK..",
    "....KKKNMNNNKNNNNNKddK..",
    "......KNMNNNKKNNNNNKK...",
    "......KNMNNnKKNNNNnK....",
    ".....KNMNNNnKKNNNNNnK...",
    ".....KMMMMMMKKMMMMMMK...",
    "......KDTDDKK.KDTDDK....",
    "......KDTDDK..KDTDDK....",
    ".....KVVVVvK..KVVVVvK...",
    ".....KKKKKKK..KKKKKKK...",
]

BACK = [
    "..........K...K.........",
    "......K..KRK.KRK..K.....",
    ".......KKRRRKRHRKKK.....",
    "......KRRRHRRRHHRRRK....",
    ".....KRRHHHRRHHHHRRRK...",
    "....KRRHHhhHHHhHHRRRrK..",
    "....KRHHHHHHHHHHHRRRrK..",
    "...KRRRHHHRRRRHHRRRrrK..",
    "...KRRRRRRRRRRRRRRRrrrK.",
    "...KrRRRRRHRRRRRRRRRrrK.",
    "...KrRRRRHRRRRRHRRRRrrK.",
    "...KrRRRRRRRRRRRRRRRrrK.",
    "....KrRRRRRRrRRRRRRrrK..",
    "....KrrRRRRrRRRRRRrrrK..",
    "....KrrrRRrRRRrRRRrrrK..",
    ".....KrrrrRrrRrrRrrrK...",
    ".....KKrrrrrrrrrrrrKK...",
    "......KsKrrKrrKrrKsK....",
    ".......KsSKKKKKKSsK.....",
    "........KKsssssKK.......",
    "..KKKKbBBBBBBBBBBbK.....",
    ".KPPPKBBBBCCBBBBBBK.....",
    "KPQQPKbBBBBBBBBBbKsK....",
    "KPPQPPKOOObBbOOOOKSSK...",
    "KpPPPpKOOOOBOOOOOKSLSK..",
    "KppppKKOOOOBOOOOOKSSK...",
    ".KKKKSKOOOOOOOOOOKSsK...",
    "....KSSKOOOOOOOOoKsSK...",
    "....KsSKdDDDDDDDdKDDK...",
    "...KDDDKnNNNNNNNNKDTDK..",
    "...KDTDKNNNNNNNNNKdDdK..",
    "...KdDdKNNNNKNNNNNKKK...",
    "....KKKNNNNNKKNNNNNK....",
    "......KNNNNnKKNNNNnK....",
    ".....KNNNNNnKKNNNNNnK...",
    ".....KMMMMMMKKMMMMMMK...",
    "......KDDDDKK.KDDDDK....",
    "......KDTDDK..KDTDDK....",
    ".....KVVVVvK..KVVVVvK...",
    ".....KKKKKKK..KKKKKKK...",
]

RIGHT = [
    "........K..K............",
    "......K.KRKKRK.K........",
    ".....KRKRRRRRRKRK.......",
    "....KRRRRHHHRRRRK.......",
    "...KRRRHHHHhHHRRRK......",
    "..KRRRHHHhhHHHHRRK......",
    ".KRRRRHHHHHHHHHRRRK.....",
    "..KRRRRHHRRRRRRRRRRK....",
    "KrRRRRRRRRRRRRRRKRRK....",
    ".KrrRRRRRRRRRRRKSKRK....",
    "..KrrRRRRRRRRRKSSSKK....",
    "..KrrrRRRRRRRKSSLSSK....",
    "...KrrRRRRRrKSSSSSSK....",
    "...KrrrRRRrKsSSSKKSK....",
    "....KrrrrrKsSSSSKwEK....",
    "....KKrrrKKsSSSSSEESK...",
    "......KKKsKssSSSSSSSK...",
    ".........KssSSSSDDSK....",
    ".........KKsssSSSsK.....",
    "..........KKKsssKK......",
    "........KKbBBBBBK.......",
    ".......KbBBCCBBBbK......",
    "......KbKOOYOOOObK......",
    "......KBKOYYKKKKOK......",
    ".....KbKKOOKPPPPKK......",
    ".....KBK.KOKPQQPPK......",
    "......K..KOKPQPPpK......",
    ".........KOOKpppK.......",
    ".........KOOKSSK........",
    ".........KoOKSLK........",
    ".........KdGKDDDK.......",
    ".........KnNKDTDK.......",
    "........KNMNNKKK........",
    "........KNMNNNK.........",
    ".......KNMNNNNNK........",
    ".......KMMMKMMMMK.......",
    ".......KDTDKKDTDK.......",
    ".......KDTDK.KDTDK......",
    "......KVVVVK.KVVVVVK....",
    "......KKKKKK.KKKKKKK....",
]

W, H = 24, 40


def render(grid):
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    for y, row in enumerate(grid[:H]):
        for x, ch in enumerate(row[:W]):
            if ch in PAL:
                img.putpixel((x, y), (*PAL[ch], 255))
    return img


POSES = {"face": FRONT, "dos": BACK, "gauche": None, "droite": RIGHT}


def poses():
    right = render(RIGHT)
    return [render(FRONT), render(BACK), right.transpose(Image.FLIP_LEFT_RIGHT), right]


if __name__ == "__main__":
    imgs = poses()
    z = 8
    sheet = Image.new("RGBA", ((W + 4) * 4 * z, (H + 4) * z * 2), (0, 0, 0, 255))
    for k, img in enumerate(imgs):
        for row, bg in enumerate(((90, 120, 60, 255), (40, 44, 60, 255))):
            cell = Image.new("RGBA", (W + 4, H + 4), bg)
            cell.alpha_composite(img, (2, 2))
            sheet.paste(cell.resize(((W + 4) * z, (H + 4) * z), Image.NEAREST), (k * (W + 4) * z, row * (H + 4) * z))
    sheet.save(sys.argv[1])
