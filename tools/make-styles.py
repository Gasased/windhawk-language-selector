# Builds assets/styles.png (every display style, light + dark) from shots/modes-*.png (tools/modes.ps1).
from PIL import Image, ImageDraw, ImageFont
tiles = {
 'light': [(12,38,288,290),(600,38,1176,158),(12,328,132,524),(600,328,836,448),(12,562,288,702),(600,562,720,682)],
 'dark':  [(12,38,288,290),(600,38,1176,178),(12,328,132,504),(600,328,836,448),(12,542,288,682),(600,542,720,642)],
}
labels = ['List','List, horizontal','Codes only','Codes only, horizontal','Only the new language','Only the new language, code only']
def panel(im, box, dark):
    t = im.crop(box).convert('RGB'); w,h = t.size; px = t.load()
    def hit(x,y):
        r,g,b = px[x,y]
        return (r < 70 and abs(r-g) < 6 and abs(g-b) < 6) if dark else (205 < r < 243 and abs(r-g) < 5 and abs(g-b) < 5)
    def run(seq):  # longest unbroken stretch: panel edges are long, editor text is not
        best = cur = 0
        for v in seq: cur = cur + 1 if v else 0; best = max(best, cur)
        return best
    rows = [run(hit(x,y) for x in range(w)) for y in range(h)]
    cols = [run(hit(x,y) for y in range(h)) for x in range(w)]
    ry = [y for y,c in enumerate(rows) if c >= 30]; cx = [x for x,c in enumerate(cols) if c >= 20]
    x0,x1,y0,y1 = min(cx), max(cx), min(ry), max(ry)
    m = 12
    out = t.crop((max(0,x0-m), max(0,y0-m), min(w,x1+m+2), min(h,y1+m+4)))
    # the editor text sits right above the panel: blank the strip above its top edge
    top = y0 - max(0, y0-m) - 1
    ImageDraw.Draw(out).rectangle((0, 0, out.size[0], top - 1), fill=(249,249,249))
    return out
font = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 15)
fontb = ImageFont.truetype('C:/Windows/Fonts/seguisb.ttf', 19)
BG, FG, SUB = (243,243,243), (32,32,32), (96,96,96)
W, pad = 1180, 32
order = [[0,1],[2,3,4,5]]
sections = []
for theme in ['light','dark']:
    src = Image.open(f'shots/modes-{theme}.png')
    sections.append((theme, [panel(src, b, theme=='dark') for b in tiles[theme]]))
H = pad
for _, ps in sections: H += 40 + sum(max(ps[i].size[1] for i in r) + 44 for r in order) + 10
can = Image.new('RGB', (W, H), BG); d = ImageDraw.Draw(can); y = pad - 8
for theme, ps in sections:
    d.text((pad, y), 'Light theme' if theme == 'light' else 'Dark theme', font=fontb, fill=FG); y += 40
    for r in order:
        x = pad; rh = max(ps[i].size[1] for i in r)
        for i in r:
            p = ps[i]
            d.text((x, y), labels[i], font=font, fill=SUB)
            d.rounded_rectangle((x-1, y+23, x+p.size[0], y+24+p.size[1]), 6, outline=(222,222,222), fill=(249,249,249))
            can.paste(p, (x, y + 24))
            x += p.size[0] + 34
        y += rh + 44
    y += 10
can.save('assets/styles.png', optimize=True); print(can.size)
