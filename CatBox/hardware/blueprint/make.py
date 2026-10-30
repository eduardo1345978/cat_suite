# Cat Box 3 - blueprints (A3 technical drawings) of every printed part.
#   python3 make.py <scratchpad>      -> bp/sheets.html, bp/CatBox3_blueprinty.pdf, bp/sheet_NN.png
import json, math, os, subprocess, sys, datetime
import numpy as np

S = sys.argv[1]
B = S + '/bp'
O = S + '/stl/out'
ST = S + '/stl'

# ------------------------------------------------------------------ source data
parts = json.load(open(ST + '/parts.json'))
screws = json.load(open(ST + '/screws.json'))
layout = json.load(open(ST + '/layout/current.json')); layout = (layout.get('data') or layout)['parts']
cl = json.load(open(ST + '/case_layers.json'))
LEN = screws['len']; sy = LEN / parts['H']
W, D = 420.0, 221.0
FRONT, BACK, PANEL = 35.0, 57.0, 3.0
SLOPE = math.atan2(BACK - FRONT, D)

def mat(R=np.eye(3), t=(0, 0, 0)):
    M = np.hstack([np.asarray(R, float), np.asarray(t, float).reshape(3, 1)]); return M
def comp(A, Bm):  # A after B (3x4 each)
    R = A[:, :3] @ Bm[:, :3]; t = A[:, :3] @ Bm[:, 3] + A[:, 3]; return mat(R, t)
def Rx(a): c, s = math.cos(a), math.sin(a); return np.array([[1, 0, 0], [0, c, -s], [0, s, c]])
def Rz(a): c, s = math.cos(a), math.sin(a); return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])
flat = lambda M: [round(float(x), 6) for x in M.reshape(-1)]
F = lambda n: O + '/' + n + '.stl'
FLIP = lambda h: flat(mat(Rx(math.pi), (0, 0, h)))      # printed top-down -> as mounted

# panel holes (in the panel's own frame: front-left corner, face up; Y along the panel towards the back)
def pc(q):
    w = q.get('d') or q['w']; h = q.get('d') or q['h']
    return q['pos'][0] + w / 2, LEN - (q['pos'][1] + h / 2) * sy
holes = []
cnt = {}
def hid(k): cnt[k] = cnt.get(k, 0) + 1; return '%s%d' % (k, cnt[k])
for q in sorted(parts['parts'], key=lambda q: (-pc(q)[1] if q['kind'] != 'logo' else 0, pc(q)[0]) if q['kind'] != 'logo' else (0, 0)):
    if q['kind'] == 'logo': continue
    x, y = pc(q)
    if q['kind'] == 'circle':
        if q['id'] == 'speaker': holes.append(dict(id=hid('R'), x=x, y=y, t='reproduktor', size='5× 28×2', pocket='Ø40,6', shape='spk'))
        elif q['id'] == 'joy': holes.append(dict(id=hid('J'), x=x, y=y, t='joystick', size='Ø30', pocket='', shape='c', d=30.0))
        elif q.get('pot'): holes.append(dict(id=hid('P'), x=x, y=y, t='potenciometer', size='Ø7,4', pocket='', shape='c', d=7.4, name=q['name']))
        else: holes.append(dict(id=hid('E'), x=x, y=y, t='enkóder', size='Ø7,4', pocket='', shape='c', d=7.4))
    else:
        if q['id'] == 'display': holes.append(dict(id=hid('D'), x=x, y=y, t='displej', size='110×66', pocket='121,6×76,6', shape='r', w=110, h=66))
        elif q['id'] == 'slider': holes.append(dict(id=hid('L'), x=x, y=y, t='slider', size='64×3', pocket='76×10', shape='r', w=64, h=3))
        else: holes.append(dict(id=hid('S'), x=x, y=y, t='MX spínač', size='14×14', pocket='15,4×15,4', shape='r', w=14, h=14))
for sx, sv in screws['screws']:
    holes.append(dict(id=hid('M'), x=sx, y=LEN - sv, t='skrutka M3', size='Ø3,4', pocket='', shape='c', d=3.4))

# ------------------------------------------------------------------ projection jobs
ASM = []
for n in ('CatBox3_case_1', 'CatBox3_case_2', 'CatBox3_case_3'): ASM.append({'file': F(n)})
PF = mat(Rx(SLOPE), (0, 0, FRONT - PANEL))
ASM.append({'file': F('CatBox3_panel_full'), 'm': flat(PF)})
for q in parts['parts']:
    if q['kind'] == 'logo': continue
    x, y = pc(q)
    cap = lambda name, z, rz=0.0: ASM.append({'file': F(name), 'm': flat(comp(PF, mat(Rx(math.pi) @ Rz(rz), (x, y, z))))})
    if q.get('pot'):
        big = q['id'] == 'pot_cutoff'; cap('CatBox3_knob_cutoff_15mm' if big else 'CatBox3_knob_pot_11mm', 5 + (15 if big else 14), math.pi / 2)
    elif q['id'].startswith('enc'): cap('CatBox3_cap_encoder_14mm', 5 + 16, math.pi / 2)
    elif q['id'] == 'slider': x -= 10; cap('CatBox3_cap_slider', 3 + 4 + 8, math.pi / 2)
    elif q['kind'] == 'rect' and q['id'] != 'display': cap('CatBox3_keycap_mx', 3 + 6.6 + 7.2)

