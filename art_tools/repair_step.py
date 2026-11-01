from PIL import Image, ImageDraw, ImageFont
import numpy as np
src = Image.open('/home/claude/art/02_BG_Step_Mode_Empty.png').convert('RGB')
im = src.copy()

# 1) SPOOK knob: replace the doubled well with a clean copy of the TONE well
tone_c, spook_c = (674, 669), (970, 669)
box = (tone_c[0]-78, 600, tone_c[0]+78, 712)
patch = src.crop(box)
# feathered mask so the panel texture blends at the edges
w, h = patch.size
yy, xx = np.mgrid[0:h, 0:w]
fx = np.minimum(xx, w-1-xx) / 10.0
fy = np.minimum(yy, h-1-yy) / 10.0
mask = Image.fromarray((np.clip(np.minimum(fx, fy), 0, 1) * 255).astype(np.uint8))
# first clear the old doubled ring under a wider area with panel texture
clean = src.crop((1040, 600, 1120, 712)).resize((200, 112))
im.paste(clean, (spook_c[0]-100, 600))
im.paste(patch, (spook_c[0]-78, 600), mask)

# 2) SPACE label: remove the pasted box, then build "SPACE" from the panel's own letters
fill = src.crop((1040, 712, 1120, 756))           # clean panel texture
for x in range(1146, 1402, 80):
    im.paste(fill, (x, 712))
letters = {'S': (917, 935), 'P': (941, 958), 'A': (411, 429), 'C': (387, 405), 'E': (365, 381)}
gap = 6
widths = [letters[c][1] - letters[c][0] + 1 for c in 'SPACE']
total = sum(widths) + gap * 4
x = 1261 - total // 2 - 22
for c in 'SPACE':
    a, b = letters[c]
    glyph = src.crop((a - 1, 712, b + 2, 754))
    im.paste(glyph, (x - 1, 712))
    x += (b - a + 1) + gap
# small caption, same lavender and mono face as the other pasted labels
d = ImageDraw.Draw(im)
f = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf', 13)
d.text((x + 6, 715), "MAX", font=f, fill=(206, 170, 228))
d.text((x + 6, 732), "20%", font=f, fill=(206, 170, 228))

im.save('bg_step_fixed_1536.png')
im.resize((1024, 600), Image.LANCZOS).save('bg_step.png')
im.crop((250, 580, 1420, 770)).save('/tmp/knobs_fixed.png')
