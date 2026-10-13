# Renders assets/settings/*.gif: one animation per settings group, showing what the
# settings do. The panel is redrawn here from the mod's own geometry (row height,
# paddings, accent mark, shadow, animations), not recorded, so values can be tweened.
# Usage: python tools/make-setting-gifs.py [name ...]   (no names = all)
import math, os, shutil, subprocess, sys, tempfile
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont
# Frames are opaque RGB: Pillow blends translucent drawing only onto RGB images.

S = 2                      # supersampling; frames are drawn at S x and scaled down
W, H = 760, 420            # GIF size
FPS = 20
OUT = os.path.join(os.path.dirname(__file__), '..', 'assets', 'settings')

F_UI, F_SB, F_MONO, F_ICON = ('C:/Windows/Fonts/segoeui.ttf', 'C:/Windows/Fonts/seguisb.ttf',
                              'C:/Windows/Fonts/CascadiaMono.ttf', 'C:/Windows/Fonts/SegoeIcons.ttf')
F_CJK = ('C:/Windows/Fonts/YuGothM.ttc', 1)    # Yu Gothic UI: what Windows falls back to for Japanese
F_CJK_SB = ('C:/Windows/Fonts/YuGothB.ttc', 2)
_fonts = {}
def font(path, size, text=''):
    if any(ord(c) >= 0x3000 for c in text):
        path = F_CJK_SB if path == F_SB else F_CJK
    k = (path, max(1, round(size * S)))
    if k not in _fonts:
        _fonts[k] = ImageFont.truetype(path[0], k[1], index=path[1]) if isinstance(path, tuple) else ImageFont.truetype(path, k[1])
    return _fonts[k]
def X(v): return int(round(v * S))

# ---------------------------------------------------------------- easing / timeline
def clamp(v, a=0.0, b=1.0): return max(a, min(b, v))
def seg(t, a, b): return clamp((t - a) / (b - a)) if b > a else float(t >= a)
def ease(p): return 1 - (1 - p) ** 3                    # the mod's EaseOut
def smooth(p): return p * p * (3 - 2 * p)
def lerp(a, b, p): return a + (b - a) * p
def mix(c1, c2, p): return tuple(int(round(lerp(a, b, p))) for a, b in zip(c1, c2))
def hexrgb(h): return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))

# ---------------------------------------------------------------- the panel (mod geometry)
ITEMS = [dict(code='ENG', name='English (United States)', native='English (United States)', layout='US'),
         dict(code='УКР', name='Ukrainian', native='Українська', layout='Ukrainian (Enhanced)'),
         dict(code='日本語', name='Japanese', native='日本語', layout='Japanese')]
LINE1, LINE2, LINE_GAP, MARGIN = 20, 16, 2, 18
DEF = dict(theme='light', accent=None, horizontal=False, only_new=False, reverse=False, abbr=True, name=True,
           layout=True, native=False, scale=100, opacity=97, radius=8, shadow=True, pad=4, padx=16, pady=8,
           gap=12, itemgap=2, sel=1.0)

def P(**kw):
    p = dict(DEF); p.update(kw); return p

def tw(text, size):  # text width in DIPs
    return font(F_UI, size, text).getlength(text) / S

def accent_of(p):
    if p['accent']: return p['accent'] if isinstance(p['accent'], tuple) else hexrgb(p['accent'])
    return (0x60, 0xCD, 0xFF) if p['theme'] == 'dark' else (0x00, 0x5F, 0xB8)

def geometry(p):
    abbr, name, lay = p['abbr'], p['name'], p['layout']
    if not (abbr or name or lay): abbr = True
    codes = abbr and not name and not lay
    allitems = [dict(it, label=it['native'] if p['native'] else it['name']) for it in ITEMS]
    if p['only_new']: items = [allitems[int(round(p['sel'])) % 3]]
    else: items = allitems[::-1] if p['reverse'] else allitems
    abbr_col = max(tw(it['code'], 12) for it in allitems)
    code_col = max(tw(it['code'], 14) for it in allitems)
    def text_w(it):
        return max(tw(it['label'], 14) if name else 0, tw(it['layout'], 12 if name else 14) if lay else 0)
    row_h = p['pady'] * 2 + (LINE1 + LINE_GAP + LINE2 if name and lay else LINE1)
    def item_w(it):
        if codes: return p['padx'] * 2 + code_col
        return p['padx'] * 2 + (abbr_col + (p['gap'] if name or lay else 0) if abbr else 0) + text_w(it)
    n, pad, ig = len(items), p['pad'], p['itemgap']
    if p['horizontal'] and not p['only_new']:
        ws = [item_w(it) for it in items]
        Wp, Hp = pad * 2 + sum(ws) + ig * (n - 1), pad * 2 + row_h
        rects, x = [], pad
        for w in ws: rects.append((x, pad, x + w, pad + row_h)); x += w + ig
    else:
        Wp = pad * 2 + max(item_w(it) for it in (allitems if p['only_new'] else items))
        Hp = pad * 2 + row_h * n + ig * (n - 1)
        rects = [(pad, pad + i * (row_h + ig), Wp - pad, pad + i * (row_h + ig) + row_h) for i in range(n)]
    return dict(W=Wp, H=Hp, rects=rects, items=items, codes=codes, abbr=abbr, name=name, lay=lay,
                abbr_col=abbr_col, code_col=code_col, row_h=row_h)