jobs = [
    dict(name='asm', angle=30, res=4, meshes=ASM, views=['front', 'top', 'left', 'iso'], hidden=[]),
    dict(name='panel', angle=30, res=5, meshes=[{'file': F('CatBox3_panel_full')}], views=['top'], hidden=['top']),
    dict(name='labels', angle=30, res=5, meshes=[{'file': F('CatBox3_panel_labels_full')}], views=['top'], hidden=[]),
    dict(name='p1', angle=30, res=5, meshes=[{'file': F('CatBox3_panel_1')}], views=['top'], hidden=['top']),
    dict(name='p2', angle=30, res=5, meshes=[{'file': F('CatBox3_panel_2')}], views=['top'], hidden=['top']),
    dict(name='p3', angle=30, res=5, meshes=[{'file': F('CatBox3_panel_3')}], views=['top'], hidden=['top']),
    dict(name='sp1', angle=30, res=8, meshes=[{'file': F('CatBox3_panel_splice_1')}], views=['top'], hidden=[]),
    dict(name='sp2', angle=30, res=8, meshes=[{'file': F('CatBox3_panel_splice_2')}], views=['top'], hidden=[]),
    dict(name='case', angle=25, res=5, sliver=0.2, meshes=[{'file': F('CatBox3_case_%d' % i)} for i in (1, 2, 3)], views=['top', 'front', 'left', 'back', 'iso'], hidden=[]),
]
for i in (1, 2, 3):
    jobs.append(dict(name='c%d' % i, angle=25, res=6, sliver=0.2, meshes=[{'file': F('CatBox3_case_%d' % i)}],
                     views=['top', 'front', 'left', 'right', 'iso'], hidden=[]))
for n, h in (('CatBox3_knob_pot_11mm', 14), ('CatBox3_knob_cutoff_15mm', 15), ('CatBox3_cap_encoder_14mm', 16), ('CatBox3_keycap_mx', 7.2), ('CatBox3_cap_slider', 8)):
    jobs.append(dict(name=n, angle=40, res=40, meshes=[{'file': F(n), 'm': FLIP(h)}], views=['front', 'top', 'bottom', 'left', 'iso'], hidden=['front', 'left']))
json.dump(jobs, open(B + '/jobs.json', 'w'))
if not os.path.exists(B + '/views.json') or '--hlr' in sys.argv:
    subprocess.run(['node', B + '/hlr.js', B + '/jobs.json', B + '/views.json'], check=True)
V = json.load(open(B + '/views.json'))

# ------------------------------------------------------------------ drawing kit (sheet units: mm, A3 landscape)
BG, LINE, DIM, NOTE, ACC = '#17487f', '#f4f8ff', '#bcd7f7', '#dbe9fb', '#ffd98a'
FONT = "'DejaVu Sans Condensed','DejaVu Sans',sans-serif"
def num(x, d=2):
    s = ('%.' + str(d) + 'f') % x; s = s.rstrip('0').rstrip('.') if '.' in s else s; return s.replace('.', ',').replace('-0', '0') if s != '-0' else '0'
def esc(t): return str(t).replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')

