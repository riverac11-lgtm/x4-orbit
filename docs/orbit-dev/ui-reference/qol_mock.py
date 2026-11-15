import math
exec(open('mock.py').read().split('# A: hero card')[0])
def hdr(d,t):
    d.text((24,48),t,font=f(24,True),fill=BLK); d.line([16,84,W-16,84],fill=BLK,width=2)
def wrap(d,x,y,text,font,maxw,fill=BLK,lh=None):
    words=text.split(); line=""; lh=lh or font.size+8
    for w in words:
        t=(line+" "+w).strip()
        if d.textlength(t,font=font)>maxw: d.text((x,y),line,font=font,fill=fill); y+=lh; line=w
        else: line=t
    d.text((x,y),line,font=font,fill=fill); return y+lh
def page_bg(d):
    y=110
    for i in range(22):
        w=W-60 if i%7!=6 else 260; d.line([30,y,30+w,y],fill=LG,width=6); y+=28
def dialog(d,y0,y1):
    d.rounded_rectangle([24,y0,W-24,y1],radius=16,fill=WH,outline=BLK,width=3)
def button(d,x,y,w,t,sel=False):
    d.rounded_rectangle([x,y,x+w,y+48],radius=10,fill=LG if sel else WH,outline=BLK,width=2); d.text((x+w//2,y+24),t,font=f(17,sel),fill=BLK,anchor="mm")

shots=[]
# 1 Audiobook catch-up
im,d=base(); page_bg(d); dialog(d,250,590)
d.text((W//2,290),"Catch up with your audiobook",font=f(19,True),fill=BLK,anchor="mm")
d.text((W//2,326),"Audiobookshelf · Black House",font=f(14),fill=DG,anchor="mm")
d.rounded_rectangle([56,352,W-56,442],radius=10,fill=LG)
d.text((76,368),"You're further along in the audio:",font=f(14),fill=BLK)
d.text((76,394),"Chapter 23 · 58%",font=f(22,True),fill=BLK); d.text((76,422),"here: Chapter 19 · 41%",font=f(13),fill=DG)
bar(d,56,462,W-112,0.58,10); d.line([56+int((W-112)*0.41),456,56+int((W-112)*0.41),478],fill=BLK,width=3)
button(d,48,512,180,"Jump there",True); button(d,252,512,180,"Stay here")
shots.append((im,"1 · Audiobook catch-up"))

# 2 Next in series
im,d=base(); hdr(d,"Book finished!")
cover(d,40,110,120,180); d.text((176,120),"Black House",font=f(20,True),fill=BLK); d.text((176,150),"Read in 9 days",font=f(15),fill=DG)
d.text((176,176),"23h 40m total",font=f(15),fill=DG)
d.text((24,320),"Next in the series",font=f(16,True),fill=BLK)
d.rounded_rectangle([16,348,W-16,520],radius=14,outline=BLK,width=2)
cover(d,32,362,96,144); d.text((144,372),"The Talisman #3",font=f(19,True),fill=BLK); d.text((144,402),"Not on this reader yet",font=f(14),fill=DG)
d.text((144,424),"Found in BookOrbit",font=f(14),fill=DG); button(d,144,456,300,"Download & open",True)
button(d,24,560,210,"Move to Completed"); button(d,246,560,210,"Back to Home")
shots.append((im,"2 · Next in series"))

# 3 Auto-delivery while charging
im,d=base(); hdr(d,"Home")
d.rounded_rectangle([16,108,W-16,232],radius=14,fill=LG)
d.text((36,124),"While you were charging",font=f(15),fill=DG)
d.text((36,150),"2 new books arrived",font=f(22,True),fill=BLK)
d.text((36,186),"from your “Send to X4” shelf",font=f(14),fill=DG)
for i in range(2): cover(d,300+i*76,120,64,98)
for i,(t,ic) in enumerate([("Library","lib"),("Online Library","cloud"),("More","more"),("Settings","set")]):
    y=270+i*72; icon(d,ic,52,y+31,1.0); d.text((88,y+31),t,font=f(19),fill=BLK,anchor="lm")
d.text((24,600),"Settings › System › Auto-delivery",font=f(13),fill=DG)
d.text((24,622),"Only on the charger · Wi-Fi off when done",font=f(13),fill=DG)
shots.append((im,"3 · Auto-delivery on charger"))

# 4 Daily goal + calendar
im,d=base(); hdr(d,"Reading")
cx,cy,r=130,200,70
d.arc([cx-r,cy-r,cx+r,cy+r],0,360,fill=LG,width=14); d.arc([cx-r,cy-r,cx+r,cy+r],-90,-90+int(360*0.6),fill=BLK,width=14)
d.text((cx,cy-10),"18",font=f(30,True),fill=BLK,anchor="mm"); d.text((cx,cy+22),"of 30 min",font=f(13),fill=DG,anchor="mm")
d.text((236,150),"Today",font=f(16,True),fill=BLK); d.text((236,176),"12 min to your goal",font=f(15),fill=DG)
d.text((236,214),"6-day streak",font=f(18,True),fill=BLK)
d.text((24,300),"November",font=f(16,True),fill=BLK)
for i,c in enumerate("MTWTFSS"): d.text((44+i*62,334),c,font=f(13),fill=DG,anchor="mm")
import random; random.seed(4)
for day in range(30):
    col=(day+5)%7; row=(day+5)//7; x=26+col*62; y=350+row*58
    v=random.choice([0,0,1,2,3]) if day<14 else -1
    fill={-1:WH,0:WH,1:LG,2:DG,3:BLK}[v]
    d.rounded_rectangle([x,y,x+36,y+36],radius=6,fill=fill,outline=LG if v<=0 else fill,width=2)
    d.text((x+18,y+18),str(day+1),font=f(12),fill=WH if v>=2 else DG,anchor="mm")
shots.append((im,"4 · Daily goal + calendar"))

# 5 Highlights -> quote sleep screen
im,d=base(); im.paste(0,[0,0,W,H]); d=ImageDraw.Draw(im)
for i in range(40):
    random.seed(i); x=random.randint(10,W-10); y=random.randint(10,H-10); d.rectangle([x,y,x+2,y+2],fill=WH)
d.text((W//2,210),"“",font=f(90,True),fill=WH,anchor="mm")
yy=wrap(d,60,270,"A line you highlighted shows up here, a different one each time the reader sleeps.",f(26),W-120,fill=WH,lh=40)
d.line([60,yy+20,140,yy+20],fill=WH,width=2)
d.text((60,yy+40),"Book title · Chapter",font=f(16),fill=WH)
d.text((60,yy+66),"your highlight · 41%",font=f(14),fill=LG)
d.text((W//2,H-40),"Sleeping",font=f(14),fill=LG,anchor="mm")
shots.append((im,"5 · Highlights as sleep screen"))

# 6 Battery estimate + chapter ticks
im,d=base(); page_bg(d)
d.rectangle([0,H-62,W,H],fill=WH); d.line([24,H-60,W-24,H-60],fill=LG,width=1)
x0,x1=24,W-24; d.line([x0,H-40,x1,H-40],fill=LG,width=4); d.line([x0,H-40,x0+int((x1-x0)*0.41),H-40],fill=BLK,width=4)
for t in (0.08,0.17,0.29,0.36,0.52,0.63,0.71,0.85,0.93): d.line([x0+int((x1-x0)*t),H-46,x0+int((x1-x0)*t),H-34],fill=BLK,width=2)
d.text((24,H-26),"Ch. 19 · 4 pages left",font=f(13),fill=DG); d.text((W//2,H-26),"41%",font=f(13,True),fill=BLK,anchor="ma")
d.text((W-24,H-26),"~9 days of reading",font=f(13),fill=DG,anchor="ra")
d.text((W-84,14),"~9 days ·",font=f(14),fill=DG,anchor="ra")
shots.append((im,"6 · Battery days + chapter ticks"))

out=Image.new('L',(3*W+4*40,2*(H+120)),235); od=ImageDraw.Draw(out)
for i,(img,t) in enumerate(shots):
    x=40+(i%3)*(W+40); y=80+(i//3)*(H+120); out.paste(img,(x,y)); od.rectangle([x-2,y-2,x+W+1,y+H+1],outline=120,width=2)
    od.text((x+W//2,y-40),t,font=f(28,True),fill=0,anchor="mm")
out.save('qol_options.png'); print(out.size)