def render_panel(p):
    """RGBA image of the panel with its shadow margin; scale in px per DIP = S * scale/100."""
    g = geometry(p); k = S * p['scale'] / 100
    dark = p['theme'] == 'dark'
    m = MARGIN
    iw, ih = int(math.ceil((g['W'] + 2 * m) * k)), int(math.ceil((g['H'] + 2 * m) * k))
    def R(v): return v * k
    r = p['radius']
    box = (R(m), R(m), R(m + g['W']) - 1, R(m + g['H']) - 1)
    bg = (0x2C, 0x2C, 0x2C) if dark else (0xF9, 0xF9, 0xF9)
    body = Image.new('RGB', (iw, ih), bg)          # colors, blended onto the opaque fill
    ink = Image.new('L', (iw, ih), 0)              # text + accent mark: stay opaque whatever the opacity
    d, di = ImageDraw.Draw(body, 'RGBA'), ImageDraw.Draw(ink)
    shape = Image.new('L', (iw, ih), 0)
    ImageDraw.Draw(shape).rounded_rectangle(box, R(r), fill=255)
    d.rounded_rectangle(box, R(r), outline=(255, 255, 255, 20) if dark else (0, 0, 0, 16), width=max(1, int(R(1))))
    fg = (255, 255, 255) if dark else (0x1A, 0x1A, 0x1A)
    acc = accent_of(p)
    ox, oy = R(m), R(m)
    ir = max(0, r - p['pad'] + 1)
    rects = g['rects']
    def text(xy, t, size, alpha, anchor):
        f = font(F_UI, size * p['scale'] / 100, t)
        d.text(xy, t, font=f, fill=fg + (alpha,), anchor=anchor); di.text(xy, t, font=f, fill=alpha, anchor=anchor)
    if not p['only_new']:
        s_ = p['sel'] if not p['reverse'] else (len(rects) - 1 - p['sel'])
        i0 = int(math.floor(s_)) % len(rects); i1 = min(i0 + 1, len(rects) - 1); f = s_ - math.floor(s_)
        rc = tuple(lerp(a, b, f) for a, b in zip(rects[i0], rects[i1]))
        d.rounded_rectangle((ox + R(rc[0]), oy + R(rc[1]), ox + R(rc[2]), oy + R(rc[3])), R(ir),
                            fill=(255, 255, 255, 18) if dark else (0, 0, 0, 13))
        cx, cy = (rc[0] + rc[2]) / 2, (rc[1] + rc[3]) / 2
        pill = (cx - 8, rc[3] - 4, cx + 8, rc[3] - 1) if p['horizontal'] else (rc[0], cy - 8, rc[0] + 3, cy + 8)
        pb = tuple(o + R(v) for o, v in zip((ox, oy, ox, oy), pill))
        d.rounded_rectangle(pb, R(1.5), fill=acc + (255,)); di.rounded_rectangle(pb, R(1.5), fill=255)
    for it, rc in zip(g['items'], rects):
        if g['codes']:
            lift = 1 if p['horizontal'] and not p['only_new'] else 0
            text((ox + R((rc[0] + rc[2]) / 2), oy + R((rc[1] + rc[3]) / 2 - lift)), it['code'], 14, 255, 'mm')
            continue
        x, top = rc[0] + p['padx'], rc[1] + p['pady']
        l1, l2 = top + LINE1 / 2, top + LINE1 + LINE_GAP + LINE2 / 2
        if g['abbr']:
            text((ox + R(x), oy + R(l1)), it['code'], 12, 230, 'lm'); x += g['abbr_col'] + p['gap']
        if g['name']:
            text((ox + R(x), oy + R(l1)), it['label'], 14, 255, 'lm')
        if g['lay']:
            if g['name']: text((ox + R(x), oy + R(l2)), it['layout'], 12, 158, 'lm')
            else: text((ox + R(x), oy + R(l1)), it['layout'], 14, 255, 'lm')
    alpha = ImageChops.lighter(shape.point(lambda a: int(a * p['opacity'] / 100)), ImageChops.multiply(ink, shape))
    panel = body.convert('RGBA'); panel.putalpha(alpha)
    out = Image.new('RGBA', (iw, ih), (0, 0, 0, 0))
    if p['shadow']:
        mask = Image.new('L', (iw, ih), 0)
        ImageDraw.Draw(mask).rounded_rectangle((box[0], box[1] + R(3), box[2], box[3] + R(3)), R(r), fill=255)
        out.putalpha(mask.filter(ImageFilter.GaussianBlur(R(6))).point(lambda a: int(a * (0.34 if dark else 0.22))))
    out.alpha_composite(panel)
    return out, g, k

def place(caret, p, g, k, side='below', wa=None, offx=0, offy=4):
    """Panel top-left (canvas units, without margin) next to caret (x, top, bottom): the mod's PlaceNear."""
    cx, ct, cb = caret
    w, h = g['W'] * k / S, g['H'] * k / S
    wa = wa or (24, 60, W - 24, H - 24)
    gap = 4
    if side == 'below' and cb + gap + offy + h > wa[3]: side = 'above'
    elif side == 'above' and ct - gap - offy - h < wa[1]: side = 'below'
    elif side == 'right' and cx + gap + offx + w > wa[2]: side = 'left'
    elif side == 'left' and cx - gap - offx - w < wa[0]: side = 'right'
    inset = (p['pad'] + p['padx']) * k / S
    midy = (ct + cb - h) / 2 + offy
    x, y = {'below': (cx - inset + offx, cb + gap + offy), 'above': (cx - inset + offx, ct - gap - offy - h),
            'right': (cx + gap + offx, midy), 'left': (cx - gap - offx - w, midy)}[side]
    em = 8
    return max(wa[0] + em, min(x, wa[2] - em - w)), max(wa[1] + em, min(y, wa[3] - em - h)), side

def blit_panel(img, panel_img, k, pos, e=1.0, anim='slide', side='below', out=False, slide=8):
    """Draws the panel at pos (canvas units) at animation progress e (the mod's Render())."""
    if e <= 0: return
    e2 = e * e if out else ease(e)
    pim = panel_img
    x, y = X(pos[0]) - MARGIN * k, X(pos[1]) - MARGIN * k
    if anim == 'zoom':
        z = 0.9 + 0.1 * e2
        nw, nh = max(1, int(pim.width * z)), max(1, int(pim.height * z))
        cxp = {'below': (pim.width / 2, MARGIN * k), 'above': (pim.width / 2, pim.height - MARGIN * k),
               'right': (MARGIN * k, pim.height / 2), 'left': (pim.width - MARGIN * k, pim.height / 2)}[side]
        x, y = x + cxp[0] * (1 - z), y + cxp[1] * (1 - z)
        pim = pim.resize((nw, nh), Image.LANCZOS)
    if anim == 'slide':
        dd = (1 - e2) * slide * S
        x += {'right': -dd, 'left': dd}.get(side, 0); y += {'below': -dd, 'above': dd}.get(side, 0)
    if anim != 'none' and e2 < 1:
        a = pim.getchannel('A').point(lambda v: int(v * e2)); pim = pim.copy(); pim.putalpha(a)
    img.paste(pim, (int(round(x)), int(round(y))), pim)

# ---------------------------------------------------------------- scenery
GRAD = ((0xDC, 0xE6, 0xF5), (0xF0, 0xE2, 0xF1))
GRAD_DARK = ((0x1B, 0x22, 0x33), (0x2A, 0x1C, 0x2E))
_grad = {}
def backdrop(dark=0.0):
    key = round(dark, 2)
    if key not in _grad:
        g = Image.new('RGB', (2, 2)); c1 = [mix(a, b, dark) for a, b in zip(GRAD, GRAD_DARK)]
        g.putpixel((0, 0), c1[0]); g.putpixel((1, 0), mix(c1[0], c1[1], .5)); g.putpixel((0, 1), mix(c1[0], c1[1], .5)); g.putpixel((1, 1), c1[1])
        _grad[key] = g.resize((W * S, H * S), Image.BILINEAR)
    return _grad[key].copy()