class Sheet:
    def __init__(s, title, part, scale, material, note=''):
        s.el, s.title, s.part, s.scale, s.material, s.note = [], title, part, scale, material, note
    def add(s, x): s.el.append(x)
    def text(s, x, y, t, size=2.5, anchor='start', color=NOTE, weight='normal', rot=0, ls=0):
        tr = ' transform="rotate(%g %g %g)"' % (rot, x, y) if rot else ''
        s.add('<text x="%.2f" y="%.2f" font-size="%g" fill="%s" text-anchor="%s" font-weight="%s" letter-spacing="%g"%s>%s</text>' % (x, y, size, color, anchor, weight, ls, tr, esc(t)))
    def line(s, x1, y1, x2, y2, w=0.18, c=DIM, dash=None):
        s.add('<line x1="%.2f" y1="%.2f" x2="%.2f" y2="%.2f" stroke="%s" stroke-width="%g"%s/>' % (x1, y1, x2, y2, c, w, ' stroke-dasharray="%s"' % dash if dash else ''))
    def arrow(s, x, y, ang, c=DIM, L=2.4, Wd=0.75):
        ca, sa = math.cos(ang), math.sin(ang)
        p = [(x, y), (x - L * ca + Wd * sa, y - L * sa - Wd * ca), (x - L * ca - Wd * sa, y - L * sa + Wd * ca)]
        s.add('<path d="M%.2f %.2fL%.2f %.2fL%.2f %.2fZ" fill="%s"/>' % (p[0] + p[1] + p[2] + (c,)))
    # ---- a projected view
    def view(s, job, vname, scale, ox=None, oy=None, label=None, lw=0.3, hw=0.15, label_dy=None, left=None, top=None):
        v = V[job][vname]
        if left is not None: ox = left - v['b'][0] * scale
        if top is not None: oy = top + v['b'][3] * scale
        X = lambda u: ox + u * scale; Y = lambda w: oy - w * scale
        def path(pl):
            out = []
            for p in pl:
                out.append('M' + ' '.join('%.2f %.2f' % (X(p[i]), Y(p[i + 1])) for i in range(0, len(p), 2)))
            return ''.join(out)
        if v['hid']: s.add('<path d="%s" fill="none" stroke="%s" stroke-width="%g" stroke-dasharray="1.2 0.7" stroke-opacity="0.55" stroke-linecap="round"/>' % (path(v['hid']), DIM, hw))
        s.add('<path d="%s" fill="none" stroke="%s" stroke-width="%g" stroke-linecap="round" stroke-linejoin="round"/>' % (path(v['vis']), LINE, lw))
        u0, u1, v0, v1 = v['b']
        m = Map(X, Y, scale, (X(u0), Y(v1), X(u1), Y(v0)))
        if label:
            ly = m.bb[3] + (label_dy if label_dy is not None else 8)
            s.text((m.bb[0] + m.bb[2]) / 2, ly, label, 2.6, 'middle', ACC, 'bold', ls=0.3)
        return m
    # ---- dimensions (model coords through a map)
    def dimH(s, m, u1, u2, v, off, txt=None, v2=None, tx=None):
        x1, x2 = m.X(u1), m.X(u2); y1 = m.Y(v); y2 = m.Y(v if v2 is None else v2)
        yd = min(y1, y2) - off if off > 0 else max(y1, y2) - off
        for x, y in ((x1, y1), (x2, y2)):
            g = 0.8 if yd < y else -0.8
            s.line(x, y - g, x, yd - (1.2 if yd < y else -1.2))
        s.line(x1, yd, x2, yd)
        if abs(x2 - x1) > 7: s.arrow(x1, yd, math.pi); s.arrow(x2, yd, 0)
        else: s.arrow(x1, yd, 0); s.arrow(x2, yd, math.pi); s.line(min(x1, x2) - 4, yd, max(x1, x2) + 4, yd)
        t = txt if txt is not None else num(abs(u2 - u1))
        s.text(tx if tx is not None else (x1 + x2) / 2, yd - 0.9, t, 2.5, 'middle', DIM)
    def dimV(s, m, v1, v2, u, off, txt=None, u2=None):
        y1, y2 = m.Y(v1), m.Y(v2); x1 = m.X(u); x2 = m.X(u if u2 is None else u2)
        xd = max(x1, x2) + off if off > 0 else min(x1, x2) + off
        for x, y in ((x1, y1), (x2, y2)):
            g = 0.8 if xd > x else -0.8
            s.line(x + g, y, xd + (1.2 if xd > x else -1.2), y)
        s.line(xd, y1, xd, y2)
        if abs(y2 - y1) > 7: s.arrow(xd, max(y1, y2), math.pi / 2); s.arrow(xd, min(y1, y2), -math.pi / 2)
        else: s.arrow(xd, max(y1, y2), -math.pi / 2); s.arrow(xd, min(y1, y2), math.pi / 2); s.line(xd, min(y1, y2) - 4, xd, max(y1, y2) + 4)
        t = txt if txt is not None else num(abs(v2 - v1))
        s.text(xd - 0.9, (y1 + y2) / 2, t, 2.5, 'middle', DIM, rot=-90)
    def lead(s, m, u, v, dx, dy, txt, r=0.0, color=DIM, size=2.4):
        """A leader from a point (or a circle of radius r, model mm) to a note dx, dy (sheet mm) away."""
        x, y = m.X(u), m.Y(v)
        a = math.atan2(dy, dx); x0, y0 = x + math.cos(a) * r * m.s, y + math.sin(a) * r * m.s
        x1, y1 = x + dx, y + dy
        s.line(x0, y0, x1, y1, c=color); s.arrow(x0, y0, a + math.pi, c=color)
        sh = 2.0 if dx >= 0 else -2.0
        tw = len(txt) * size * 0.5
        s.line(x1, y1, x1 + sh + (tw if dx >= 0 else -tw), y1, c=color)
        s.text(x1 + sh * 0.6, y1 - 0.8, txt, size, 'start' if dx >= 0 else 'end', color)
    def centre(s, m, u, v, r):
        x, y, R = m.X(u), m.Y(v), r * m.s + 1.5
        s.line(x - R, y, x + R, y, 0.13, DIM, '3 0.8 0.6 0.8'); s.line(x, y - R, x, y + R, 0.13, DIM, '3 0.8 0.6 0.8')
    def svg(s, n, total):
        g = []
        g.append('<rect width="420" height="297" fill="%s"/>' % BG)
        g.append('<rect width="420" height="297" fill="url(#grid)"/>')
        g.append('<rect width="420" height="297" fill="url(#vig)"/>')
        g.append('<rect x="10" y="10" width="400" height="277" fill="none" stroke="%s" stroke-width="0.6"/>' % LINE)
        g.append('<rect x="12" y="12" width="396" height="273" fill="none" stroke="%s" stroke-width="0.2" stroke-opacity="0.6"/>' % LINE)
        for i in range(8):  # zone marks
            x = 10 + 400 * (i + 0.5) / 8; g.append('<text x="%.1f" y="9" font-size="2.2" fill="%s" text-anchor="middle">%d</text>' % (x, DIM, i + 1))
            g.append('<text x="%.1f" y="291.6" font-size="2.2" fill="%s" text-anchor="middle">%d</text>' % (x, DIM, i + 1))
        for i in range(6):
            y = 10 + 277 * (i + 0.5) / 6; c = 'ABCDEF'[i]
            g.append('<text x="5.5" y="%.1f" font-size="2.2" fill="%s" text-anchor="middle">%s</text>' % (y + 0.8, DIM, c))
            g.append('<text x="414.5" y="%.1f" font-size="2.2" fill="%s" text-anchor="middle">%s</text>' % (y + 0.8, DIM, c))
        g += s.el
        # title block (bottom right)
        x0, y0, w, h = 270, 245, 138, 40
        tb = ['<g font-family="%s">' % FONT]
        tb.append('<rect x="%g" y="%g" width="%g" height="%g" fill="%s" fill-opacity="0.92" stroke="%s" stroke-width="0.5"/>' % (x0, y0, w, h, BG, LINE))
        for yy in (y0 + 13, y0 + 22, y0 + 31): tb.append('<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="%s" stroke-width="0.25"/>' % (x0, yy, x0 + w, yy, LINE))
        for xx, a, b in ((x0 + 46, y0 + 13, y0 + h), (x0 + 92, y0 + 13, y0 + h)): tb.append('<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="%s" stroke-width="0.25"/>' % (xx, a, xx, b, LINE))
        def cell(x, y, lab, val, size=3.0, bold=False):
            tb.append('<text x="%g" y="%g" font-size="1.7" fill="%s" letter-spacing="0.2">%s</text>' % (x + 1.5, y + 2.6, DIM, esc(lab)))
            size = min(size, 43.0 / max(1, len(val) * 0.52))
            tb.append('<text x="%g" y="%g" font-size="%g" fill="%s" font-weight="%s">%s</text>' % (x + 1.5, y + 7.2, size, LINE, 'bold' if bold else 'normal', esc(val)))
        tb.append('<text x="%g" y="%g" font-size="6.2" fill="%s" font-weight="bold" letter-spacing="0.6">CAT BOX 3</text>' % (x0 + 3, y0 + 8.2, LINE))
        tb.append('<text x="%g" y="%g" font-size="2.6" fill="%s">%s</text>' % (x0 + 3, y0 + 11.6, ACC, esc(s.title)))
        # first-angle projection symbol
        sx, syy = x0 + 118, y0 + 6.5
        tb.append('<g stroke="%s" stroke-width="0.25" fill="none"><path d="M%g %g L%g %g L%g %g L%g %g Z"/><circle cx="%g" cy="%g" r="2.8"/><circle cx="%g" cy="%g" r="1.4"/><line x1="%g" y1="%g" x2="%g" y2="%g" stroke-dasharray="1 0.5"/></g>' % (
            LINE, sx - 7, syy - 2.8, sx - 7, syy + 2.8, sx - 1, syy + 1.6, sx - 1, syy - 1.6, sx + 6, syy, sx + 6, syy, sx - 9, syy, sx + 10, syy))
        cell(x0, y0 + 13, 'DIEL / SÚBOR', s.part, 2.6, True)
        cell(x0 + 46, y0 + 13, 'MIERKA', s.scale)
        cell(x0 + 92, y0 + 13, 'LIST', '%d / %d' % (n, total))
        cell(x0, y0 + 22, 'MATERIÁL', s.material, 2.6)
        cell(x0 + 46, y0 + 22, 'JEDNOTKY', 'mm')
        cell(x0 + 92, y0 + 22, 'DÁTUM', datetime.date.today().strftime('%d.%m.%Y'), 2.8)
        cell(x0, y0 + 31, 'NÁVRH', 'Šimon', 2.8)
        cell(x0 + 46, y0 + 31, 'PROJEKCIA', 'ISO E (európska)', 2.4)
        cell(x0 + 92, y0 + 31, 'TOLERANCIA', '±0,2 (tlač)', 2.6)
        tb.append('</g>')
        g += tb
        if s.note:
            yy = 285 - 3.4 * len(s.note)
            for i, t in enumerate(s.note): g.append('<text x="16" y="%.1f" font-size="2.3" fill="%s">%s</text>' % (yy + i * 3.4, NOTE, esc(t)))
        defs = ('<defs><pattern id="grid" width="10" height="10" patternUnits="userSpaceOnUse"><path d="M10 0H0V10" fill="none" stroke="#ffffff" stroke-opacity="0.07" stroke-width="0.2"/></pattern>'
                '<pattern id="hatch" width="1.2" height="1.2" patternUnits="userSpaceOnUse" patternTransform="rotate(45)"><line x1="0" y1="0" x2="0" y2="1.2" stroke="%s" stroke-width="0.18"/></pattern>'
                '<radialGradient id="vig" cx="50%%" cy="45%%" r="75%%"><stop offset="0" stop-color="#2a63a6" stop-opacity="0.35"/><stop offset="1" stop-color="#0b2e5a" stop-opacity="0.45"/></radialGradient></defs>') % LINE
        return '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 420 297" width="420mm" height="297mm" font-family="%s">%s%s</svg>' % (FONT, defs, ''.join(g))

