# Renders what the screens look like with their live parts drawn on top of the
# backgrounds: the same drawing the JUCE code will do. Preview only.
import json, math, sys
from PIL import Image, ImageDraw, ImageFont
sys.path.insert(0, '.')
from layout import build

L = build()
OUT = '/home/claude/hhart/out'
MONO = '/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf'
P = {  # palette sampled from the parts sheet
    'mint': (132, 251, 188), 'mintHi': (228, 255, 233), 'accent': (157, 255, 204),
    'soft': (59, 112, 93), 'softEdge': (132, 172, 147), 'dimTick': (44, 74, 62),
    'lav': (235, 200, 240), 'lavMid': (217, 167, 228), 'knob': (151, 123, 154), 'fader': (197, 170, 205),
    'faderLine': (120, 93, 135), 'dark': (22, 20, 34), 'text': (225, 197, 232), 'darkText': (16, 27, 27),
}
pat = json.load(open('pattern.json'))

def font(sz): return ImageFont.truetype(MONO, sz)

def knob(d, cx, cy, well, arc, value):
    # 11 segment ring over the baked one: dim ring, then the lit part
    a0, a1 = 135.0, 405.0
    segs = 11
    for i in range(segs):
        t0 = a0 + (a1 - a0) * i / segs + 3
        t1 = a0 + (a1 - a0) * (i + 1) / segs - 3
        lit = (i + 0.5) / segs <= value
        d.arc([cx - arc, cy - arc, cx + arc, cy + arc], t0, t1, fill=P['mint'] if lit else P['dimTick'], width=7)
    r = well - 3
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=P['knob'], outline=P['lav'], width=2)
    ang = math.radians(a0 + (a1 - a0) * value)
    d.line([cx + math.cos(ang) * r * 0.25, cy + math.sin(ang) * r * 0.25,
            cx + math.cos(ang) * (r - 3), cy + math.sin(ang) * (r - 3)], fill=P['lav'], width=3)

def strip(path, n, idx, size):
    s = Image.open(path).convert('RGBA'); w = s.size[0] // n
    return s.crop((idx * w, 0, (idx + 1) * w, s.size[1])).resize(size, Image.NEAREST)

def decrypt(im, d, rect, lit):
    x, y, w, h = rect
    d.text((x, y), "DECRYPTING...", font=font(9), fill=P['text'])
    n = 12; sw = (w - 4) / n
    for i in range(n):
        sx = x + 2 + i * sw
        d.rectangle([sx + 1, y + 14, sx + sw - 2, y + h - 2], fill=P['mint'] if i < lit else P['dark'],
                    outline=P['faderLine'])

# ---------------- STEP ----------------
S = L['step']
im = Image.open(f'{OUT}/backgrounds/bg_step.png').convert('RGBA')
d = ImageDraw.Draw(im)
cw, ch = S['cells']['w'], S['cells']['h']
play = 4
for c, x in enumerate(S['cells']['colX']):
    for r, y in enumerate(S['cells']['rowY']):
        v = pat['drums'][c][r]
        if v:
            fill, edge = (P['accent'], P['mintHi']) if v >= 117 else ((P['soft'], P['softEdge']) if v < 74 else (P['mint'], P['mintHi']))
            d.rectangle([x + 3, y + 3, x + cw - 5, y + ch - 4], fill=fill)
            d.line([x + 3, y + 3, x + cw - 5, y + 3], fill=edge, width=2)
    if c == play:   # playhead: the column's number button lights, cells get a thin outline
        ny = S['stepNumbers']['y']
        d.rectangle([x + 1, ny + 1, x + cw - 2, ny + S['stepNumbers']['h'] - 2], outline=P['mint'], width=2)
        for y in S['cells']['rowY']:
            d.rectangle([x + 1, y + 1, x + cw - 2, y + ch - 2], outline=P['mint'], width=1)
# bar strip: bar 1 selected (mint), bars that exist (8) lavender, bars with notes get a dot
for b, x in enumerate(S['cells']['colX']):
    by, bh = S['barStrip']['y'], S['barStrip']['h']
    if b < 8:
        col = P['mint'] if b == 0 else P['faderLine']
        d.rectangle([x + 3, by + 2, x + cw - 5, by + bh - 2], fill=col)
