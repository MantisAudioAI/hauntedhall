# Haunted Hall screen layout at 1024 x 600 (KICK's screen), measured from the backgrounds.
# Rects are [x, y, w, h]; knobs are [cx, cy, wellRadius, arcRadius].
def build():
    L = {"screen": [1024, 600]}
    # ---------------- MASTER ----------------
    rowsOn = [152, 244, 336, 429]
    meterTop = [173, 261, 353, 446]
    L["master"] = {
        "on":    [[255, y, 49, 35] for y in rowsOn],
        "mute":  [[317, y, 39, 35] for y in rowsOn],
        "meter": [[392, y, 147, 13] for y in meterTop],      # 12 segments inside
        "meterSegments": 12,
        "faderX": [650, 746, 839, 936], "faderTop": 160, "faderBottom": 266, "faderCap": [26, 12],
        "bypassLed": [650, 366, 7],
        "bypassHit": [618, 352, 64, 45],
        "timeDisplay": [726, 357, 132, 33],
        "knobs": {"TIME": [646, 440, 21, 31], "FEEDBACK": [741, 440, 21, 31], "MIX": [836, 439, 21, 31], "LEVEL": [931, 440, 21, 31]},
        "tabMaster": [207, 528, 301, 48], "tabDrum": [518, 528, 300, 48],
        "ghostSprite": [905, 18], "decryptBar": None,
    }
    # ---------------- STEP ----------------
    colX = [round(223.1 + 49.98 * c - 21.5) for c in range(16)]
    rowY = [round(219.4 + 40.04 * r - 15.5) for r in range(5)]
    L["step"] = {
        "cells": {"colX": colX, "rowY": rowY, "w": 45, "h": 32},
        "stepNumbers": {"y": 166, "h": 31, "w": 46},
        "barStrip": {"y": 151, "h": 11},                 # one slot per column = bars 1-16
        "barSelector": [32, 162, 160, 34],               # "< BAR 01/08 >" in the empty box
        "bars816": [567, 130, 158, 20],                  # left half = 8 BARS, right half = 16 BARS
        "patternAB": [733, 130, 106, 20],
        "rowLabel": [[28, y - 1, 165, 33] for y in rowY],
        "rowLock": [[34, y + 2, 14, 14] for y in rowY],
        "melodyStrip": [200, 398, 795, 6],
        "knobs": {"DECAY": [264, 446, 26, 37], "TONE": [449, 446, 26, 37], "SPOOK": [647, 446, 26, 37], "SPACE": [841, 446, 26, 37]},
        "buttons": {"RANDOM": [45, 525, 177, 51], "CLEAR": [235, 525, 176, 51], "COPY": [424, 525, 176, 51],
                    "PASTE": [614, 525, 176, 51], "UNDO": [803, 525, 176, 51]},
        "decryptBar": [810, 14, 198, 30],
        "batSprite": [128, 40],
    }
    # ---------------- CARTRIDGE BROWSER ----------------
    L["browser"] = {
        "tabs": {"DRUM": [311, 16, 148, 57], "BASS": [483, 16, 147, 57], "LEAD": [654, 16, 144, 57], "PAD": [820, 16, 147, 57]},
        "bankPrev": [416, 92, 51, 32], "bankName": [470, 92, 341, 32], "bankNext": [814, 92, 51, 32],
        "presetSlots": [[339, y, 247, 34] for y in [141, 181, 222, 262, 303]] + [[704, y, 248, 34] for y in [141, 181, 222, 262, 303]],
        "random": [600, 180, 90, 95],
        "favorite": [600, 302, 90, 35],
        "cartWindow": [87, 142, 178, 204],
        "cartArt": [87, 148, 178, 192],          # 96 x 96 art at 2x, 3-4 px trimmed each side to fit
        "knobs": {"DECAY": [424, 392, 24, 36], "TONE": [569, 392, 24, 36], "SPOOK": [717, 393, 24, 36], "SPACE": [866, 392, 24, 36]},
        "carouselPrev": [130, 487, 31, 43], "carouselNext": [617, 487, 31, 43],
        "carouselSlots": [[175, 480, 66, 65], [247, 480, 65, 65], [319, 480, 65, 65], [391, 480, 65, 65], [463, 480, 65, 65], [535, 480, 66, 65]],
        "loadPreset": [676, 470, 259, 46], "back": [676, 526, 259, 46],
    }
    return L