class Map:
    def __init__(m, X, Y, s, bb): m.X, m.Y, m.s, m.bb = X, Y, s, bb

def section_rect(sh, x, y, w, h):  # a hatched cut face (sheet coords)
    sh.add('<rect x="%.2f" y="%.2f" width="%.2f" height="%.2f" fill="url(#hatch)" stroke="%s" stroke-width="0.3"/>' % (x, y, w, h, LINE))

sheets = []
PLA = 'PETG / PLA, tmavý'

# ================================================================== 1. assembly
sh = Sheet('Zostava – celkový pohľad', 'Cat Box 3 – zostava', '1:2 · axon. 1:3', 'tlačené diely + elektronika',
           ['Drevené bočnice (2× 12 mm, nekreslené) pridávajú k šírke 24 mm: spolu 444 mm.',
            'Panel je sklonený o %s° (vpredu %s mm, vzadu %s mm bez gombíkov).' % (num(math.degrees(SLOPE), 1), num(FRONT), num(BACK))])
s = 0.5
fr = sh.view('asm', 'front', s, left=26, top=24, label='POHĽAD SPREDU', label_dy=17)
pl = sh.view('asm', 'top', s, ox=fr.X(0), top=104, label='PÔDORYS', label_dy=17)
lf = sh.view('asm', 'left', s, left=262, oy=fr.Y(0), label='BOKORYS ZĽAVA', label_dy=17)
iso = sh.view('asm', 'iso', 1 / 3, left=258, top=112, label='AXONOMETRIA', lw=0.2, label_dy=10)
sh.dimH(fr, 0, W, 0, -8)
sh.dimV(fr, 0, FRONT, 0, -6)
sh.dimV(lf, 0, BACK, -D, -6)
sh.dimV(lf, 0, FRONT, 0, 6)
zt = V['asm']['left']['b'][3]
sh.dimV(lf, 0, zt, 0, 14, num(zt, 1))
sh.dimH(lf, -D, 0, 0, -8)
sh.dimH(pl, 0, W, 0, -8)
sh.dimV(pl, 0, D, 0, -8)
sheets.append(sh)

# ================================================================== 2. panel with the holes
sh = Sheet('Panel – otvory a zápustky', 'CatBox3_panel_full (1-3)', '1:2 · detaily 5:1', PLA + ' + svetlý (popisy)',
           ['Počiatok 0,0 = predný ľavý roh panela, líce hore. Y ide pozdĺž panela dozadu (dĺžka %s).' % num(LEN),
            'Hrúbka 3; zápustky zospodu 1,5 hlboké. Popisy vygravírované 0,4 do líca. Čiarkovane = zápustky zospodu.'])