CARD = (16, 16, W - 16, H - 16)
def window(img, dark=0.0, title=None, card=CARD):
    d = ImageDraw.Draw(img, 'RGBA')
    sh = Image.new('L', img.size, 0)
    ImageDraw.Draw(sh).rounded_rectangle([X(v) for v in (card[0], card[1] + 4, card[2], card[3] + 4)], X(10), fill=70)
    img.paste((0, 0, 0), (0, 0), sh.filter(ImageFilter.GaussianBlur(X(10))))
    d.rounded_rectangle([X(v) for v in card], X(10), fill=mix((0xF9, 0xF9, 0xF9), (0x20, 0x20, 0x20), dark) + (255,),
                        outline=mix((0xDD, 0xDD, 0xDD), (0x40, 0x40, 0x40), dark) + (255,), width=X(1))
    d.line([X(card[0]), X(card[1] + 44), X(card[2]), X(card[1] + 44)], fill=mix((0xE8, 0xE8, 0xE8), (0x33, 0x33, 0x33), dark) + (255,), width=X(1))
    ic = mix((0x60, 0x60, 0x60), (0xB0, 0xB0, 0xB0), dark) + (255,)
    for i, gl in enumerate(('\uE921', '\uE922', '\uE8BB')):
        d.text((X(card[2] - 104 + i * 36), X(card[1] + 22)), gl, font=font(F_ICON, 10), fill=ic, anchor='mm')

def text_line(img, x, y, text, dark=0.0, size=17):
    ImageDraw.Draw(img, 'RGBA').text((X(x), X(y)), text, font=font(F_MONO, size),
                                     fill=mix((0x22, 0x22, 0x22), (0xE6, 0xE6, 0xE6), dark) + (255,), anchor='lm')
    return x + font(F_MONO, size).getlength(text) / S

def caret(img, x, y, t, dark=0.0, h=24, always=False):
    """Blinking caret centered on y; returns its rect (x, top, bottom)."""
    if always or (t % 1.06) < 0.53:
        ImageDraw.Draw(img).rectangle([X(x), X(y - h / 2), X(x) + max(1, X(1.2)) - 1, X(y + h / 2)],
                                      fill=mix((0x11, 0x11, 0x11), (0xF0, 0xF0, 0xF0), dark))
    return (x, y - h / 2, y + h / 2)

def label(img, title, value, t, t_changed, dark=0.0, x=None, y=None, prev=None, swatch=None):
    """Setting chip in the title bar: 'Title  Value', the value rolls in when it changes."""
    x = CARD[0] + 18 if x is None else x; y = CARD[1] + 22 if y is None else y
    d = ImageDraw.Draw(img, 'RGBA')
    ft, fv = font(F_UI, 13), font(F_SB, 13)
    tw_ = ft.getlength(title) / S
    vw = max(fv.getlength(value) / S, fv.getlength(prev) / S if prev else 0)
    sw = 22 if swatch else 0
    box = (x, y - 14, x + 12 + tw_ + 10 + vw + 14 + sw, y + 14)
    d.rounded_rectangle([X(v) for v in box], X(14), fill=mix((0xEE, 0xF1, 0xF6), (0x2E, 0x2E, 0x34), dark) + (255,))
    d.text((X(x + 12), X(y)), title, font=ft, fill=mix((0x5A, 0x5A, 0x66), (0xA8, 0xA8, 0xB4), dark) + (255,), anchor='lm')
    p = smooth(seg(t, t_changed, t_changed + 0.28))
    vx = x + 12 + tw_ + 10
    if swatch:
        d.ellipse([X(vx), X(y - 6), X(vx + 12), X(y + 6)], fill=swatch + (255,)); vx += sw
    col = mix((0x1A, 0x1A, 0x1A), (0xFF, 0xFF, 0xFF), dark)
    layer = Image.new('RGBA', img.size, (0, 0, 0, 0)); ld = ImageDraw.Draw(layer)
    if prev is not None and p < 0.5:
        q = p * 2
        ld.text((X(vx), X(y - 8 * q)), prev, font=font(F_SB, 13, prev), fill=col + (int(255 * (1 - q)),), anchor='lm')
    if prev is None or p >= 0.5:
        q = 1.0 if prev is None else (p - 0.5) * 2
        ld.text((X(vx), X(y + 8 * (1 - q))), value, font=font(F_SB, 13, value), fill=col + (int(255 * q),), anchor='lm')
    clip = Image.new('L', img.size, 0); ImageDraw.Draw(clip).rectangle([X(v) for v in box], fill=255)
    img.paste(layer, (0, 0), ImageChops.multiply(layer.getchannel('A'), clip))
    return box

def chip_text(img, x, y, text, dark=0.0, size=12, color=None, anchor='lm'):
    ImageDraw.Draw(img, 'RGBA').text((X(x), X(y)), text, font=font(F_UI, size, text),
                                     fill=(color or mix((0x66, 0x66, 0x70), (0xA0, 0xA0, 0xAA), dark)) + (255,), anchor=anchor)

def toggle(img, x, y, on, name, dark=0.0, hl=0.0, acc=(0x00, 0x5F, 0xB8)):
    """Windows 11 style toggle switch, on in 0..1 (animated), with a label to its right."""
    d = ImageDraw.Draw(img, 'RGBA')
    if hl > 0:
        d.rounded_rectangle([X(x - 10), X(y - 17), X(x + 250), X(y + 17)], X(6), fill=acc + (int(26 * hl),))
    off_c = mix((0x8A, 0x8A, 0x8A), (0xA0, 0xA0, 0xA0), dark)
    d.rounded_rectangle([X(x), X(y - 10), X(x + 40), X(y + 10)], X(10),
                        fill=acc + (int(255 * on),), outline=mix(off_c, acc, on) + (255,), width=X(1))
    kx = lerp(x + 10, x + 30, ease(on)); kr = lerp(6, 7, on)
    d.ellipse([X(kx - kr), X(y - kr), X(kx + kr), X(y + kr)], fill=mix(off_c, (255, 255, 255), on) + (255,))
    chip_text(img, x + 52, y, name, dark, 13, mix((0x22, 0x22, 0x22), (0xEE, 0xEE, 0xEE), dark))

def slider(img, x, y, w, frac, name, value, dark=0.0, hl=0.0, acc=(0x00, 0x5F, 0xB8)):
    d = ImageDraw.Draw(img, 'RGBA')
    if hl > 0:
        d.rounded_rectangle([X(x - 10), X(y - 26), X(x + w + 10), X(y + 16)], X(6), fill=acc + (int(26 * hl),))
    chip_text(img, x, y - 13, name, dark, 12.5, mix((0x22, 0x22, 0x22), (0xEE, 0xEE, 0xEE), dark))
    chip_text(img, x + w, y - 13, value, dark, 12.5, mix((0x55, 0x55, 0x55), (0xBB, 0xBB, 0xBB), dark), anchor='rm')
    d.rounded_rectangle([X(x), X(y + 3), X(x + w), X(y + 7)], X(2), fill=mix((0xC8, 0xC8, 0xC8), (0x55, 0x55, 0x55), dark) + (255,))
    d.rounded_rectangle([X(x), X(y + 3), X(x + w * frac), X(y + 7)], X(2), fill=acc + (255,))
    kx = x + w * frac
    d.ellipse([X(kx - 9), X(y - 4), X(kx + 9), X(y + 14)], fill=(255, 255, 255, 255), outline=mix((0xCC, 0xCC, 0xCC), (0x66, 0x66, 0x66), dark) + (255,), width=X(1))
    d.ellipse([X(kx - 5 - hl), X(y - hl), X(kx + 5 + hl), X(y + 10 + hl)], fill=acc + (255,))

