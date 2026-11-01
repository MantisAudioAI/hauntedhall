from PIL import Image, ImageDraw
import json,sys
def overlay(bg, rects, out, scale=2):
    im=Image.open(bg).convert('RGB').resize((1024*scale,600*scale),Image.NEAREST)
    d=ImageDraw.Draw(im)
    for name,(x,y,w,h) in rects:
        d.rectangle([x*scale,y*scale,(x+w)*scale-1,(y+h)*scale-1],outline=(255,40,40),width=1)
    im.save(out)