s = 0.5
pm = sh.view('panel', 'top', s, left=24, top=30, label='PÔDORYS PANELA (LÍCE)', lw=0.22, hw=0.12, label_dy=28)
sh.dimH(pm, 0, W, LEN, 8)
sh.dimV(pm, 0, LEN, 0, -8)
for h in holes:
    x, y = pm.X(h['x']), pm.Y(h['y'])
    if h['shape'] == 'r' and h['t'] == 'MX spínač': sh.text(x, y + 0.9, h['id'], 2.0, 'middle', ACC, 'bold')
    elif h['shape'] == 'c' and h['t'] in ('potenciometer', 'enkóder', 'skrutka M3'): sh.text(x + 2.4, y - 2.0, h['id'], 1.6, 'start', ACC, 'bold')
    else: sh.text(x, y + 0.9, h['id'], 2.4, 'middle', ACC, 'bold')
def first(t): return next(h for h in holes if h['t'] == t)
d0 = first('displej'); sh.lead(pm, d0['x'] + 55, d0['y'] - 33, 8, 40, 'okno 110×66, zápustka 121,6×76,6')
j0 = first('joystick'); sh.lead(pm, j0['x'], j0['y'], 12, -18, 'Ø30', r=15)
l0 = first('slider'); sh.lead(pm, l0['x'] + 32, l0['y'], 22, 33, 'štrbina 64×3, zápustka 76×10')
r0 = first('reproduktor'); sh.lead(pm, r0['x'] + 4, r0['y'] - 18, 14, 66, '5× 28×2, zápustka Ø40,6')
# hole table
tx, ty = 250, 24
sh.text(tx, ty - 6, 'TABUĽKA OTVOROV', 3.2, 'start', ACC, 'bold', ls=0.4)
cols = 3; per = math.ceil(len(holes) / cols); cw = 52
for c in range(cols):
    x = tx + c * cw
    sh.text(x, ty, 'č.', 2.0, 'start', DIM, 'bold'); sh.text(x + 15, ty, 'X', 2.0, 'end', DIM, 'bold'); sh.text(x + 27, ty, 'Y', 2.0, 'end', DIM, 'bold'); sh.text(x + 29, ty, 'otvor', 2.0, 'start', DIM, 'bold')
    sh.line(x, ty + 1.0, x + cw - 3, ty + 1.0, 0.2)
    for i, h in enumerate(holes[c * per:(c + 1) * per]):
        y = ty + 4.2 + i * 3.25
        sh.text(x, y, h['id'], 2.0, 'start', ACC, 'bold'); sh.text(x + 15, y, num(h['x'], 1), 2.0, 'end', NOTE); sh.text(x + 27, y, num(h['y'], 1), 2.0, 'end', NOTE); sh.text(x + 29, y, h['size'], 2.0, 'start', NOTE)
legend = ['P = potenciometer Ø7,4 (M7) · E = enkóder Ø7,4 · S = MX 14×14, zápustka 15,4×15,4×1,5',
          'J = joystick · D = displej · L = slider · R = reproduktor · M = skrutka M3 (Ø3,4)',
          'X, Y = stred otvoru.']
for i, t in enumerate(legend): sh.text(tx, ty + 4.2 + per * 3.25 + 3 + i * 3.2, t, 2.0, 'start', NOTE)
# details (schematic sections, 5:1)
k = 5.0; T, P = 3.0, 1.5; dy = 212
def hrect(x1, y1, x2, y2):
    sh.add('<rect x="%.2f" y="%.2f" width="%.2f" height="%.2f" fill="url(#hatch)" stroke="%s" stroke-width="0.3"/>' % (min(x1, x2), min(y1, y2), abs(x2 - x1), abs(y2 - y1), LINE))
cxa = 84; L = 4.0
sh.text(24, dy - 22, 'DETAIL A – MX OTVOR (rez) 5:1', 2.6, 'start', ACC, 'bold', ls=0.3)
for sd in (-1, 1):
    hrect(cxa + 7.0 * sd * k, dy, cxa + (7.7 + L) * sd * k, dy + (T - P) * k)
    hrect(cxa + 7.7 * sd * k, dy + (T - P) * k, cxa + (7.7 + L) * sd * k, dy + T * k)
dm = Map(lambda u: cxa + u * k, lambda v: dy + T * k - v * k, k, None)
sh.dimH(dm, -7, 7, T, 6, '14')
sh.dimH(dm, -7.7, 7.7, 0, -6, '15,4')
sh.dimV(dm, 0, P, 7.7 + L, 4, '1,5')
sh.dimV(dm, 0, T, -7.7 - L, -4, '3')
sh.text(cxa, dy + T * k + 14, 'spínač zacvakne zhora, zápustka dáva miesto pätke', 2.0, 'middle', NOTE)
cxb = 205
sh.text(158, dy - 22, 'DETAIL B – POT / ENKÓDER 5:1', 2.6, 'start', ACC, 'bold', ls=0.3)
for sd in (-1, 1): hrect(cxb + 3.7 * sd * k, dy, cxb + (3.7 + 7) * sd * k, dy + T * k)
dm2 = Map(lambda u: cxb + u * k, lambda v: dy + T * k - v * k, k, None)
sh.dimH(dm2, -3.7, 3.7, T, 6, 'Ø7,4')
sh.dimV(dm2, 0, T, 10.7, 4, '3')
sh.line(cxb, dy - 4, cxb, dy + T * k + 4, 0.13, DIM, '3 0.8 0.6 0.8')
sh.text(cxb, dy + T * k + 14, 'M7 matica zhora, výstupok proti otáčaniu odlomiť', 2.0, 'middle', NOTE)
sheets.append(sh)