def keycap(img, x, y, text, down, w=None, dark=0.0):
    w = w or max(44, font(F_SB, 13, text).getlength(text) / S + 26)
    d = ImageDraw.Draw(img, 'RGBA'); dy = 3 * down
    d.rounded_rectangle([X(x), X(y + 3), X(x + w), X(y + 37)], X(7), fill=mix((0xB8, 0xBE, 0xC8), (0x15, 0x15, 0x18), dark) + (255,))
    d.rounded_rectangle([X(x), X(y + dy), X(x + w), X(y + 34 + dy)], X(7),
                        fill=mix(mix((0xFF, 0xFF, 0xFF), (0x3A, 0x3A, 0x40), dark), (0xE2, 0xEC, 0xFA), down * (1 - dark)) + (255,),
                        outline=mix((0xC9, 0xCE, 0xD6), (0x55, 0x55, 0x5C), dark) + (255,), width=X(1))
    d.text((X(x + w / 2), X(y + 17 + dy)), text, font=font(F_SB, 13, text), fill=mix((0x30, 0x30, 0x30), (0xEE, 0xEE, 0xEE), dark) + (255,), anchor='mm')
    return w

def finish(img): return img.resize((W, H), Image.LANCZOS)

def write_gif(name, frame_fn, duration, fps=FPS):
    tmp = tempfile.mkdtemp(prefix='gif-')
    n = int(round(duration * fps))
    for i in range(n):
        frame_fn(i / fps).save(os.path.join(tmp, f'f{i:04d}.png'))
    os.makedirs(OUT, exist_ok=True)
    dst = os.path.join(OUT, name + '.gif')
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-framerate', str(fps), '-i', os.path.join(tmp, 'f%04d.png'),
                    '-vf', 'split[a][b];[a]palettegen=max_colors=256:stats_mode=full[p];[b][p]paletteuse=dither=none:diff_mode=rectangle',
                    dst], check=True)
    shutil.rmtree(tmp)
    print(f'{name}.gif  {n} frames  {os.path.getsize(dst) // 1024} KB')