# bar selector
bx, by, bw, bh = S['barSelector']
d.text((bx + 8, by + 7), "<  BAR 01/08  >", font=font(15), fill=P['mint'])
# 8 / 16 bars and A / B: underline the active half
x, y, w, h = S['bars816']; d.rectangle([x + 4, y + h - 3, x + w // 2 - 6, y + h - 1], fill=P['mint'])
x, y, w, h = S['patternAB']; d.rectangle([x + 4, y + h - 3, x + w // 2 - 6, y + h - 1], fill=P['mint'])
# locks: kick and snare locked
lc = Image.open(f'{OUT}/icons/lock_closed_20.png').resize((14, 14), Image.LANCZOS)
lo = Image.open(f'{OUT}/icons/lock_open_20.png').resize((14, 14), Image.LANCZOS)
for r, rect in enumerate(S['rowLock']):
    d.rectangle([rect[0] - 1, rect[1] - 1, rect[0] + 14, rect[1] + 14], fill=(110, 110, 118))
    im.alpha_composite(lc if r < 2 else lo, (rect[0], rect[1]))
# melody strip: one tick per lane note in this bar
mx, my, mw, mh = S['melodyStrip']
for c, x in enumerate(S['cells']['colX']):
    note, length = pat['lane'][c]
    if note:
        d.rectangle([x + 3, my, x + 3 + min(length * 50, 50 * (16 - c)) - 8, my + mh - 1], fill=P['mint'])
# knobs
for (name, (cx, cy, w, a)), v in zip(S['knobs'].items(), [0.5, 0.62, 0.3, 0.5]):
    knob(d, cx, cy, w, a, v)
# RANDOM pressed look: a mint outline (the button that was just used)
x, y, w, h = S['buttons']['RANDOM']; d.rectangle([x, y, x + w - 1, y + h - 1], outline=P['mint'], width=2)
decrypt(im, d, S['decryptBar'], 9)
im.alpha_composite(strip(f'{OUT}/sprites/bat_4x64x64.png', 4, 1, (40, 40)), tuple(S['batSprite']))
im.convert('RGB').save(f'{OUT}/preview_step.png')

# ---------------- MASTER ----------------
M = L['master']
im = Image.open(f'{OUT}/backgrounds/bg_master.png').convert('RGBA')
d = ImageDraw.Draw(im)
ons = [True, True, True, False]; mutes = [False, False, True, False]
meters = [0.75, 0.58, 0.0, 0.0]; faders = [0.8, 0.8, 0.65, 0.9]
for i in range(4):
    x, y, w, h = M['on'][i]
    if ons[i]:
        d.rectangle([x + 3, y + 3, x + w - 4, y + h - 4], fill=P['mint'])
        d.text((x + w / 2, y + h / 2), "ON", font=font(14), fill=P['darkText'], anchor='mm')
    else:
        d.text((x + w / 2, y + h / 2), "ON", font=font(14), fill=(120, 100, 130), anchor='mm')
    if mutes[i]:
        x, y, w, h = M['mute'][i]; d.rectangle([x + 1, y + 1, x + w - 2, y + h - 2], outline=P['mint'], width=3)
    x, y, w, h = M['meter'][i]; n = M['meterSegments']; sw = w / n
    for s in range(int(round(meters[i] * n))):
        col = P['mint'] if s < 9 else ((240, 220, 120) if s < 11 else (255, 110, 140))
        d.rectangle([x + s * sw + 2, y + 2, x + (s + 1) * sw - 2, y + h - 2], fill=col)
for fx, v in zip(M['faderX'], faders):
    cw2, ch2 = M['faderCap']
    cy = M['faderBottom'] - v * (M['faderBottom'] - M['faderTop'])
    d.rectangle([fx - cw2 / 2, cy - ch2 / 2, fx + cw2 / 2, cy + ch2 / 2], fill=P['fader'], outline=P['lav'])
    d.line([fx - cw2 / 2 + 3, cy, fx + cw2 / 2 - 3, cy], fill=P['faderLine'], width=2)
x, y, w, h = M['timeDisplay']
d.text((x + w / 2, y + h / 2), "375 ms", font=font(19), fill=P['mint'], anchor='mm')
for (name, (cx, cy, w, a)), v in zip(M['knobs'].items(), [0.42, 0.39, 0.3, 0.8]):
    knob(d, cx, cy, w, a, v)
x, y, w, h = M['tabMaster']
ov = Image.new('RGBA', (w - 8, h - 8), P['mint'] + (70,)); im.alpha_composite(ov, (x + 4, y + 4))
d.rectangle([x + 2, y + 2, x + w - 3, y + h - 3], outline=P['mint'], width=3)
im.alpha_composite(strip(f'{OUT}/sprites/glitch_4x96x64.png', 4, 2, (96, 64)), (640, 2))
im.convert('RGB').save(f'{OUT}/preview_master.png')

# ---------------- cartridges contact sheet (2x, as the browser will show them) ----------------
import glob, os
fs = sorted(glob.glob(f'{OUT}/cartridges/*.png'))
sheet = Image.new('RGB', (5 * 212, 2 * 236), (24, 18, 34))
dd = ImageDraw.Draw(sheet)
for i, f in enumerate(fs):
    c = Image.open(f); c = c.resize((192, 192), Image.NEAREST)
    x, y = (i % 5) * 212 + 10, (i // 5) * 236 + 10
    sheet.paste(c, (x, y))
    dd.text((x, y + 196), os.path.basename(f)[5:-4].replace('_PROVISIONAL', ' *'), font=font(12), fill=P['text'])
sheet.save(f'{OUT}/preview_cartridges.png')
print('done')

# ---------------- BROWSER ----------------
B = L['browser']
im = Image.open(f'{OUT}/backgrounds/bg_browser.png').convert('RGBA')
d = ImageDraw.Draw(im)
# selected category tab: PAD (this is Synth B's browser)
x, y, w, h = B['tabs']['PAD']
im.alpha_composite(Image.new('RGBA', (w - 10, h - 10), P['mint'] + (45,)), (x + 5, y + 5))
d.rounded_rectangle([x + 2, y + 2, x + w - 3, y + h - 3], radius=8, outline=P['mint'], width=3)
x, y, w, h = B['bankName']
d.text((x + w / 2, y + h / 2), "SYNTH B  ·  PAD  ·  HAUNTED CASTLE", font=font(15), fill=P['mint'], anchor='mm')
names = ["Haunted Choir", "Cathedral", "Space Pad", "Corrupted Save", "Night Stabs", "Glowing Ambient"]
for i, rect in enumerate(B['presetSlots']):
    x, y, w, h = rect
    if i < len(names):
        sel = i == 0
        if sel:
            im.alpha_composite(Image.new('RGBA', (w - 8, h - 8), P['mint'] + (55,)), (x + 4, y + 4))
            d.rounded_rectangle([x + 1, y + 1, x + w - 2, y + h - 2], radius=6, outline=P['mint'], width=2)
        d.text((x + 12, y + h / 2), f"B{i + 1:02d}  {names[i].upper()}", font=font(14),
               fill=P['mint'] if sel else P['text'], anchor='lm')
# favorite: on for the selected preset
star = Image.open(f'{OUT}/icons/star_on_20.png').resize((22, 22), Image.LANCZOS)
x, y, w, h = B['favorite']
im.alpha_composite(Image.new('RGBA', (w - 20, h - 8), (24, 16, 32, 255)), (x + 10, y + 4))
im.alpha_composite(star, (x + w // 2 - 11, y + h // 2 - 11))
# cartridge picture at 2x, trimmed to the window
x, y, w, h = B['cartWindow']
d.rectangle([x, y, x + w - 1, y + h - 1], fill=(14, 10, 18))
art = Image.open(f'{OUT}/cartridges/cart_haunted_castle.png').convert('RGB')
trim = (96 - w // 2) // 2
art = art.crop((trim, 0, trim + w // 2, 96)).resize((w // 2 * 2, 192), Image.NEAREST)
ax, ay = B['cartArt'][0], B['cartArt'][1]
im.paste(art, (ax, ay))
# knobs
for (name, (cx, cy, w2, a)), v in zip(B['knobs'].items(), [0.5, 0.55, 0.2, 0.4]):
    knob(d, cx, cy, w2, a, v)
# carousel thumbnails (48 x 48 = exact half size), first one selected
order = ['haunted_castle', 'skeleton', 'haunted_organ', 'alien_saucer', 'night_racer', 'boss_dragon']
for i, (rect, k) in enumerate(zip(B['carouselSlots'], order)):
    x, y, w, h = rect
    th = Image.open(f'{OUT}/cartridges/cart_{k}.png').convert('RGB').resize((48, 48), Image.BOX)
    im.paste(th, (x + (w - 48) // 2, y + (h - 48) // 2))
    if i == 0:
        d.rounded_rectangle([x + 1, y + 1, x + w - 2, y + h - 2], radius=6, outline=P['mint'], width=3)
im.alpha_composite(strip(f'{OUT}/sprites/ghost_8x64x64.png', 8, 3, (48, 48)), (286, 92))
im.convert('RGB').save(f'{OUT}/preview_browser.png')
print('browser done')