# ================================================================== 3. panel print pieces + splices
sh = Sheet('Panel – diely na tlač a spojovacie pásy', 'CatBox3_panel_1/2/3 + splice_1/2', '1:2', PLA + ' + svetlý',
           ['Diely sa prekrývajú v ozubenom šve; pásy (1,5 mm) sa vlepia zospodu cez šev (epoxid), na rovnom stole.',
            'Tlač: líce dole na podložku, farba sa mení vo výške 0,4 a 0,6 mm (popisy).'])
s = 0.5
x = 24
for i, nm in enumerate(('p1', 'p2', 'p3')):
    b = V[nm]['top']['b']
    m = sh.view(nm, 'top', s, left=x, top=36, label='DIEL %d – PÔDORYS' % (i + 1), lw=0.2, hw=0.1, label_dy=10)
    sh.dimH(m, b[0], b[1], b[3], 8, num(b[1] - b[0], 1))
    sh.text((m.bb[0] + m.bb[2]) / 2, m.bb[3] + 15, 'X %s – %s' % (num(b[0], 1), num(b[1], 1)), 2.0, 'middle', NOTE)
    if i == 0: sh.dimV(m, b[2], b[3], b[0], -8, num(b[3] - b[2], 2))
    x = m.bb[2] + 10
for i, nm in enumerate(('sp1', 'sp2')):
    b = V[nm]['top']['b']
    m = sh.view(nm, 'top', s, left=x + 12 + i * 40, top=36, label='PÁS %d' % (i + 1), lw=0.25, label_dy=10)
    sh.dimH(m, b[0], b[1], b[3], 6, num(b[1] - b[0], 1))
    sh.dimV(m, b[2], b[3], b[1], 5, num(b[3] - b[2], 1))
sh.text(x + 12, 175, 'Pásy: hrúbka 1,5 mm, kreslené zhora,', 2.2, 'start', NOTE)
sh.text(x + 12, 178.6, 'v polohe pod švom (X ako na paneli).', 2.2, 'start', NOTE)
sheets.append(sh)

# ================================================================== 4. labels
sh = Sheet('Popisy (svetlá vrstva v gravírovaní)', 'CatBox3_panel_labels_full', '1:2', 'svetlý filament (1 vrstva)',
           ['Popisy nie sú samostatný diel na lepenie: je to vrstva 0,4 mm vo výške 2,6–3,0 panela, tlačí sa zmenou farby.',
            'Súbor slúži pre multi-materiál (MMU) alebo na kontrolu písma.'])
m = sh.view('labels', 'top', 0.5, left=40, top=40, label='PÔDORYS POPISOV', lw=0.2, label_dy=12)
b = V['labels']['top']['b']
sh.dimH(m, b[0], b[1], b[3], 8, num(b[1] - b[0], 1))
sh.dimV(m, b[2], b[3], b[1], 8, num(b[3] - b[2], 1))
sh.text(m.bb[0], m.bb[3] + 20, 'Poloha v súradniciach panela: X od %s, Y od %s; výška 2,6 – 3,0.' % (num(b[0], 1), num(b[2], 1)), 2.2, 'start', NOTE)
sheets.append(sh)

# ================================================================== 5. case overview
def body_list(name, plane='xy'):
    out = []
    for bd in cl[name]:
        if bd.get('plane', 'xy') != plane: continue
        for pg in bd['polys']:
            o = np.array(pg['outer']); hs = [np.array(h) for h in pg['holes']]
            out.append(dict(z0=bd['z0'], z1=bd['z1'], bb=(o.min(0), o.max(0)), holes=[(h.min(0), h.max(0)) for h in hs]))
    return out
sh = Sheet('Krabica – celok (3 diely spojené)', 'CatBox3_case_1 + _2 + _3', '1:2', 'PETG (Pi hreje) alebo PLA',
           ['Steny a dno 3 mm. Horná hrana stien = spodok panela: vpredu %s, vzadu %s.' % (num(FRONT - PANEL), num(BACK - PANEL)),
            'Diely sa spoja cez príruby 4 mm: 5× M3×12 + matica na každý šev. Porty vzadu sú vo výške 24 mm (stred).'])
s = 0.5
cf = sh.view('case', 'front', s, left=26, top=26, label='POHĽAD SPREDU', label_dy=17)
cp = sh.view('case', 'top', s, ox=cf.X(0), top=96, label='PÔDORYS', label_dy=9)
clf = sh.view('case', 'left', s, left=262, oy=cf.Y(0), label='BOKORYS ZĽAVA', label_dy=17)
cb = sh.view('case', 'back', s, left=26, top=232, label='POHĽAD ZOZADU (porty)', label_dy=7)
ci = sh.view('case', 'iso', 0.26, left=258, top=104, label='AXONOMETRIA', lw=0.2, label_dy=8)
sh.dimH(cf, 0, W, 0, -8)
sh.dimV(cf, 0, FRONT - PANEL, W, 6)
sh.dimH(cp, 0, 150.5, D, 8); sh.dimH(cp, 150.5, 270, D, 8); sh.dimH(cp, 270, W, D, 8)
sh.dimV(cp, 0, D, W, 8)
sh.dimV(clf, 0, BACK - PANEL, -D, -6)
sh.dimV(clf, 0, FRONT - PANEL, 0, 6)
sh.dimH(clf, -D, 0, 0, -8)
ports = [('usbc', 'USB-C 11,5×5'), ('power', 'vypínač Ø12,2'), ('usba', 'USB-A 15,2×7,8'), ('phones', 'slúch. Ø6,2'), ('outL', 'L Ø9,7'), ('outR', 'R Ø9,7')]
for i, (pid, txt) in enumerate(ports):
    p = layout[pid]; cxp = p['x'] + p['w'] / 2
    sh.lead(cb, -cxp, 24, 3 if i < 3 else -3, -9 - (i % 3) * 4.5, txt, color=ACC, size=2.0)