# ---------------------------------------------------------------- scenes
def scene_placement():
    """Placement relative to the text cursor, and the flip near the edge."""
    sides = ['below', 'above', 'right', 'left']
    names = {'below': 'Below', 'above': 'Above', 'right': 'Right', 'left': 'Left'}
    beat = 1.7
    p = P(layout=False)   # one-line entries: fits both above and below the caret in this frame
    pim, g, k = render_panel(p)
    def frame(t):
        img = backdrop(); window(img)
        i = int(t // beat)
        if i < 4:
            side, tt = sides[i], t - i * beat
            x = text_line(img, 300, 228, 'Hello world')
            c = caret(img, x, 228, t)
            label(img, 'Placement', names[side], t, i * beat, prev=names[sides[i - 1]] if i else None)
        elif i == 4:
            side, tt = 'below', t - 4 * beat
            x = text_line(img, 300, 150, 'Hello world')
            c = caret(img, x, 150, t)
            offy = lerp(4, 24, smooth(seg(tt, 0.5, 0.9))) if tt < 1.2 else lerp(24, 4, smooth(seg(tt, 1.2, 1.5)))
            label(img, 'Vertical offset', f'{offy:.0f} px', t, 4 * beat, prev=None)
            px, py, real = place(c, p, g, k, side, offy=offy)
            e = seg(tt, 0.1, 0.3) if tt < beat - 0.3 else 1 - seg(tt, beat - 0.3, beat - 0.05)
            blit_panel(img, pim, k, (px, py), e, 'slide', real, out=tt >= beat - 0.3, slide=10)
            if e > 0.5:
                d = ImageDraw.Draw(img, 'RGBA')
                d.rectangle([X(c[0] - 30), X(c[2]), X(c[0] + 30), X(py)], fill=(0xE3, 0x00, 0x8C, 90), outline=(0xE3, 0x00, 0x8C, 255), width=X(1))
            return finish(img)
        else:
            side, tt = 'below', t - 5 * beat
            text_line(img, 56, 90, 'Near the bottom of the window,')
            x = text_line(img, 56, 370, 'there is no room below')
            c = caret(img, x, 370, t)
            label(img, 'Placement', 'Below, flips up at the edge', t, 5 * beat, prev=None)
        px, py, real = place(c, p, g, k, side)
        e = seg(tt, 0.25, 0.25 + 0.2) if tt < beat - 0.45 else 1 - seg(tt, beat - 0.45, beat - 0.17)
        blit_panel(img, pim, k, (px, py), e, 'slide', real, out=tt >= beat - 0.45, slide=10)
        return finish(img)
    return frame, beat * 6

def scene_animation():
    """The four animations in a 2x2 grid, slowed down 2x."""
    anims = [('slide', 'Fade and slide'), ('zoom', 'Fade and zoom'), ('fade', 'Fade'), ('none', 'None')]
    p = P(name=False, layout=False, horizontal=True, scale=130)
    pim, g, k = render_panel(p)
    x0s, y0s = (CARD[0] + 14, (CARD[0] + CARD[2]) / 2 + 4), (CARD[1] + 56, (CARD[1] + 56 + CARD[3]) / 2 + 2)
    cw, ch = (CARD[2] - CARD[0]) / 2 - 18, (CARD[3] - CARD[1] - 56) / 2 - 10
    cyc = 2.4
    def frame(t):
        img = backdrop(); window(img)
        label(img, 'Animation', 'every style, 2× slower', t, 0)
        d = ImageDraw.Draw(img, 'RGBA')
        tt = t % cyc
        for i, (a, nm) in enumerate(anims):
            x0, y0 = x0s[i % 2], y0s[i // 2]
            cell = (x0, y0, x0 + cw, y0 + ch)
            d.rounded_rectangle([X(v) for v in cell], X(8), fill=(255, 255, 255, 255), outline=(0xE4, 0xE4, 0xE4, 255), width=X(1))
            chip_text(img, x0 + cw - 14, y0 + 18, nm, size=13, color=(0x33, 0x33, 0x33), anchor='rm')
            x = text_line(img, x0 + 22, y0 + 30, 'Hi ', size=16)
            c = caret(img, x, y0 + 30, t, h=22)
            px, py, side = place(c, p, g, k, 'below', wa=cell)
            fin, hold = 0.28, 1.0
            if tt < 0.3: continue
            if tt < 0.3 + fin + hold: e, out = seg(tt, 0.3, 0.3 + fin), False
            else: e, out = 1 - seg(tt, 0.3 + fin + hold, 0.3 + fin + hold + 0.44), True
            blit_panel(img, pim, k, (px, py), e, a, side, out=out, slide=16)
        return finish(img)
    return frame, cyc * 3

def scene_layout():
    """List direction, Show only the new language, Reverse order; the highlight glides between languages."""
    states = [('List direction', 'Vertical', P()), ('List direction', 'Horizontal', P(horizontal=True)),
              ('Show only the new language', 'On', P(only_new=True)), ('Reverse language order', 'On', P(reverse=True))]
    beat = 2.6
    def frame(t):
        img = backdrop(); window(img)
        i = min(int(t // beat), len(states) - 1); tt = t - i * beat
        title, val, base = states[i]
        prev = None
        if i:
            pt, pv, _ = states[i - 1]; prev = pv if pt == title else None
        label(img, title, val, t, i * beat, prev=prev)
        x = text_line(img, 60, 100, 'Switching through three languages')
        c = caret(img, x, 100, t)
        # glide 0 -> 1 -> 2 -> 0 with pauses
        steps = [(0.55, 0.85, 0, 1), (1.25, 1.55, 1, 2)]
        sel = 0.0
        for a, b, s0, s1 in steps:
            if tt >= a: sel = lerp(s0, s1, smooth(seg(tt, a, b)))
        if base['only_new']: sel = round(sel)
        p = dict(base, sel=sel)
        pim, g, k = render_panel(p)
        px, py, side = place(c, p, g, k, 'below')
        e = seg(tt, 0.05, 0.3) if tt < beat - 0.4 else 1 - seg(tt, beat - 0.4, beat - 0.12)
        blit_panel(img, pim, k, (px, py), e, 'slide', side, out=tt >= beat - 0.4)
        return finish(img)
    return frame, beat * len(states)

def scene_parts():
    """Show language code / name / keyboard layout, names in their own language: toggles drive the panel."""
    seq = [dict(abbr=1, name=1, layout=1, native=0), dict(abbr=1, name=1, layout=0, native=0),
           dict(abbr=1, name=0, layout=0, native=0), dict(abbr=1, name=1, layout=0, native=0),
           dict(abbr=1, name=1, layout=1, native=0), dict(abbr=1, name=1, layout=1, native=1),
           dict(abbr=0, name=1, layout=1, native=1)]
    keys = [('abbr', 'Language code'), ('name', 'Language name'), ('layout', 'Keyboard layout'), ('native', 'Names in their own language')]
    beat = 1.8
    def state_at(t):
        i = min(int(t // beat), len(seq) - 1)
        return i, seq[i], seq[i - 1] if i else seq[0], t - i * beat
    def frame(t):
        img = backdrop(); window(img)
        i, cur, prev, tt = state_at(t)
        sw = smooth(seg(tt, 0.0, 0.14))
        changed = [kk for kk, _ in keys if cur[kk] != prev[kk]]
        names = ['Everything', 'Without the layout', 'Codes only', 'Code and name', 'Everything',
                 'Names in their own language', 'Without the code']
        label(img, 'What each entry shows', names[i], t, i * beat, prev=names[i - 1] if i else None)
        for j, (kk, nm) in enumerate(keys):
            on = lerp(prev[kk], cur[kk], sw)
            toggle(img, 470, 120 + j * 52, on, nm, hl=(1 - seg(tt, 1.0, 1.6)) if kk in changed else 0)
        x = text_line(img, 52, 100, 'Typing here')
        c = caret(img, x, 100, t)
        # cross-fade old -> new panel, both anchored at the caret
        for st, alpha in ((prev, 1 - sw), (cur, sw)):
            if alpha <= 0.01: continue
            p = P(abbr=bool(st['abbr']), name=bool(st['name']), layout=bool(st['layout']), native=bool(st['native']))
            pim, g, k = render_panel(p)
            px, py, side = place(c, p, g, k, 'below', wa=(24, 60, 440, H - 24))
            blit_panel(img, pim, k, (px, py), alpha, 'fade', side)
        return finish(img)
    return frame, beat * len(seq)

def scene_shape():
    """Size, Corner radius, Opacity, Drop shadow: sliders morph the panel live."""
    phases = [('Size', 'scale', [100, 140, 80, 100], 50, 400, '%'), ('Corner radius', 'radius', [8, 20, 0, 8], 0, 24, ' px'),
              ('Opacity', 'opacity', [97, 35, 97], 10, 100, '%'), ('Drop shadow', 'shadow', [1, 0, 1], 0, 1, '')]
    beat = 2.4
    def val(ph, tt):
        pts = ph[2]; n = len(pts) - 1
        q = clamp(tt / (beat - 0.3)) * n
        j = min(int(q), n - 1); return lerp(pts[j], pts[j + 1], smooth(q - j))
    def frame(t):
        img = backdrop(); window(img)
        i = min(int(t // beat), len(phases) - 1); tt = t - i * beat
        vals = {}
        for j, ph in enumerate(phases):
            vals[ph[1]] = val(ph, tt) if j == i else (ph[2][-1] if j < i else ph[2][0])
        ph = phases[i]; v = vals[ph[1]]
        label(img, ph[0], ('On' if v > 0.5 else 'Off') if ph[1] == 'shadow' else f'{v:.0f}{ph[5].strip()}', t, 0)
        chip_text(img, 500, 360, 'illustration: Windhawk shows number fields', size=11.5)
        # text behind the panel so opacity is visible
        for r, line in enumerate(['The quick brown fox jumps', 'over the lazy dog. Пиши', 'по-українськи or in English,', 'the panel follows you.']):
            text_line(img, 52, 96 + r * 30, line, size=16)
        x = 52 + font(F_MONO, 16).getlength('The quick') / S
        c = caret(img, x, 96, t, h=22)
        p = P(scale=vals['scale'], radius=vals['radius'], opacity=vals['opacity'], shadow=vals['shadow'] > 0.5)
        pim, g, k = render_panel(p)
        px, py, side = place(c, p, g, k, 'below', wa=(24, 60, 470, H - 24))
        blit_panel(img, pim, k, (px, py), 1.0, 'none', side)
        for j, ph in enumerate(phases):
            v = vals[ph[1]]; y = 112 + j * 66
            hl = 1.0 if j == i else 0.0
            if ph[1] == 'shadow':
                toggle(img, 500, y, clamp(v), 'Drop shadow', hl=hl)
            else:
                slider(img, 500, y, 210, (v - ph[3]) / (ph[4] - ph[3]), ph[0], f'{v:.0f}{ph[5]}', hl=hl)
        return finish(img)
    return frame, beat * len(phases)

def scene_theme():
    """Theme and accent color; the editor follows the theme so it reads like a real switch."""
    steps = [('Theme', 'Light', 0.0, None), ('Theme', 'Dark', 1.0, None), ('Accent color', '#FF8800', 1.0, '#FF8800'),
             ('Accent color', '#E3008C', 1.0, '#E3008C'), ('Accent color', '#16C60C', 1.0, '#16C60C'), ('Theme', 'Light', 0.0, '#16C60C')]
    beat = 1.7
    def frame(t):
        i = min(int(t // beat), len(steps) - 1); tt = t - i * beat
        title, val, dk, acc = steps[i]
        pdk = steps[i - 1][2] if i else dk; pacc = steps[i - 1][3] if i else acc
        f = smooth(seg(tt, 0, 0.45))
        dark = lerp(pdk, dk, f)
        img = backdrop(dark); window(img, dark)
        prev = steps[i - 1][1] if i and steps[i - 1][0] == title else None
        x = text_line(img, 60, 100, 'Light or dark, any accent', dark)
        c = caret(img, x, 100, t, dark)
        def acc_rgb(a, theme): return hexrgb(a) if a else ((0x60, 0xCD, 0xFF) if theme == 'dark' else (0x00, 0x5F, 0xB8))
        theme = 'dark' if dark > 0.5 else 'light'
        a_now = mix(acc_rgb(pacc, theme), acc_rgb(acc, theme), f)
        sel = 1
        label(img, title, val, t, i * beat, dark, prev=prev, swatch=a_now if title == 'Accent color' else None)
        p = P(theme=theme, accent=a_now, scale=125, sel=sel)
        pim, g, k = render_panel(p)
        px, py, side = place(c, p, g, k, 'below')
        blit_panel(img, pim, k, (px, py), 1.0, 'none', side)
        return finish(img)
    return frame, beat * len(steps)

def scene_spacing():
    """The five spacing settings with measurement bands, at 150% so the gaps are visible."""
    phases = [('Panel padding', 'pad', 4, 14), ('Box padding, left and right', 'padx', 16, 30),
              ('Box padding, top and bottom', 'pady', 8, 16), ('Gap between the code and the name', 'gap', 12, 30),
              ('Space between language boxes', 'itemgap', 2, 12)]
    beat = 2.2
    BAND = (0xE3, 0x00, 0x8C)
    def frame(t):
        img = backdrop(); window(img)
        i = min(int(t // beat), len(phases) - 1); tt = t - i * beat
        name, key, lo, hi = phases[i]
        v = lerp(lo, hi, smooth(seg(tt, 0.3, 0.9))) if tt < 1.4 else lerp(hi, lo, smooth(seg(tt, 1.4, 2.0)))
        p = P(scale=135, **{key: v})
        prev = phases[i - 1][0] if i else None
        label(img, name, f'{v:.0f} px', t, i * beat)
        pim, g, k = render_panel(p)
        px, py = 70, 66
        blit_panel(img, pim, k, (px, py), 1.0, 'none')
        s = p['scale'] / 100
        def R(a): return a * s
        r0, r1 = g['rects'][1], g['rects'][2]   # the selected box (its background is visible) and the next one
        d = ImageDraw.Draw(img, 'RGBA')
        def band(x0, y0, x1, y1):
            d.rectangle([X(px + R(x0)), X(py + R(y0)), X(px + R(x1)), X(py + R(y1))], fill=BAND + (90,), outline=BAND + (255,), width=X(1))
        if key == 'pad':
            r0 = g['rects'][0]; rl = g['rects'][-1]
            band(0, 0, r0[0], g['H']); band(r0[2], 0, g['W'], g['H'])
            band(r0[0], 0, r0[2], r0[1]); band(r0[0], rl[3], r0[2], g['H'])
        for rc in g['rects']:   # faint outline of every box, so the box edges are visible
            d.rounded_rectangle([X(px + R(rc[0])), X(py + R(rc[1])), X(px + R(rc[2])), X(py + R(rc[3]))], X(R(5)), outline=(0x88, 0x88, 0x99, 120), width=X(1))
        if key == 'padx': band(r0[0], r0[1], r0[0] + p['padx'], r0[3]); band(r0[2] - p['padx'], r0[1], r0[2], r0[3])
        if key == 'pady': band(r0[0], r0[1], r0[2], r0[1] + p['pady']); band(r0[0], r0[3] - p['pady'], r0[2], r0[3])
        if key == 'gap':
            gx = r0[0] + p['padx'] + g['abbr_col']; band(gx, r0[1], gx + p['gap'], r0[3])
        if key == 'itemgap': band(r0[0], r0[3], r0[2], r1[1]); band(g['rects'][0][0], g['rects'][0][3], g['rects'][0][2], r0[1])
        chip_text(img, W - 50, H - 40, 'shown at 135% size', size=12, anchor='rm')
        return finish(img)
    return frame, beat * len(phases)

def stopwatch(img, x, y, f, text):
    """Illustration-only timer next to the keys (the real panel has no bar)."""
    d = ImageDraw.Draw(img, 'RGBA')
    chip_text(img, x, y - 12, text, size=12)
    d.rounded_rectangle([X(x), X(y), X(x + 150), X(y + 6)], X(3), fill=(0, 0, 0, 22))
    if f > 0: d.rounded_rectangle([X(x), X(y), X(x + 150 * f), X(y + 6)], X(3), fill=(0x00, 0x5F, 0xB8, 255))

def scene_timing():
    """Time visible after the keys are released, Hide as soon as you start typing, Click a language."""
    beats = [(0.0, 'delay', 300), (3.0, 'delay', 1500), (6.4, 'typing', 4000), (9.6, 'click', 300)]
    def frame(t):
        img = backdrop(); window(img)
        bi = max(j for j, b in enumerate(beats) if t >= b[0]); t0, kind, delay = beats[bi]; tt = t - t0
        p = P(sel=(bi + 1) % 3)              # each Alt+Shift moves to the next language
        if kind == 'delay': label(img, 'Time visible after release', f'{delay} ms', t, t0, prev='300 ms' if bi == 1 else None)
        elif kind == 'typing': label(img, 'Hide as soon as you start typing', 'On  (time visible 4 s)', t, t0)
        else: label(img, 'Click a language to switch to it', 'On', t, t0)
        press, release = 0.6, 0.8                # a tap: held 250 ms or more the mod shows the hold preview instead
        alt_dn, sh_dn = press <= tt < release + 0.04, press + 0.05 <= tt < release
        typed = ''
        if kind == 'typing':
            for j, ch in enumerate('and'):
                if tt >= 2.0 + j * 0.22: typed += ch
        x = text_line(img, 60, 100, 'Hello ')
        x2 = text_line(img, x, 100, typed) if typed else x
        c = caret(img, x2, 100, t)
        kx = 60
        kx += keycap(img, kx, H - 82, 'Alt', alt_dn) + 8
        kx += keycap(img, kx, H - 82, 'Shift', sh_dn) + 26
        if kind == 'typing':
            for j, ch in enumerate('AND'):
                dn = 2.0 + j * 0.22 <= tt < 2.0 + j * 0.22 + 0.12
                kx += keycap(img, kx, H - 82, ch, dn, w=40) + 6
        # click beat: the pointer comes onto the panel (hovering keeps it open), clicks the third language, leaves
        ptr = None; hover_until = None
        if kind == 'click':
            if tt >= 1.4 and tt < 2.0: p['sel'] = (bi + 1) % 3
            if tt >= 2.0: p['sel'] = 2
        pim, g, k = render_panel(p)
        anchor = (x, c[1], c[2])
        px, py, side = place(anchor, p, g, k, 'below', wa=(24, 60, W - 24, H - 100))
        rows = g['rects']
        if kind == 'click':
            tx, ty = px + (rows[2][0] + 60) * k / S, py + (rows[2][1] + rows[2][3]) / 2 * k / S
            sx, sy = 600, 330
            mv = smooth(seg(tt, 1.0, 1.4)); back = smooth(seg(tt, 2.3, 2.8))
            ptr = (lerp(lerp(sx, tx, mv), sx, back), lerp(lerp(sy, ty, mv), sy, back))
            hover_until = 2.5                 # pointer off the panel from here: the timer starts
        shown = tt >= release
        if kind == 'typing': hide_at = 2.0
        elif kind == 'click': hide_at = hover_until + delay / 1000
        else: hide_at = release + delay / 1000
        if shown:
            out = tt >= hide_at
            e = seg(tt, release, release + 0.2) if not out else 1 - seg(tt, hide_at, hide_at + 0.26)
            blit_panel(img, pim, k, (px, py), e, 'slide', side, out=out)
        start = hover_until if kind == 'click' else release
        f = 1 - seg(tt, start, start + delay / 1000) if tt >= start else 1
        if tt >= hide_at: f = 0
        stopwatch(img, 480, H - 64, f, f'time visible: {delay} ms' + ('  (typing cuts it short)' if kind == 'typing' else ''))
        if kind == 'click' and 1.95 <= tt < 2.1:   # click ripple
            r = 6 + 40 * seg(tt, 1.95, 2.1)
            ImageDraw.Draw(img, 'RGBA').ellipse([X(ptr[0] - r), X(ptr[1] - r), X(ptr[0] + r), X(ptr[1] + r)], outline=(0x00, 0x5F, 0xB8, int(200 * (1 - seg(tt, 1.95, 2.1)))), width=X(2))
        if ptr: pointer(img, *ptr)
        return finish(img)
    return frame, 13.6

# a simplified stand-in for Windows' own switcher flyout (bigger, with a footer link)
def windows_flyout(img, x, y, e, sel=1):
    if e <= 0: return
    fly = img.copy()
    w, h = 300, 3 * 58 + 56
    sh = Image.new('L', img.size, 0)
    ImageDraw.Draw(sh).rounded_rectangle([X(x), X(y + 6), X(x + w), X(y + h + 6)], X(8), fill=90)
    fly.paste((0, 0, 0), (0, 0), sh.filter(ImageFilter.GaussianBlur(X(12))))
    d = ImageDraw.Draw(fly, 'RGBA')
    d.rounded_rectangle([X(x), X(y), X(x + w), X(y + h)], X(8), fill=(0xEE, 0xEE, 0xEE, 255), outline=(0, 0, 0, 30), width=X(1))
    for i, it in enumerate(ITEMS):
        ry = y + 8 + i * 58
        if i == sel:
            d.rounded_rectangle([X(x + 6), X(ry), X(x + w - 6), X(ry + 54)], X(5), fill=(0, 0, 0, 14))
            d.rounded_rectangle([X(x + 6), X(ry + 17), X(x + 9), X(ry + 37)], X(1.5), fill=(0x00, 0x5F, 0xB8, 255))
        d.text((X(x + 22), X(ry + 27)), it['code'], font=font(F_SB, 13, it['code']), fill=(0x1A, 0x1A, 0x1A, 255), anchor='lm')
        d.text((X(x + 70), X(ry + 18)), it['name'], font=font(F_UI, 14, it['name']), fill=(0x1A, 0x1A, 0x1A, 255), anchor='lm')
        sub = 'Microsoft IME' if it['code'] == '日本語' else it['layout'] + ' keyboard'   # how Windows' switcher names them
        d.text((X(x + 70), X(ry + 38)), sub, font=font(F_UI, 12), fill=(0x1A, 0x1A, 0x1A, 160), anchor='lm')
    d.text((X(x + 22), X(y + h - 24)), 'More keyboard settings', font=font(F_UI, 13), fill=(0x00, 0x5F, 0xB8, 255), anchor='lm')
    img.paste(Image.blend(img, fly, ease(e)))

def desktop(img):
    d = ImageDraw.Draw(img, 'RGBA')
    d.rectangle([0, X(H - 40), X(W), X(H)], fill=(0xF3, 0xF3, 0xF3, 240))
    d.line([0, X(H - 40), X(W), X(H - 40)], fill=(0, 0, 0, 25), width=X(1))
    for i in range(5):
        cx = W / 2 - 80 + i * 40
        d.rounded_rectangle([X(cx - 11), X(H - 31), X(cx + 11), X(H - 9)], X(4), fill=[(0x00, 0x78, 0xD4), (0xF2, 0xB7, 0x05), (0x00, 0x99, 0xBC), (0x5C, 0x2D, 0x91), (0x10, 0x7C, 0x10)][i] + (255,))
    chip_text(img, W - 80, H - 20, 'ENG', size=12, color=(0x1A, 0x1A, 0x1A), anchor='mm')

def pointer(img, x, y):
    pts = [(0, 0), (0, 17), (4.5, 13), (7.5, 20), (10, 19), (7, 12), (12.5, 12)]
    ImageDraw.Draw(img).polygon([(X(x + a), X(y + b)) for a, b in pts], fill=(0, 0, 0), outline=(255, 255, 255))

def scene_switcher():
    """Windows' own switcher setting, and what happens without a text cursor."""
    beats = [('Windows\' own language switcher', 'Hide it', 'caret', 'mod'),
             ('Windows\' own language switcher', 'Moved next to the text cursor', 'caret', 'win-caret'),
             ('Windows\' own language switcher', 'Where Windows puts it', 'caret', 'win-corner'),
             ('No text cursor', 'Keep Windows\' usual behavior', 'none', 'win-corner'),
             ('No text cursor', 'Show this panel at the mouse pointer', 'none', 'mod-mouse')]
    beat = 2.2
    p = P(); pim, g, k = render_panel(p)
    def frame(t):
        img = backdrop(); desktop(img)
        i = min(int(t // beat), len(beats) - 1); tt = t - i * beat
        title, val, cur, show = beats[i]
        prev = beats[i - 1][1] if i and beats[i - 1][0] == title else None
        card = (40, 50, W - 220, H - 64)
        if cur == 'caret':
            window(img, card=card)
            x = text_line(img, 70, 126, 'Win+Space here')
            c = caret(img, x, 126, t)
        else:
            chip_text(img, 60, 60, 'Desktop: no text cursor anywhere', size=13, color=(0x33, 0x33, 0x44))
        label(img, title, val, t, i * beat, prev=prev, x=24, y=24)
        e = seg(tt, 0.35, 0.6) if tt < beat - 0.35 else 1 - seg(tt, beat - 0.35, beat - 0.1)
        mx, my = 300, 150
        if cur == 'none': pointer(img, mx, my)
        dn = 0.2 <= tt < beat - 0.45
        presses = sum(1 for k0 in range(4) if 0.2 + k0 * 0.8 <= tt < beat - 0.45) if cur == 'caret' else 1
        p_sel = presses % 3                   # each Space moves the selection, in both panels
        kx = W - 196
        kx += keycap(img, kx, 64, 'Win', dn) + 8
        keycap(img, kx, 64, 'Space', dn and (tt % 0.8) < 0.5, w=90)
        def tag(x, y, text):
            if e <= 0.05: return
            f = font(F_SB, 11.5); w = f.getlength(text) / S + 18
            d = ImageDraw.Draw(img, 'RGBA')
            d.rounded_rectangle([X(x), X(y), X(x + w), X(y + 22)], X(11), fill=(0x1F, 0x1F, 0x24, int(225 * e)))
            d.text((X(x + w / 2), X(y + 11)), text, font=f, fill=(255, 255, 255, int(255 * e)), anchor='mm')
        if show == 'mod':
            ps = dict(p, sel=p_sel); psim, gs, ks = render_panel(ps)
            px, py, side = place(c, ps, gs, ks, 'below', wa=(24, 40, W - 24, H - 48)); blit_panel(img, psim, ks, (px, py), e, 'slide', side, out=tt >= beat - 0.35)
            tag(px + gs['W'] * ks / S + 10, py + 4, 'this mod')
        elif show == 'win-caret':
            windows_flyout(img, c[0] - 16, c[2] + 6, e, sel=p_sel); tag(c[0] + 300, c[2] + 12, "Windows' switcher")
        elif show == 'win-corner':
            windows_flyout(img, W - 312, H - 40 - 3 * 58 - 56 - 12, e, sel=p_sel); tag(W - 312, H - 40 - 3 * 58 - 56 - 40, "Windows' switcher")
        elif show == 'mod-mouse':
            px, py, side = place((mx, my, my + 20), p, g, k, 'below', wa=(24, 40, W - 24, H - 48)); blit_panel(img, pim, k, (px, py), e, 'slide', side, out=tt >= beat - 0.35)
            tag(px + g['W'] * k / S + 10, py + 4, 'this mod')
        if cur == 'none': pointer(img, mx, my)
        return finish(img)
    return frame, beat * len(beats)

def scene_modes():
    """Every display mode in use: two language switches each."""
    modes = [(('List direction', 'Vertical'), P()), (('List direction', 'Horizontal'), P(horizontal=True)),
             (('What each entry shows', 'Codes only'), P(name=False, layout=False)),
             (('List direction', 'Horizontal (codes only)'), P(name=False, layout=False, horizontal=True)),
             (('Show only the new language', 'On'), P(only_new=True)),
             (('Show only the new language', 'On (codes only)'), P(only_new=True, name=False, layout=False)),
             (('Reverse language order', 'On'), P(reverse=True))]
    beat = 2.6
    def frame(t):
        img = backdrop(); window(img)
        i = min(int(t // beat), len(modes) - 1); tt = t - i * beat
        (title, val), base = modes[i]
        pt, pv = modes[i - 1][0] if i else (None, None)
        label(img, title, val, t, i * beat, prev=pv if pt == title else None)
        x = text_line(img, 60, 104, 'Switching languages')
        c = caret(img, x, 104, t)
        s0 = (2 * i) % 3                             # two switches per beat: continue where the last beat ended
        sel = s0 + sum(1 for a in (0.95, 1.65) if tt >= a)   # the highlight jumps like the real one
        kx = 60
        down = any(a - 0.12 <= tt < a for a in (0.95, 1.65))
        kx += keycap(img, kx, H - 82, 'Alt', down) + 8
        keycap(img, kx, H - 82, 'Shift', down)
        e = seg(tt, 0.15, 0.4) if tt < beat - 0.42 else 1 - seg(tt, beat - 0.42, beat - 0.14)
        p = dict(base, sel=sel % 3)
        pim, g, k = render_panel(p)
        px, py, side = place(c, p, g, k, 'below')
        blit_panel(img, pim, k, (px, py), e, 'slide', side, out=tt >= beat - 0.42)
        return finish(img)
    return frame, beat * len(modes)

def make_styles():
    """assets/styles.png: every display style, light and dark (still image)."""
    modes = [('List', P()), ('List, horizontal', P(horizontal=True)), ('Codes only', P(name=False, layout=False)),
             ('Codes only, horizontal', P(name=False, layout=False, horizontal=True)),
             ('Only the new language', P(only_new=True)), ('Only the new language, code only', P(only_new=True, name=False, layout=False))]
    rows = [[0, 1], [2, 3, 4, 5]]
    secs = []
    for theme, bg, fg, sub in (('light', (0xF3, 0xF3, 0xF3), (0x20, 0x20, 0x20), (0x60, 0x60, 0x60)),
                               ('dark', (0x20, 0x20, 0x22), (0xF0, 0xF0, 0xF0), (0xA8, 0xA8, 0xB0))):
        ims = [render_panel(dict(m, theme=theme, sel=1.0 if not m.get('only_new') else 1))[0] for _, m in modes]
        secs.append((theme, bg, fg, sub, ims))
    pad, gapx, cap = 36, 26, 30
    Wc = 1180 * S
    heights = [max(secs[0][4][i].height for i in r) for r in rows]
    Hs = X(60) + sum(h + X(cap) + X(12) for h in heights) + X(20)
    out = Image.new('RGB', (Wc, Hs * 2))
    for n, (theme, bg, fg, sub, ims) in enumerate(secs):
        sec = Image.new('RGB', (Wc, Hs), bg); d = ImageDraw.Draw(sec, 'RGBA')
        d.text((X(pad), X(30)), 'Light theme' if theme == 'light' else 'Dark theme', font=font(F_SB, 19), fill=fg, anchor='lm')
        y = X(60)
        for r, h in zip(rows, heights):
            x = X(pad) - MARGIN * S
            for i in r:
                im = ims[i]
                d.text((x + MARGIN * S, y + X(10)), modes[i][0], font=font(F_UI, 14), fill=sub, anchor='lm')
                sec.paste(im, (int(x), int(y + X(cap) - MARGIN * S)), im)
                x += im.width - 2 * MARGIN * S + X(gapx) + 2 * MARGIN * S - X(10)
            y += h + X(cap) - 2 * MARGIN * S + X(40)
        out.paste(sec, (0, n * Hs))
    out = out.resize((Wc // S, out.height // S), Image.LANCZOS)
    out.save(os.path.join(OUT, '..', 'styles.png'), optimize=True); print('styles.png', out.size)

SCENES = dict(modes=scene_modes, placement=scene_placement, animation=scene_animation, parts=scene_parts,
              shape=scene_shape, theme=scene_theme, spacing=scene_spacing, timing=scene_timing, switcher=scene_switcher)

if __name__ == '__main__':
    names = sys.argv[1:] or list(SCENES) + ['styles']
    for nm in names:
        if nm == 'styles': make_styles(); continue
        fn, dur = SCENES[nm]()
        fps = {'shape': 12, 'spacing': 12}.get(nm, FPS)   # whole-panel morphs: fewer frames, much smaller files
        if os.environ.get('STILL'):  # STILL=seconds -> one PNG for checking
            fn(float(os.environ['STILL'])).save(os.path.join(OUT, f'_{nm}.png')); continue
        write_gif(nm, fn, dur, fps)
