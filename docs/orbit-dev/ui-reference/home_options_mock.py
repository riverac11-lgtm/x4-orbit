from PIL import Image, ImageDraw, ImageFont
W,H=480,800
BLK,DG,LG,WH=0,85,190,255
F="/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"; FB="/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
def f(sz,b=False): return ImageFont.truetype(FB if b else F,sz)
def base():
    im=Image.new('L',(W,H),WH); d=ImageDraw.Draw(im)
    d.text((W-70,14),"90%",font=f(14),fill=BLK); d.rectangle([W-34,17,W-14,29],outline=BLK,width=2); d.rectangle([W-31,20,W-19,26],fill=BLK)
    return im,d
def cover(d,x,y,w,h):
    d.rectangle([x,y,x+w,y+h],fill=DG)
    for i in range(0,w+h,9): d.line([x+max(0,i-h),y+min(h,i),x+min(w,i),y+max(0,i-w)],fill=LG,width=1)
    d.rectangle([x+8,y+h//2-16,x+w-8,y+h//2+16],fill=WH); d.text((x+w//2,y+h//2),"cover",font=f(13),fill=DG,anchor="mm")
    d.rectangle([x,y,x+w,y+h],outline=BLK,width=2)
def bar(d,x,y,w,pct,h=8):
    d.rounded_rectangle([x,y,x+w,y+h],radius=h//2,outline=BLK,width=2)
    d.rounded_rectangle([x,y,x+int(w*pct),y+h],radius=h//2,fill=BLK)
def hints(d,labels):
    xs=[40,140,250,350]
    for x,l in zip(xs,labels):
        if not l: continue
        d.rounded_rectangle([x,H-38,x+90,H-4],radius=6,outline=BLK,width=2); d.text((x+45,H-21),l,font=f(15),fill=BLK,anchor="mm")
def icon(d,kind,cx,cy,s=1.0):
    k=lambda v:int(v*s)
    if kind=="lib":
        for i,dx in enumerate((-14,-4,6)): d.rectangle([cx+k(dx),cy-k(16),cx+k(dx+8),cy+k(16)],outline=BLK,width=2)
        d.line([cx+k(16),cy-k(14),cx+k(22),cy+k(16)],fill=BLK,width=3)
    if kind=="cloud":
        d.ellipse([cx-k(20),cy-k(6),cx-k(2),cy+k(12)],outline=BLK,width=2); d.ellipse([cx-k(8),cy-k(16),cx+k(14),cy+k(8)],outline=BLK,width=2)
        d.ellipse([cx+k(4),cy-k(4),cx+k(22),cy+k(12)],outline=BLK,width=2); d.rectangle([cx-k(12),cy+k(2),cx+k(14),cy+k(11)],fill=WH)
        d.line([cx-k(12),cy+k(12),cx+k(14),cy+k(12)],fill=BLK,width=2); d.line([cx+k(1),cy-k(4),cx+k(1),cy+k(22)],fill=BLK,width=2)
        d.polygon([(cx-k(6),cy+k(15)),(cx+k(8),cy+k(15)),(cx+k(1),cy+k(23))],fill=BLK)
    if kind=="more":
        for dx in (-12,0,12): d.ellipse([cx+k(dx)-3,cy-3,cx+k(dx)+3,cy+3],fill=BLK)
    if kind=="set":
        for dy,px in ((-8,-6),(8,6)):
            d.line([cx-k(18),cy+k(dy),cx+k(18),cy+k(dy)],fill=BLK,width=2); d.ellipse([cx+k(px)-5,cy+k(dy)-5,cx+k(px)+5,cy+k(dy)+5],fill=WH,outline=BLK,width=2)
    if kind=="stats":
        for i,hh in enumerate((10,18,26)): d.rectangle([cx-k(14)+i*k(11),cy+k(13)-k(hh),cx-k(7)+i*k(11),cy+k(13)],fill=BLK)
    if kind=="mark":
        d.polygon([(cx-k(10),cy-k(15)),(cx+k(10),cy-k(15)),(cx+k(10),cy+k(15)),(cx,cy+k(7)),(cx-k(10),cy+k(15))],outline=BLK,width=2)

# A: hero card + 2x2 tiles
im,d=base()
d.text((24,48),"Continue reading",font=f(15),fill=DG)
d.rounded_rectangle([16,72,W-16,300],radius=14,outline=BLK,width=2)
cover(d,32,88,128,196)
d.text((176,96),"Black House",font=f(24,True),fill=BLK); d.text((176,130),"Stephen King",font=f(17),fill=BLK)
d.text((176,154),"The Talisman #2",font=f(15),fill=DG)
d.text((176,206),"41%",font=f(22,True),fill=BLK); d.text((236,211),"~1h 03m left",font=f(15),fill=DG)
bar(d,176,242,268,0.41,10); d.text((176,262),"Confirm to continue",font=f(13),fill=DG)
tiles=[("Library","lib"),("Online Library","cloud"),("More","more"),("Settings","set")]
for i,(t,ic) in enumerate(tiles):
    x=16+(i%2)*228; y=322+(i//2)*186; sel=(i==1)
    d.rounded_rectangle([x,y,x+220,y+172],radius=14,outline=BLK,width=3 if sel else 2,fill=LG if sel else WH)
    icon(d,ic,x+110,y+68,1.5); d.text((x+110,y+130),t,font=f(18,sel),fill=BLK,anchor="mm")
hints(d,["","Open","Left","Right"]); a=im

# B: big cover centrepiece + icon dock
im,d=base()
cover(d,140,46,200,300)
d.text((W//2,372),"Black House",font=f(26,True),fill=BLK,anchor="mm")
d.text((W//2,404),"Stephen King  ·  The Talisman #2",font=f(15),fill=DG,anchor="mm")
bar(d,60,430,360,0.41,10)
d.text((60,450),"41%",font=f(15,True),fill=BLK); d.text((420,450),"~1h 03m left",font=f(15),fill=DG,anchor="ra")
d.text((W//2,486),"Read 43m today",font=f(13),fill=DG,anchor="mm")
d.line([40,520,W-40,520],fill=LG,width=2)
dock=[("Library","lib"),("Online","cloud"),("More","more"),("Settings","set")]
for i,(t,ic) in enumerate(dock):
    cx=60+i*120; sel=(i==0)
    if sel: d.rounded_rectangle([cx-52,548,cx+52,690],radius=14,fill=LG)
    icon(d,ic,cx,598,1.3); d.text((cx,656),t,font=f(16,sel),fill=BLK,anchor="mm")
hints(d,["","Open","Left","Right"]); b=im

# C: refined list + recent shelf
im,d=base()
d.text((24,44),"Good morning",font=f(26,True),fill=BLK)
cover(d,24,92,96,144)
d.text((136,96),"Black House",font=f(21,True),fill=BLK); d.text((136,126),"Stephen King",font=f(15),fill=BLK)
d.text((136,148),"The Talisman #2",font=f(14),fill=DG)
bar(d,136,190,320,0.41,8); d.text((136,206),"41%  ·  ~1h 03m left",font=f(14),fill=DG)
d.text((24,262),"Recently added",font=f(15,True),fill=BLK); d.text((W-24,262),"See all ›",font=f(14),fill=DG,anchor="ra")
for i in range(4): cover(d,24+i*112,288,96,138)
rows=[("Library","lib"),("Online Library","cloud"),("More","more"),("Settings","set")]
for i,(t,ic) in enumerate(rows):
    y=452+i*72; sel=(i==1)
    if sel: d.rounded_rectangle([16,y,W-16,y+62],radius=10,fill=LG)
    icon(d,ic,52,y+31,1.0); d.text((88,y+31),t,font=f(19,sel),fill=BLK,anchor="lm"); d.text((W-32,y+31),"›",font=f(24),fill=DG,anchor="rm")
    if not sel and i<3: d.line([88,y+66,W-24,y+66],fill=LG,width=1)
hints(d,["","Open","Up","Down"]); c=im

out=Image.new('L',(3*W+4*40,H+120),235); od=ImageDraw.Draw(out)
for i,(img,t) in enumerate(((a,"A · Card + tiles"),(b,"B · Cover first"),(c,"C · List + recent shelf"))):
    x=40+i*(W+40); out.paste(img,(x,80)); od.rectangle([x-2,78,x+W+1,80+H+1],outline=120,width=2)
    od.text((x+W//2,40),t,font=f(30,True),fill=0,anchor="mm")
out.save('home_options.png'); print(out.size)