ux = layout['usbc']['x'] + layout['usbc']['w'] / 2
sh.dimV(cb, 0, 24, -ux, 0, '24')
sheets.append(sh)

# ================================================================== 6-8. case pieces
piece_x = {1: (0, 150.5), 2: (150.5, 270), 3: (270, W)}
for i in (1, 2, 3):
    name = 'CatBox3_case_%d' % i; a, bq = piece_x[i]; wpc = bq - a
    sh = Sheet('Krabica – diel %d' % i, name, '1:2 · axon. 1:2', 'PETG / PLA, 4 obvody, výplň 20 %',
               ['Tlač otvorenou stranou hore, bez podpier, vrstva 0,2 mm.',
                {1: 'Stĺpiky pre závitové vložky M3 (Ø4, 7 mm hlboko), 4 dištančné stĺpiky Pi 5 (M2,5, 58×49).',
                 2: 'Stredný diel: príruby na oboch stranách, porty slúchadlá + výstupy L/R vzadu.',
                 3: 'Stĺpiky pre závitové vložky M3 (Ø4, 7 mm hlboko).'}[i]])
    s = 0.5; J = 'c%d' % i
    m_fr = sh.view(J, 'front', s, left=34, top=28, label='POHĽAD SPREDU', label_dy=16)
    m_top = sh.view(J, 'top', s, ox=m_fr.X(0), top=100, label='PÔDORYS', label_dy=9)
    xl = m_fr.bb[2] + 34
    m_l = sh.view(J, 'left', s, left=xl, oy=m_fr.Y(0), label='BOKORYS ZĽAVA', label_dy=16)
    m_r = sh.view(J, 'right', s, left=xl, top=100, label='BOKORYS SPRAVA', label_dy=9)
    sh.view(J, 'iso', 0.5, left=xl + (20 if i == 2 else 0), top=150, label='AXONOMETRIA', lw=0.2, label_dy=7)
    sh.dimH(m_top, a, bq, D, 8, num(wpc, 1))
    sh.dimV(m_top, 0, D, a, -8)
    sh.dimV(m_fr, 0, FRONT - PANEL, bq, 6)
    sh.dimH(m_fr, a, bq, 0, -8, num(wpc, 1))
    sh.dimV(m_l, 0, BACK - PANEL, -D, -6)
    sh.dimH(m_l, -D, 0, 0, -8)
    done = set()
    for bd in body_list(name):
        (x0, y0_), (x1, y1) = bd['bb']
        if bd['z0'] < 0.5: continue
        cxp, cyp = (x0 + x1) / 2, (y0_ + y1) / 2; dia = x1 - x0
        if bd['z1'] < 9 and 'pi' not in done:
            sh.lead(m_top, cxp, cyp, 16, 14, 'Ø6,5 / Ø2,2, výška 5 (Pi)', r=dia / 2, color=ACC, size=2.0); done.add('pi')
        elif bd['z1'] >= 9 and bd['holes'] and 'post' not in done and cyp < 50:
            sh.lead(m_top, cxp, cyp, 16 if cxp < (a + bq) / 2 else -16, -16, 'stĺpik Ø9, vložka M3 Ø4', r=dia / 2, color=ACC, size=2.0); done.add('post')
    pis = [((bd['bb'][0][0] + bd['bb'][1][0]) / 2, (bd['bb'][0][1] + bd['bb'][1][1]) / 2) for bd in body_list(name) if 0.5 < bd['z0'] and bd['z1'] < 9]
    if pis:
        xs_ = sorted(set(round(p[0], 2) for p in pis)); ys_ = sorted(set(round(p[1], 2) for p in pis))
        sh.dimH(m_top, xs_[0], xs_[-1], ys_[-1], 6, num(xs_[-1] - xs_[0], 1))
        sh.dimV(m_top, ys_[0], ys_[-1], xs_[-1], 6, num(ys_[-1] - ys_[0], 1))
    if i in (1, 2): sh.lead(m_r, 40, 7, 6, -12, '5× Ø3,4 (M3×12 + matica)', r=1.75, color=ACC, size=2.0)
    if i in (2, 3): sh.lead(m_l, -40, 7, 6, -12, '5× Ø3,4 (M3×12 + matica)', r=1.75, color=ACC, size=2.0)
    if i == 1: sh.lead(m_l, -110, 14, 6, -14, '3× Ø3,4 bočnica (vrut 3×10)', r=1.75, color=ACC, size=2.0)
    if i == 3: sh.lead(m_r, 110, 14, 6, -12, '3× Ø3,4 bočnica (vrut 3×10)', r=1.75, color=ACC, size=2.0)
    sheets.append(sh)

# ================================================================== 9. knobs
sh = Sheet('Gombíky a krytka enkódera', 'knob_pot / knob_cutoff / cap_encoder', '5:1', PLA + ', vrstva 0,15',
           ['Kreslené ako namontované (vrch hore). Súbory STL sú otočené: vrch leží na podložke. Ryska navrchu 1,0 × 0,6 = ukazovateľ.',
            'D-otvor 6,15 s plôškou 4,6 (hriadeľ 6 mm D). Tesný = prevŕtaj vrtákom 6 mm, voľný = kvapka lepidla.'])
k = 5.0
specs = [('CatBox3_knob_pot_11mm', 11, 14, 9, True, 'POT 11 mm (27×)'), ('CatBox3_knob_cutoff_15mm', 15, 15, 9, True, 'CUTOFF 15 mm (1×)'), ('CatBox3_cap_encoder_14mm', 14, 16, 11, False, 'ENKÓDER 14 mm (12×)')]
for i, (nm, Dk, Hk, hd, skirt, lab) in enumerate(specs):
    cx = 72 + i * 126
    R = Dk / 2 + (1.6 if skirt else 0)
    sh.text(cx, 22, lab, 3.0, 'middle', ACC, 'bold', ls=0.4)
    mb = sh.view(nm, 'bottom', k, ox=cx, top=30, lw=0.26)
    mf = sh.view(nm, 'front', k, ox=cx, top=mb.bb[3] + 22, lw=0.28, hw=0.15)
    sh.text(cx, mb.bb[3] + 6, 'POHĽAD ZDOLA', 2.2, 'middle', DIM, 'bold')
    sh.text(cx, mf.bb[3] + 15, 'POHĽAD SPREDU', 2.2, 'middle', DIM, 'bold')
    sh.dimH(mf, -Dk / 2, Dk / 2, Hk, 6, 'Ø' + num(Dk))
    if skirt: sh.dimH(mf, -R, R, 0, -7, 'Ø' + num(Dk + 3.2))
    sh.dimV(mf, 0, Hk, R, 7, num(Hk))
    sh.dimV(mf, 0, hd, -R, -7, num(hd))
    if skirt: sh.dimV(mf, 0, 1.6, R, 16, '1,6')
    sh.centre(mb, 0, 0, R)
    sh.lead(mb, 0, 3.05, 18, -12, 'D Ø6,15 / 4,6', color=ACC, size=2.2)
sheets.append(sh)

# ================================================================== 10. keycap + slider cap
sh = Sheet('MX čiapočka a krytka slidera', 'CatBox3_keycap_mx / cap_slider', '5:1', 'priehľadný PETG (MX) · PLA',
           ['Kreslené ako namontované (vrch hore). MX krížik 4,15 × 1,35, hĺbka 4; stena 1,2; vrch 1,4.',
            'Krytka slidera: štrbina pre páčku 4,1 × 1,3, hĺbka 6.'])
k = 5.0
cx = 64
sh.text(cx, 22, 'MX ČIAPOČKA (26×)', 3.0, 'middle', ACC, 'bold', ls=0.4)
mb = sh.view('CatBox3_keycap_mx', 'bottom', k, ox=cx, top=30, lw=0.28)
mf = sh.view('CatBox3_keycap_mx', 'front', k, ox=cx, top=mb.bb[3] + 22, lw=0.28, hw=0.16)
ml = sh.view('CatBox3_keycap_mx', 'left', k, left=mf.bb[2] + 20, oy=mf.Y(0), lw=0.28, hw=0.16)
sh.text(cx, mb.bb[3] + 6, 'POHĽAD ZDOLA', 2.2, 'middle', DIM, 'bold')
sh.text(cx, mf.bb[3] + 8, 'POHĽAD SPREDU', 2.2, 'middle', DIM, 'bold')
sh.text((ml.bb[0] + ml.bb[2]) / 2, ml.bb[3] + 8, 'BOKORYS ZĽAVA', 2.2, 'middle', DIM, 'bold')
sh.dimH(mf, -8.8, 8.8, 7.2, 6, '17,6')
sh.dimV(mf, 0, 7.2, 8.8, 7, '7,2')
sh.dimV(mf, 1.8, 5.8, -8.8, -7, '4')
sh.dimV(mf, 5.8, 7.2, -8.8, -15, '1,4')
sh.dimH(mb, 7.6, 8.8, 8.8, 6, '1,2', tx=mb.X(8.8) + 5)
sh.lead(mb, 2.07, 0.3, 32, -22, 'krížik 4,15×1,35', color=ACC, size=2.2)
sh.lead(mb, 0, -2.75, 32, 22, 'stĺpik Ø5,5', color=ACC, size=2.2)
cx2 = 270
sh.text(cx2 + 40, 22, 'KRYTKA SLIDERA (1×)', 3.0, 'middle', ACC, 'bold', ls=0.4)
sb = sh.view('CatBox3_cap_slider', 'bottom', k, ox=cx2, top=30, lw=0.28)
sf = sh.view('CatBox3_cap_slider', 'front', k, ox=cx2, top=sb.bb[3] + 22, lw=0.28, hw=0.16)
sl = sh.view('CatBox3_cap_slider', 'left', k, left=sf.bb[2] + 18, oy=sf.Y(0), lw=0.28, hw=0.16)
sh.text(cx2, sb.bb[3] + 6, 'POHĽAD ZDOLA', 2.2, 'middle', DIM, 'bold')
sh.text(cx2, sf.bb[3] + 8, 'POHĽAD SPREDU', 2.2, 'middle', DIM, 'bold')
sh.text((sl.bb[0] + sl.bb[2]) / 2, sl.bb[3] + 8, 'BOKORYS ZĽAVA', 2.2, 'middle', DIM, 'bold')
sh.dimH(sf, -6, 6, 8, 6, '12')
sh.dimV(sf, 0, 8, 6, 7, '8')
sh.dimV(sf, 0, 6, -6, -7, '6')
sh.dimH(sl, -9, 9, 8, 6, '18')
sh.lead(sb, 0.65, 0, 26, -16, 'štrbina 4,1×1,3', color=ACC, size=2.2)
sheets.append(sh)

# ------------------------------------------------------------------ output
total = len(sheets)
svgs = [x.svg(i + 1, total) for i, x in enumerate(sheets)]
for i, sv in enumerate(svgs): open(B + '/sheet_%02d.svg' % (i + 1), 'w').write(sv)
html = ('<!doctype html><html><head><meta charset="utf-8"><title>Cat Box 3 – blueprinty</title><style>@page{size:420mm 297mm;margin:0}'
        'html,body{margin:0;background:#0b2242}.pg{width:420mm;height:297mm;page-break-after:always;break-after:page;overflow:hidden}.pg svg{display:block}</style></head><body>'
        + ''.join('<div class="pg">%s</div>' % sv for sv in svgs) + '</body></html>')
open(B + '/sheets.html', 'w').write(html)
print('sheets', total, 'holes', len(holes))
